"""No credentials/MQTT traffic: verify TLS1.2 using the firmware's exact root."""
import argparse
from pathlib import Path
import re
import socket
import ssl
import sys

ROOT = Path(__file__).resolve().parents[1]

def main():
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8')
    parser = argparse.ArgumentParser()
    parser.add_argument('--proxy', help='Optional local HTTP CONNECT proxy, e.g. 127.0.0.1:7897')
    args = parser.parse_args()
    header = (ROOT / 'include/BemfaRootCa.h').read_text(encoding='utf-8')
    pem = re.search(r'-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----', header, re.S).group(0)
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    ctx.minimum_version = ssl.TLSVersion.TLSv1_2
    ctx.maximum_version = ssl.TLSVersion.TLSv1_2
    ctx.load_verify_locations(cadata=pem)
    ctx.set_ciphers('ECDHE-RSA-AES128-GCM-SHA256')
    ctx.set_ecdh_curve('prime256v1')
    if args.proxy:
        host, port = args.proxy.rsplit(':', 1)
        raw = socket.create_connection((host, int(port)), timeout=15)
        raw.sendall(b'CONNECT bemfa.com:9503 HTTP/1.1\r\nHost: bemfa.com:9503\r\n\r\n')
        response = bytearray()
        while not response.endswith(b'\r\n\r\n'):
            b = raw.recv(1)
            if not b or len(response) > 8192:
                raw.close()
                raise RuntimeError('Invalid HTTP CONNECT response')
            response.extend(b)
        if response.split(b'\r\n', 1)[0].split()[1] != b'200':
            raw.close()
            raise RuntimeError('HTTP CONNECT was not accepted')
    else:
        raw = socket.create_connection(('bemfa.com', 9503), timeout=15)
    with raw:
        with ctx.wrap_socket(raw, server_hostname='bemfa.com') as tls:
            cert = tls.getpeercert()
            lines = [
                'TLS probe, no credentials or MQTT CONNECT sent.',
                'Host and SNI: bemfa.com; port: 9503',
                'Trust: exact public Certum root embedded in firmware (no other roots).',
                'Certificate and hostname verification: OK',
                'Protocol: ' + tls.version(),
                'Cipher: ' + tls.cipher()[0],
                'ECDH curve constrained to prime256v1 / P-256.',
                'Peer subject: ' + repr(cert.get('subject')),
                'Peer issuer: ' + repr(cert.get('issuer')),
                'Peer notBefore: ' + cert['notBefore'],
                'Peer notAfter: ' + cert['notAfter'],
                'Peer SAN: ' + repr(cert.get('subjectAltName')),
                'Result: PASS; this does not verify ESP32 or MQTT authentication.',
            ]
    result = '\n'.join(lines) + '\n'
    print(result, end='')
    (ROOT / 'docs/evidence/tls-firmware-root.txt').write_text(result, encoding='utf-8')
    return 0

if __name__ == '__main__':
    sys.exit(main())
