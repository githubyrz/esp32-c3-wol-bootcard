"""Explicitly authorized live test. Subscribes only; no WOL or MQTT publish.

Credentials: ignored .native/bemfa-live.json {uid, topic}. Keep only redacted
results in docs/evidence. Send off/on from the web console or Android app.
This is a Python protocol probe, not the ESP32 firmware.
"""
from pathlib import Path
from datetime import datetime
import argparse
import json
import re
import select
import socket
import ssl
import time

ROOT = Path(__file__).resolve().parents[1]
REPORT = ROOT / 'docs/evidence/live-mqtt.json'
STARTED = time.monotonic()
report = {'executed_at': datetime.now().astimezone().isoformat(),
          'client': 'Python stdlib MQTT 3.1.1 probe', 'hardware_tested': False,
          'wol_sent': False, 'result': 'FAIL', 'events': []}


def record(event, **values):
    entry = {'event': event, 'elapsed_seconds': round(time.monotonic()-STARTED, 3), **values}
    report['events'].append(entry)
    print(json.dumps(entry), flush=True)
    REPORT.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')


def string(value):
    raw = value.encode()
    return len(raw).to_bytes(2, 'big') + raw


class Probe:
    def __init__(self, ctx, uid, topic, session):
        self.sock = ctx.wrap_socket(socket.create_connection(('bemfa.com', 9503), 10),
                                    server_hostname='bemfa.com')
        self.sock.settimeout(5)
        self.buffer = bytearray()
        self.topic = topic.encode()
        self.session = session
        self.online = False
        self.off_on = []
        self.pongs = 0
        record('tls_verified', session=session, protocol=self.sock.version(), cipher=self.sock.cipher()[0])
        self.send(0x10, b'\x00\x04MQTT\x04\x02\x00\x1e' + string(uid))
        h, b = self.wait(2, 10)
        record('connack', session=session, response=list(b) if len(b)==2 else 'invalid_length')
        if len(b) != 2 or b[1] != 0:
            raise RuntimeError('CONNACK rejected')
        record('authenticated', session=session, requested_clean_session=True,
               server_session_present=bool(b[0] & 1))
        self.send(0x82, b'\x00\x01' + string(topic) + b'\x00')
        h, b = self.wait(9, 10)
        if b != b'\x00\x01\x00':
            raise RuntimeError('SUBACK did not grant QoS0')
        self.online = True
        record('subscribed', session=session, granted_qos=0)

    def send(self, header, body=b''):
        length = len(body)
        encoded = bytearray([header])
        while True:
            digit = length % 128
            length //= 128
            encoded.append(digit | (128 if length else 0))
            if not length:
                break
        self.sock.sendall(encoded + body)

    def frame(self):
        if len(self.buffer) >= 2:
            length, multiplier = 0, 1
            for index in range(1, min(5, len(self.buffer))):
                digit = self.buffer[index]
                length += (digit & 127) * multiplier
                if length > 4096:
                    raise RuntimeError('Frame exceeds probe bound')
                if not digit & 128:
                    end = index + 1 + length
                    if len(self.buffer) >= end:
                        result = self.buffer[0], bytes(self.buffer[index + 1:end])
                        del self.buffer[:end]
                        return result
                    return None
                multiplier *= 128
                if index == 4:
                    raise RuntimeError('Invalid remaining length')
        return None

    def read(self, timeout=0.5):
        frame = self.frame()
        if frame:
            return frame
        if self.sock.pending() or select.select([self.sock], [], [], timeout)[0]:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise RuntimeError('Server disconnected')
            self.buffer.extend(chunk)
        return self.frame()

    def incoming(self, header, body):
        if header >> 4 == 3:
            if len(body) < 2:
                raise RuntimeError('Short PUBLISH')
            size = int.from_bytes(body[:2], 'big')
            if len(body) < 2 + size:
                raise RuntimeError('Short topic')
            topic, payload = body[2:2 + size], body[2 + size:]
            qos = (header >> 1) & 3
            if qos != 0:
                raise RuntimeError('Unexpected PUBLISH QoS')
            command = payload.decode('ascii') if payload in (b'on', b'off') else 'other'
            record('message', session=self.session, after_suback=self.online,
                   topic_matches=topic == self.topic, command=command, bytes=len(payload),
                   retain=bool(header & 1), dup=bool(header & 8), qos=qos)
            if self.online and topic == self.topic and not header & 9:
                self.off_on.append(command)
        elif header == 0xd0 and not body:
            self.pongs += 1
            record('ping_response', session=self.session)

    def wait(self, packet_type, seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            item = self.read()
            if item:
                if item[0] >> 4 == packet_type:
                    return item
                self.incoming(*item)
        raise RuntimeError('Expected MQTT response timed out')

    def close(self):
        try:
            self.send(0xe0)
        finally:
            self.sock.close()


def main():
    global REPORT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', choices=('console', 'app'), default='console')
    args = parser.parse_args()
    report['control_source'] = args.source
    if args.source == 'app':
        REPORT = ROOT / 'docs/evidence/app-live-mqtt.json'
    credentials = json.loads((ROOT / '.native/bemfa-live.json').read_text(encoding='utf-8'))
    if not re.fullmatch(r'[0-9a-fA-F]{32}', credentials['uid']):
        raise RuntimeError('Invalid local UID format')
    pem = re.search(r'-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----',
                    (ROOT / 'include/BemfaRootCa.h').read_text(encoding='utf-8'), re.S).group(0)
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    ctx.minimum_version = ctx.maximum_version = ssl.TLSVersion.TLSv1_2
    ctx.load_verify_locations(cadata=pem)
    ctx.set_ciphers('ECDHE-RSA-AES128-GCM-SHA256')
    ctx.set_ecdh_curve('prime256v1')
    probe = Probe(ctx, **credentials, session=1)
    try:
        started = last_ping = time.monotonic()
        record('awaiting_'+args.source+'_off_on', maximum_seconds=240)
        while time.monotonic() - started < 240:
            if time.monotonic() - last_ping >= 20:
                probe.send(0xc0)
                last_ping = time.monotonic()
            item = probe.read()
            if item:
                probe.incoming(*item)
            if probe.off_on[-2:] == ['off', 'on'] and probe.pongs:
                break
        else:
            raise RuntimeError('Controller off/on or heartbeat not observed before deadline')
        record(args.source+'_off_on_verified')
    finally:
        probe.close()
    probe = Probe(ctx, **credentials, session=2)
    try:
        probe.send(0xc0)
        probe.wait(13, 10)
        record('reconnect_and_heartbeat_verified')
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            item = probe.read()
            if item:
                probe.incoming(*item)
    finally:
        probe.close()
    report['result'] = 'PASS'
    record('complete', connection_closed=True)


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        # Do not render arbitrary exception values or credential-bearing frames.
        record('failed', error_type=type(error).__name__,
               reason=str(error) if type(error) is RuntimeError else 'details suppressed')
        raise SystemExit(1)
