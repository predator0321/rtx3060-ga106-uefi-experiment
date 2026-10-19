#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Windows/Linux PyTorch baseline. Does not unlock or change GPU configuration."""
import argparse
import json
from pathlib import Path
import platform
import statistics
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--size', type=int, default=4096)
    parser.add_argument('--iterations', type=int, default=20)
    parser.add_argument('--samples', type=int, default=5)
    parser.add_argument('--device', type=int, default=0)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if not 256 <= args.size <= 8192 or min(args.iterations, args.samples) < 1:
        parser.error('size must be 256..8192; iterations/samples must be positive')
    try:
        import torch
    except ImportError:
        parser.exit(2, 'Run with the existing ComfyUI Python that has CUDA PyTorch installed.\n')
    if not torch.cuda.is_available():
        parser.exit(2, 'CUDA PyTorch and an NVIDIA GPU are required.\n')
    if not 0 <= args.device < torch.cuda.device_count():
        parser.error('Invalid CUDA device index')
    torch.cuda.set_device(args.device)
    torch.manual_seed(20261019)
    torch.backends.cuda.matmul.allow_tf32 = False
    # Make comparisons explicitly use FP32 accumulation without reduced-
    # precision intermediate reductions. GEMM algorithm choice may still vary.
    torch.backends.cuda.matmul.allow_fp16_reduced_precision_reduction = False
    torch.backends.cuda.matmul.allow_bf16_reduced_precision_reduction = False
    props = torch.cuda.get_device_properties(args.device)
    report = dict(kind='baseline-only; no unlock performed', os=platform.system(),
                  torch_version=torch.__version__, cuda_version=torch.version.cuda,
                  device=props.name, capability=list(torch.cuda.get_device_capability()),
                  memory_mib=props.total_memory // 2**20,
                  size=args.size, iterations=args.iterations, samples=args.samples,
                  reduced_precision_reduction=False, results={})
    try:
        result = subprocess.run(['nvidia-smi', '--query-gpu=name,pci.device_id,driver_version,vbios_version,memory.total,clocks.sm,power.draw,temperature.gpu',
                                 '--format=csv'], capture_output=True, text=True, timeout=10)
        report['nvidia_smi_before'] = dict(returncode=result.returncode, text=result.stdout)
    except (OSError, subprocess.TimeoutExpired):
        pass
    failed = False
    with torch.inference_mode():
        for label, dtype in [('FP16', torch.float16), ('BF16', torch.bfloat16)]:
            try:
                a = torch.randn(args.size, args.size, device='cuda', dtype=dtype) * 0.1
                b = torch.randn(args.size, args.size, device='cuda', dtype=dtype) * 0.1
                out = torch.empty_like(a)
                for _ in range(10):
                    torch.mm(a, b, out=out)
                torch.cuda.synchronize()
                times = []
                for _ in range(args.samples):
                    start, end = torch.cuda.Event(enable_timing=True), torch.cuda.Event(enable_timing=True)
                    start.record()
                    for _ in range(args.iterations):
                        torch.mm(a, b, out=out)
                    end.record()
                    end.synchronize()
                    times.append(start.elapsed_time(end) / args.iterations)
                # Independent FP64 reference for the first 32 output rows,
                # using all K terms and all columns. Not a full-output check.
                reference = a[:32].double() @ b.double()
                measured = out[:32].double()
                relative_l2 = ((measured - reference).norm() / reference.norm().clamp_min(1e-30)).item()
                finite = bool(torch.isfinite(out).all().item())
                passed = finite and relative_l2 < 0.02
                failed |= not passed
                median_ms = statistics.median(times)
                report['results'][label] = dict(median_ms=median_ms,
                    tflops=2 * args.size**3 / (median_ms / 1000) / 1e12,
                    sample_ms=times, finite=finite, relative_l2_sample=relative_l2,
                    checked_rows=32, correctness_sample_passed=passed)
                del a, b, out, reference, measured
            except (RuntimeError, NotImplementedError) as error:
                failed = True
                report['results'][label] = dict(error=str(error))
    report['note'] = 'Compare the same software/workload and similar clocks/power. This does not predict ComfyUI speedup or prove hardware unlocking.'
    text = json.dumps(report, indent=2)
    print(text)
    if args.output:
        args.output.write_text(text + '\n', encoding='utf-8')
    return int(failed)


if __name__ == '__main__':
    raise SystemExit(main())
