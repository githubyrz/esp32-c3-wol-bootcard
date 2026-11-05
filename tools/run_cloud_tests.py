"""Run production main/CloudClient + pinned upstream MQTT parser on Windows.

Only socket, Wi-Fi, UART, clock and FreeRTOS interfaces are simulated. MSVC
requires host-only spelling changes for four GCC VLAs and one packed struct.
The downloaded dependency and production firmware source are never patched.
"""
from pathlib import Path
from datetime import datetime
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SCENARIOS = '''happy session_present auth_rejected suback_rejected suback_qos1 connect_timeout
tls_failure_no_downgrade wifi_offline pre_suback_history guard retained dup
fragmented coalesced queue_overflow stale_queue disconnect_queued cooldown
heartbeat_timeout partial_payload invalid_topic empty oversized qos1 reconnect_cycles'''.split()

def main():
    sys.stdout.reconfigure(encoding='utf-8')
    out=ROOT/'.native/cloud';out.mkdir(parents=True,exist_ok=True)
    source=ROOT/'.pio/libdeps/esp32c3_supermini/ArduinoMqttClient/src/MqttClient.cpp'
    if not source.is_file():
        raise RuntimeError('Missing ArduinoMqttClient 0.1.8: run PlatformIO for esp32c3_supermini first.')
    raw=source.read_bytes()
    if hashlib.sha256(raw).hexdigest()!='f31dd7e9be6ca787d2395dca0fc74f56b6e996355e778222b3499a5ffcf8f791':
        raise RuntimeError('Pinned upstream source changed; review host adaptation before running')
    cpp=raw.decode()
    cpp,count=re.subn(r'uint8_t (packetBuffer|packetHeaderBuffer)\[5 \+ (remainingLength|headerLength)\];',
        r'std::vector<uint8_t> hostStorage(5 + \2); uint8_t* \1=hostStorage.data();',cpp)
    if count!=4 or cpp.count('struct __attribute__ ((packed)) {')!=1:
        raise RuntimeError('Unexpected upstream layout; refusing host adaptation')
    cpp=cpp.replace('struct __attribute__ ((packed)) {','#pragma pack(push, 1)\n  struct {')
    cpp=cpp.replace('} connectVariableHeader;', '} connectVariableHeader;\n#pragma pack(pop)\nstatic_assert(sizeof(connectVariableHeader)==10, "MQTT CONNECT header layout");')
    (out/'MqttClient.cpp').write_text(cpp,encoding='utf-8')
    for name in ('MqttClient.h','ArduinoMqttClient.h'):
        shutil.copyfile(source.parent/name,out/name)
    vswhere=Path(os.environ.get('ProgramFiles(x86)',r'C:\Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
    install=subprocess.check_output([str(vswhere),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
    vcvars=Path(install)/'VC/Auxiliary/Build/vcvars64.bat'
    p=subprocess.run(f'call "{vcvars}" >nul && set',shell=True,executable=os.environ.get('COMSPEC','cmd.exe'),capture_output=True,text=True,check=True)
    env=dict(os.environ)
    for line in p.stdout.splitlines():
        if '=' in line and not line.startswith('='):
            k,v=line.split('=',1);env[k.upper()]=v
    cl=shutil.which('cl.exe',path=env['PATH'])
    results=[]
    report={'executed_at':datetime.now().astimezone().isoformat(), 'hardware_tested':False,
            'cloud_account_tested':False, 'transport':'in-memory', 'result':'FAIL', 'scenarios':results}
    log=['Executed at: '+report['executed_at']+'\n',
         'Production MQTT integration, deterministic in-memory transport; no cloud account or LAN traffic.\n',
         'Upstream ArduinoMqttClient 0.1.8 SHA256: '+hashlib.sha256(raw).hexdigest()+'\n',
         'Host-only syntax adaptation: 4 VLAs -> vectors; 1 GCC packed struct -> MSVC pack; parser logic unchanged.\n']
    def run(args,timeout=30):
        p=subprocess.run(args,cwd=out,env=env,capture_output=True,timeout=timeout)
        raw=p.stdout+p.stderr
        try:t=raw.decode('utf-8')
        except UnicodeDecodeError:t=raw.decode('gb18030')
        t=t.replace('\r\n','\n');print(t,end='');log.append(t)
        if p.returncode:raise RuntimeError('Command failed: '+str(p.returncode))
        return t
    try:
        includes=[ROOT/'test/cloud_fakes',ROOT/'test/fakes',out,ROOT/'include',ROOT/'lib/WolCore/src']
        sources=[ROOT/'test/test_cloud.cpp',ROOT/'src/CloudClient.cpp',ROOT/'src/WolManager.cpp',ROOT/'src/WolWifiManager.cpp',ROOT/'lib/WolCore/src/WolCore.cpp',out/'MqttClient.cpp']
        # Upstream Arduino types intentionally use narrowing conversions, so its
        # host compatibility build disables those diagnostics; firmware is built separately.
        run([cl,'/nologo','/std:c++17','/EHsc','/W3','/wd4267','/wd4244','/utf-8','/D_CRT_SECURE_NO_WARNINGS','/DWOL_HOST_TEST',
             *['/I'+str(x) for x in includes],*[str(x) for x in sources],'/Fe:'+str(out/'cloud_tests.exe')])
        for scenario in SCENARIOS:
            output=run([str(out/'cloud_tests.exe'),scenario])
            match=re.search(r'PASS '+re.escape(scenario)+r' \((\d+) checks\)',output)
            if not match:raise RuntimeError('Missing scenario result: '+scenario)
            results.append({'name':scenario, 'checks':int(match.group(1)), 'result':'PASS'})
        total=sum(item['checks'] for item in results)
        report.update(result='PASS',checks=total,reconnect_cycles=1000)
        summary=f'RESULT: {len(SCENARIOS)} integration scenarios, {total} checks passed; includes 1000 reconnect/wake cycles.\n'
        print(summary,end='');log.append(summary)
    finally:
        (ROOT/'docs/evidence/cloud-integration.log').write_text(''.join(log),encoding='utf-8')
        (ROOT/'docs/evidence/cloud-summary.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return 0
if __name__=='__main__':sys.exit(main())
