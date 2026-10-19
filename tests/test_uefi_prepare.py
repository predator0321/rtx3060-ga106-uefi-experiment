import importlib.util
from pathlib import Path
import subprocess
import types
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('prepare_usb', ROOT / 'uefi/prepare_usb.py')
prepare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prepare)


class WindowsPreparationTests(unittest.TestCase):
    def test_board_detection_uses_actual_subsystem_and_normalizes_segment(self):
        text = '00000000:01:00.0, 0x250410DE, 0x397D1462, 12288\n'
        with patch.object(prepare.subprocess, 'run', return_value=types.SimpleNamespace(stdout=text)):
            self.assertEqual(prepare.detect_card(), ('0000:01:00.0', 0x1462, 0x397d))

    def test_wrong_sku_duplicate_and_invalid_board_are_refused(self):
        for text in ('00000000:01:00.0, 0x250310DE, 0x397D1462, 12288\n',
                     '00000000:01:00.0, 0x250410DE, 0x397D1462, 8192\n',
                     '00000000:01:00.0, 0x250410DE, 0x00000000, 12288\n',
                     '00010000:01:00.0, 0x250410DE, 0x397D1462, 12288\n',
                     '00000000:01:00.0, 0x250410DE, 0x397D1462, 12288\n' * 2):
            with patch.object(prepare.subprocess, 'run', return_value=types.SimpleNamespace(stdout=text)):
                with self.assertRaises(ValueError):
                    prepare.detect_card()


if __name__ == '__main__':
    unittest.main()
