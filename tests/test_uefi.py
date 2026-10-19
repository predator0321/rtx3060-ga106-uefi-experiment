"""Host tests for EFI policy, binary parsing, and independent SHA-256 pins."""
import ctypes
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import uuid

ROOT = Path(__file__).resolve().parents[1]


class Options(ctypes.Structure):
    _fields_ = [(name, ctypes.c_uint) for name in
                ('segment', 'bus', 'device', 'function', 'board_vendor', 'board_device')]
    _fields_ += [(name, ctypes.c_bool) for name in ('has_target', 'execute', 'ss1_full', 'windows')]
    _fields_ += [('has_windows_partition', ctypes.c_bool), ('windows_partition', ctypes.c_uint8 * 16)]


class EfiPolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        path = Path(cls.temporary.name) / 'efi-policy.so'
        subprocess.run(['cc', '-std=c11', '-shared', '-fPIC', '-Wall', '-Wextra',
                        '-Werror', '-Wno-misleading-indentation',
                        str(ROOT / 'uefi/policy.c'), str(ROOT / 'uefi/hash.c'),
                        '-o', str(path)], check=True)
        cls.lib = ctypes.CDLL(str(path))
        cls.lib.gu_sha256.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p]
        cls.lib.gu_verify_asset.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_size_t]
        cls.lib.gu_parse_options.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.POINTER(Options)]
        cls.lib.gu_capability_offset.argtypes = [ctypes.c_void_p, ctypes.c_uint]
        cls.lib.gu_bar_resource_length.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
        cls.lib.gu_bar_resource_length.restype = ctypes.c_uint64
        cls.lib.gu_partition_matches.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p]

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def test_sha256_known_vectors_and_padding_boundaries(self):
        for data in [b'', b'abc', b'a' * 1000000] + [bytes(range(n)) for n in (1, 55, 56, 63, 64, 65, 127, 128, 255)]:
            out = ctypes.create_string_buffer(32)
            self.lib.gu_sha256(data, len(data), out)
            self.assertEqual(out.raw, hashlib.sha256(data).digest())

    def test_runtime_asset_pins_accept_original_and_reject_corruption(self):
        assets = ROOT / 'build/firmware'
        if not (assets / 'gsp_ga10x.bin').is_file():
            self.skipTest('Pinned firmware not prepared')
        pins = json.loads((ROOT / 'build/offline-audit.json').read_text())['output_sha256']
        for name in [n for n in pins if n.endswith('.bin')]:
            data = (assets / name).read_bytes()
            self.assertEqual(self.lib.gu_verify_asset(name.encode(), data, len(data)), 1)
            corrupt = bytearray(data)
            corrupt[len(corrupt) // 2] ^= 1
            self.assertEqual(self.lib.gu_verify_asset(name.encode(), bytes(corrupt), len(corrupt)), 0)
        self.assertEqual(self.lib.gu_verify_asset(b'other.bin', b'abc', 3), 0)

    def test_execute_requires_exact_target_board_and_strict_single_values(self):
        valid = b'target=0000:21:00.0\r\nboard_vendor=0x1462\r\nboard_device=0x397d\r\nexecute=1\r\nss1_full=0\r\nwindows=1\r\n'
        result = Options()
        self.assertEqual(self.lib.gu_parse_options(valid, len(valid), ctypes.byref(result)), 0)
        self.assertEqual((result.segment, result.bus, result.device, result.function), (0, 0x21, 0, 0))
        self.assertTrue(result.execute)
        self.assertFalse(result.ss1_full)
        for bad in (b'execute=1\n', valid + b'execute=0\n', valid + b'unknown=1\n',
                    valid.replace(b'0x1462', b'0xffff'), valid.replace(b'0x397d', b'0'),
                    valid.replace(b'21:00.0', b'21:20.0'), valid.replace(b'21:00.0', b'21:00.8'),
                    valid.replace(b'0000:21:00.0', b'all'), valid.replace(b'execute=1', b'execute=2'),
                    valid + b'\0', b'x' * 4097):
            self.assertNotEqual(self.lib.gu_parse_options(bad, len(bad), ctypes.byref(result)), 0, bad[:100])
        self.assertEqual(self.lib.gu_parse_options(b'# default\nexecute=0\n', 20, ctypes.byref(result)), 0)
        self.assertFalse(result.execute)

    def test_capability_loop_and_bar_descriptor_bounds(self):
        config = bytearray(256)
        config[6] = 0x10
        config[0x34] = 0x40
        config[0x40:0x42] = b'\x01\x50'
        config[0x50:0x52] = b'\x10\x00'
        self.assertEqual(self.lib.gu_capability_offset(bytes(config), 0x10), 0x50)
        config[0x51] = 0x40
        self.assertEqual(self.lib.gu_capability_offset(bytes(config), 0x11), -1)
        config[0x34] = 0x41
        self.assertEqual(self.lib.gu_capability_offset(bytes(config), 0x10), -1)
        bar = bytearray(46)
        bar[:4] = b'\x8a\x2b\0\0'
        bar[38:46] = (16 * 1024**2).to_bytes(8, 'little')
        self.assertEqual(self.lib.gu_bar_resource_length(bytes(bar), 46), 16 * 1024**2)
        self.assertEqual(self.lib.gu_bar_resource_length(bytes(bar), 45), 0)
        bar[3] = 1
        self.assertEqual(self.lib.gu_bar_resource_length(bytes(bar), 46), 0)


    def test_windows_partition_guid_and_device_paths(self):
        guid = uuid.UUID('01234567-89ab-cdef-8123-456789abcdef')
        result = Options()
        config = ('windows_partition=' + str(guid) + '\n').encode()
        self.assertEqual(self.lib.gu_parse_options(config, len(config), ctypes.byref(result)), 0)
        self.assertTrue(result.has_windows_partition)
        self.assertEqual(bytes(result.windows_partition), guid.bytes_le)
        self.assertTrue(result.windows)
        for bad in (config * 2, b'windows_partition=00000000-0000-0000-0000-000000000000',
                    config.replace(b'89ab', b'89az'), config.replace(b'-', b':', 1),
                    b'windows_partition={' + str(guid).encode() + b'}'):
            self.assertNotEqual(self.lib.gu_parse_options(bad, len(bad), ctypes.byref(result)), 0)
        hd = bytearray(42)
        hd[:4] = bytes([4, 1, 42, 0])
        hd[24:40] = guid.bytes_le
        hd[40:42] = bytes([2, 2])
        end = bytes([0x7f, 0xff, 4, 0])
        path = bytes(hd) + end
        match = lambda data: self.lib.gu_partition_matches(data, len(data), guid.bytes_le)
        self.assertEqual(match(path), 1)
        other = bytearray(path)
        other[24] ^= 1
        self.assertEqual(match(bytes(other)), 0)
        self.assertEqual(match(end), 0)
        mbr = bytearray(path)
        mbr[40:42] = bytes([1, 1])
        self.assertEqual(match(bytes(mbr)), 0)
        for bad in (path[:-1], path + b'x', bytes(hd) * 2 + end,
                    bytes(hd) + bytes([0x7f, 1, 4, 0]), b'\x04\x01\x03\0' + end,
                    b'\x04\x01\xff\xff' + end, b'x' * 4097):
            self.assertEqual(match(bad), -1)


class EfiRuntimeTests(unittest.TestCase):
    def test_real_adapter_against_efi_protocol_callbacks(self):
        include = os.environ.get('EFI_INCLUDE', '/usr/include/efi')
        if not (Path(include) / 'efi.h').is_file():
            self.skipTest('gnu-efi headers required for EFI protocol callback tests')
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'runtime'
            subprocess.run(['cc', '-std=gnu11', '-O2', '-ffunction-sections', '-fdata-sections',
                '-fshort-wchar', '-mno-red-zone', '-DGNU_EFI_USE_MS_ABI',
                '-I', include, '-I', str(Path(include) / 'x86_64'),
                '-I', str(ROOT / 'uefi'), '-Wall', '-Wextra', '-Werror',
                '-Wno-unused-parameter', '-Wno-misleading-indentation',
                str(ROOT / 'uefi/runtime.c'), str(ROOT / 'tests/test_uefi_runtime.c'),
                '-Wl,--gc-sections', '-o', str(output)], check=True)
            subprocess.run([str(output)], check=True)


if __name__ == '__main__':
    unittest.main()
