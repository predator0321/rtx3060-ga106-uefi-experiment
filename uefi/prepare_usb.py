#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Create files in an ordinary directory; never format USB or edit boot settings."""
import argparse
from contextlib import contextmanager
import csv
import hashlib
import io
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'firmware'))
import extract_boot_assets as boot
import fetch_artifacts as fetch
import generate_v67 as v67


@contextmanager
def package_tar(path):
    # Python 3.14 has native zstd tar support. On 3.12/3.13, use zstandard.
    try:
        archive = tarfile.open(path, 'r:*')
    except tarfile.ReadError:
        try:
            import zstandard
        except ImportError:
            raise RuntimeError('Python <3.14 needs: python -m pip install zstandard') from None
        with path.open('rb') as source:
            with zstandard.ZstdDecompressor().stream_reader(source) as stream:
                with tarfile.open(fileobj=stream, mode='r|') as archive:
                    yield archive
    else:
        with archive:
            yield archive


def detect_card():
    command = ['nvidia-smi', '--query-gpu=pci.bus_id,pci.device_id,pci.sub_device_id,memory.total', '--format=csv,noheader,nounits']
    result = subprocess.run(command, capture_output=True, text=True, timeout=15, check=True)
    matches = []
    for row in csv.reader(io.StringIO(result.stdout)):
        if len(row) != 4:
            continue
        bdf, identity, subsystem, memory = [v.strip() for v in row]
        if int(identity, 16) != 0x250410de or int(memory) != 12288:
            continue
        # NVIDIA sometimes prints an eight-digit segment; EFI stores 16 bits.
        match = re.fullmatch(r'([0-9a-fA-F]{4,8}):([0-9a-fA-F]{2}):([0-9a-fA-F]{2})\.([0-7])', bdf)
        if not match:
            raise ValueError('Unexpected NVIDIA PCI BDF')
        segment, bus, device, function = [int(v, 16) for v in match.groups()]
        if segment > 0xffff or device > 31:
            raise ValueError('PCI address outside supported range')
        sub = int(subsystem, 16)
        subvendor, subdevice = sub & 0xffff, sub >> 16
        if not 0 < subvendor < 0xffff or not 0 < subdevice < 0xffff:
            raise ValueError('Invalid subsystem ID reported by NVIDIA')
        matches.append((f'{segment:04x}:{bus:02x}:{device:02x}.{function}', subvendor, subdevice))
    if len(matches) != 1:
        raise ValueError(f'Expected exactly one 10de:2504 12GB GPU; found {len(matches)}')
    return matches[0]


def firmware(cache, output):
    manifest = json.loads((ROOT / 'firmware/artifacts-610.43.03.json').read_text())
    source_entry, package_entry = fetch.inputs(manifest)
    source = fetch.fetch(source_entry, cache)
    package = fetch.fetch(package_entry, cache)
    print('Pinned input checksums verified.', flush=True)
    notice = package_entry['notice']
    wanted = {'.PKGINFO', v67.FIRMWARE_PREFIX + 'gsp_ga10x.bin', notice['path']}
    members = {}
    with package_tar(package) as archive:
        for item in archive:
            if item.name not in wanted:
                continue
            if item.name in members or not item.isfile() or item.size > 128 * 1024**2:
                raise ValueError('Invalid/duplicate package member')
            stream = archive.extractfile(item)
            if stream is None:
                raise ValueError('Missing regular file data')
            members[item.name] = stream.read()
    if set(members) != wanted:
        raise ValueError('Pinned package is missing required files')
    if f'pkgver = {v67.PACKAGE_VERSION}' not in members['.PKGINFO'].decode().splitlines():
        raise ValueError('Package version mismatch')
    gsp = members[v67.FIRMWARE_PREFIX + 'gsp_ga10x.bin']
    payload = v67.build_payload()
    license_bytes = members[notice['path']]
    for data, expected in [(gsp, v67.GSP_SHA256), (payload, v67.PAYLOAD_SHA256), (license_bytes, notice['sha256'])]:
        if hashlib.sha256(data).hexdigest() != expected:
            raise ValueError('Extracted asset checksum mismatch')
    with tempfile.TemporaryDirectory() as directory:
        tree = Path(directory)
        prefix = 'NVIDIA-kernel-module-source-610.43.03/src/nvidia/generated/'
        with tarfile.open(source, 'r:xz') as archive:
            for name in boot.PINNED:
                item = archive.getmember(prefix + name)
                if not item.isfile() or item.size > 32 * 1024**2:
                    raise ValueError('Invalid generated source member')
                target = tree / 'src/nvidia/generated' / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(archive.extractfile(item).read())
        boot.extract_assets(tree, output)
    (output / 'gsp_ga10x.bin').write_bytes(gsp)
    (output / 'v67-ga102-610.43.03.bin').write_bytes(payload)
    (output / 'NVIDIA-LICENSE').write_bytes(license_bytes)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('USB_READY'))
    parser.add_argument('--cache', type=Path, default=ROOT / 'audit-inputs')
    parser.add_argument('--mode', choices=('probe', 'unlock'), default='probe')
    parser.add_argument('--execute', action='store_true', help='Explicitly create execute=1 config and include firmware')
    parser.add_argument('--ss1-full', action='store_true', help='Optional SS1=8 experiment; default preserves SS1')
    args = parser.parse_args()
    if args.execute and args.mode != 'unlock':
        parser.error('--execute requires --mode unlock')
    if args.ss1_full and not args.execute:
        parser.error('--ss1-full requires --execute')
    # Never overwrite an existing destination, let alone a mounted USB root.
    if args.output.exists():
        parser.error('--output must be a new ordinary folder; existing paths are refused')
    prebuilt = ROOT / 'uefi/prebuilt'
    manifest = json.loads((prebuilt / 'manifest.json').read_text())
    name = 'GA106-PROBE.efi' if args.mode == 'probe' else 'GA106-UNLOCK.efi'
    binary = prebuilt / name
    if hashlib.sha256(binary.read_bytes()).hexdigest() != manifest['sha256'][name]:
        raise ValueError('Prebuilt EFI checksum mismatch')
    card = None
    try:
        card = detect_card()
    except (OSError, ValueError, subprocess.SubprocessError):
        if args.execute:
            raise RuntimeError('Could not uniquely detect the RTX 3060 board via nvidia-smi. No execute folder was created.') from None
    # Stage everything before exposing a reviewable output directory.
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='ga106-stage-', dir=args.output.parent) as directory:
        stage = Path(directory) / 'payload'
        bootdir = stage / 'EFI/BOOT'
        settings = stage / 'ga106'
        bootdir.mkdir(parents=True)
        settings.mkdir()
        shutil.copy2(binary, bootdir / 'BOOTX64.EFI')
        config = '# GA106 experimental EFI; remove USB to use normal Windows boot\n'
        if card:
            target, vendor, device = card
            config += f'target={target}\nboard_vendor=0x{vendor:04x}\nboard_device=0x{device:04x}\n'
        config += f'execute={int(args.execute)}\nss1_full={int(args.ss1_full)}\nwindows=1\n'
        (settings / 'config.txt').write_text(config, encoding='ascii')
        if args.execute:
            firmware(args.cache, settings / 'firmware')
        shutil.copy2(ROOT / 'uefi/README.md', stage / 'README.md')
        shutil.copy2(ROOT / 'licenses/GPL-2.0.txt', settings / 'GPL-2.0.txt')
        shutil.copy2(ROOT / 'uefi/GNU-EFI-NOTICE.txt', settings / 'GNU-EFI-NOTICE.txt')
        stage.rename(args.output)
    print(f'Created {args.output.resolve()}')
    print('No drive was formatted and no Windows/UEFI settings were changed.')
    print('Mode: ' + ('EXPERIMENTAL EXECUTE; physical GPU validation pending' if args.execute else 'read-only PCI probe; no GPU firmware execution'))


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError, tarfile.TarError) as error:
        raise SystemExit(str(error)) from None
