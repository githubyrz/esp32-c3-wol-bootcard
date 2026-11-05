"""Compile and execute production C++ on Windows using installed MSVC; no downloads."""
from pathlib import Path
import json
import os
import shutil
import socket
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / '.native'
EVIDENCE = ROOT / 'docs' / 'evidence'

def main():
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8')
    OUT.mkdir(exist_ok=True)
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    vswhere = Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    install = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
    vcvars = Path(install) / 'VC/Auxiliary/Build/vcvars64.bat'
    # Only capture environment, never print it (it may include unrelated secrets).
    shell = subprocess.run(f'call "{vcvars}" >nul && set', shell=True, executable=os.environ.get('COMSPEC', 'cmd.exe'), capture_output=True, text=True, check=True)
    env = dict(os.environ)
    env['VSLANG'] = '1033'
    for line in shell.stdout.splitlines():
        if '=' in line and not line.startswith('='):
            k, v = line.split('=', 1)
            env[k.upper()] = v
    command = ['cl.exe', '/nologo', '/std:c++17', '/EHsc', '/W4', '/WX', '/utf-8', '/D_CRT_SECURE_NO_WARNINGS',
               '/I' + str(ROOT / 'test/fakes'), '/I' + str(ROOT / 'include'), '/I' + str(ROOT / 'lib/WolCore/src'),
               str(ROOT / 'test/test_core.cpp'), str(ROOT / 'lib/WolCore/src/WolCore.cpp'), str(ROOT / 'src/WolManager.cpp'),
               '/Fe:' + str(OUT / 'wol_tests.exe')]
    log = []
    def run(args, cwd=OUT):
        p = subprocess.run(args, cwd=cwd, env=env, capture_output=True)
        raw = p.stdout + p.stderr
        try:
            text = raw.decode('utf-8')
        except UnicodeDecodeError:
            text = raw.decode('gb18030', errors='replace')
        text = text.replace('\r\n', '\n').replace('\r', '\n')
        print(text, end=''); log.append(text)
        if p.returncode:
            (EVIDENCE / 'native-tests.log').write_text(''.join(log), encoding='utf-8')
            raise RuntimeError(f'Command failed with exit {p.returncode}')
    # MSVC is not necessarily on PATH outside vcvars.
    command[0] = shutil.which('cl.exe', path=env.get('PATH')) or 'cl.exe'
    run(command)
    run([str(OUT / 'wol_tests.exe')])
    # Compile the actual AppConfig fallback with config.h absent, then with the
    # literal placeholder file present. Never read the user's real local config.
    for mode in ('missing', 'placeholder'):
        folder = OUT / ('config_' + mode)
        folder.mkdir(exist_ok=True)
        for name in ('AppConfig.h', 'config.example.h', 'BemfaRootCa.h'):
            shutil.copyfile(ROOT / 'include' / name, folder / name)
        if mode == 'placeholder':
            shutil.copyfile(ROOT / 'include/config.example.h', folder / 'config.h')
        source = folder / 'check.cpp'
        source.write_text('#include "AppConfig.h"\nint main() { return wol::validate(wol::kSettings) ? 0 : 1; }\n', encoding='utf-8')
        executable = folder / 'config_check.exe'
        run([command[0], '/nologo', '/std:c++17', '/EHsc', '/W4', '/WX', '/utf-8',
             '/I' + str(folder), '/I' + str(ROOT / 'lib/WolCore/src'), str(source),
             str(ROOT / 'lib/WolCore/src/WolCore.cpp'), '/Fe:' + str(executable)], cwd=folder)
        run([str(executable)])
        result = f'PASS actual AppConfig {mode} file: safe validation failure before network startup\n'
        print(result, end=''); log.append(result)
    packet_path = OUT / 'packet.bin'
    run([str(OUT / 'wol_tests.exe'), '--packet', str(packet_path)])
    data = packet_path.read_bytes()
    assert len(data) == 102
    assert data == b'\xff' * 6 + bytes.fromhex('021122334455') * 16
    # Loopback only: this cannot wake any physical device.
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as rx, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as tx:
        rx.bind(('127.0.0.1', 0)); rx.settimeout(2)
        assert tx.sendto(data, rx.getsockname()) == 102
        received, _ = rx.recvfrom(1024)
        assert received == data
    result = 'PASS Windows UDP loopback: 102 bytes from production C++ builder; no LAN broadcast\n'
    print(result, end=''); log.append(result)
    (EVIDENCE / 'native-tests.log').write_text(''.join(log), encoding='utf-8')
    (EVIDENCE / 'test-summary.json').write_text(json.dumps({'native_exit_code': 0, 'config_missing_and_placeholder': 'passed', 'loopback_udp_bytes': len(data), 'hardware_tested': False}, indent=2) + '\n', encoding='utf-8')
    return 0

if __name__ == '__main__':
    sys.exit(main())
