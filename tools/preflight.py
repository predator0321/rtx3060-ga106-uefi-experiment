#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Read Linux PCI sysfs only. Print board-pinned arguments; never execute them."""
import argparse
import json
from pathlib import Path
import re
import subprocess

BDF = re.compile(r"[0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}\.[0-7]\Z")


def inspect(root, target):
    checks = {}
    for name in ("vendor", "device", "subsystem_vendor", "subsystem_device", "reset_method"):
        try:
            checks[name] = (root / target / name).read_text().strip()
        except OSError:
            checks[name] = None
    def number(name):
        try:
            return int(checks[name], 0)
        except (TypeError, ValueError):
            return -1
    matched = number("vendor") == 0x10de and number("device") == 0x2504
    board_valid = all(0 < number(key) < 0xffff for key in ("subsystem_vendor", "subsystem_device"))
    driver = root / target / "driver"
    driver_name = driver.resolve().name if driver.is_symlink() else None
    reset = checks.get("reset_method") or ""
    flr = "flr" in reset.replace("[", "").replace("]", "").split()
    result = dict(target=target, checks=checks, device_matches=matched,
                  board_identity_valid=board_valid, driver=driver_name,
                  flr_advertised=flr, writes_performed=False,
                  ready_for_pre_driver_attempt=matched and board_valid and flr and driver_name is None,
                  note="An unbound device alone does not prove cold state; 12 GiB is checked by the kernel before DMA.")
    if matched and board_valid:
        result["one_shot_boot_args"] = (
            f"ga106_unlock.target={target} ga106_unlock.execute=1 "
            f"ga106_unlock.board_vendor={checks['subsystem_vendor']} "
            f"ga106_unlock.board_device={checks['subsystem_device']}")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", help="Full PCI BDF, obtained with lspci -Dnn")
    args = parser.parse_args()
    root = Path("/sys/bus/pci/devices")
    if not root.is_dir():
        parser.exit(2, "Linux PCI sysfs is required. This tool does not unlock Windows GPUs.\n")
    target = args.target
    if target is None:
        candidates = [p.name for p in sorted(root.iterdir())
                      if BDF.fullmatch(p.name) and inspect(root, p.name)["device_matches"]]
        if len(candidates) != 1:
            parser.error("Specify --target: expected exactly one 10de:2504 GPU")
        target = candidates[0]
    if not BDF.fullmatch(target):
        parser.error("Expected a full PCI address such as 0000:01:00.0")
    report = inspect(root, target)
    try:
        out = subprocess.run(["nvidia-smi", "--query-gpu=name,pci.bus_id,driver_version,vbios_version,memory.total", "--format=csv"],
                             capture_output=True, text=True, timeout=10)
        report["nvidia_smi"] = dict(returncode=out.returncode, stdout=out.stdout, stderr=out.stderr)
    except (OSError, subprocess.TimeoutExpired) as error:
        report["nvidia_smi"] = dict(error=str(error))
    print(json.dumps(report, indent=2))
    return 0 if report["device_matches"] and report["board_identity_valid"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
