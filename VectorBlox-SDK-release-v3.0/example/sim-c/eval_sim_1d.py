#!/usr/bin/env python3
import argparse
import csv
import os
import re
import subprocess
import sys
import time
from typing import Optional, Tuple

import numpy as np


PRED_RE = re.compile(r"Predicted:\s*Class\s*(\d+)")


def parse_sim_output(text: str) -> Tuple[Optional[int], Optional[float], Optional[float]]:
    """
    sim-1D-model 출력에서 (pred_class, score_normal, score_anomaly)를 파싱.
    """
    pred = None
    m = PRED_RE.search(text)
    if m:
        pred = int(m.group(1))

    score0 = None
    score1 = None
    for line in text.splitlines():
        if "Class 0 (Normal):" in line:
            try:
                score0 = float(line.split(":")[1].strip())
            except Exception:
                pass
        elif "Class 1 (Anomaly):" in line:
            try:
                score1 = float(line.split(":")[1].strip())
            except Exception:
                pass
    return pred, score0, score1


def write_tmp_bin(x_uint8_1x1x1x256: np.ndarray, tmp_path: str) -> None:
    arr = x_uint8_1x1x1x256.reshape(-1)
    if arr.size != 256:
        raise ValueError(f"Expected 256 elements, got {arr.size}")
    arr.tofile(tmp_path)


def main():
    p = argparse.ArgumentParser(
        description="Evaluate sim-1D-model accuracy on X.npy/y.npy (mmap) without generating many .bin files."
    )
    p.add_argument("--model", required=True, help="Path to .vnnx model file")
    p.add_argument("--sim", default=None, help="Path to sim-1D-model (default: use ./sim-1D-model in cwd)")
    p.add_argument("--x", default="/home/ljw/AIEngines/CNN_exam/bearing_vbx/X.npy",
                   help="Path to X.npy (shape: N,1,1,256 float32 0~1)")
    p.add_argument("--y", default="/home/ljw/AIEngines/CNN_exam/bearing_vbx/y.npy",
                   help="Path to y.npy (shape: N, int64 labels 0/1)")
    p.add_argument("--out_csv", default="eval_sim_1d.csv", help="Output CSV path")
    p.add_argument("--tmp_bin", default="/tmp/sim1d_tmp.bin", help="Temporary .bin path (overwritten each sample)")
    p.add_argument("--start", type=int, default=0, help="Start index (inclusive)")
    p.add_argument("--end", type=int, default=-1, help="End index (exclusive), -1 = until end")
    p.add_argument("--every", type=int, default=1, help="Evaluate every k-th sample (subsample)")
    p.add_argument("--max_samples", type=int, default=0, help="Stop after this many evaluated samples (0=all)")
    p.add_argument("--quiet", action="store_true", help="Less stdout")
    args = p.parse_args()

    sim_path = args.sim or os.path.join(os.getcwd(), "sim-1D-model")
    if not os.path.isfile(sim_path):
        raise FileNotFoundError(f"sim-1D-model not found: {sim_path} (use --sim to specify)")
    if not os.path.isfile(args.model):
        raise FileNotFoundError(f"Model not found: {args.model}")
    if not os.path.isfile(args.x) or not os.path.isfile(args.y):
        raise FileNotFoundError(f"Missing X or y: {args.x} / {args.y}")

    X = np.load(args.x, mmap_mode="r")
    y = np.load(args.y, mmap_mode="r")
    if X.ndim != 4 or X.shape[1:] != (1, 1, 256):
        raise ValueError(f"Unexpected X shape: {X.shape} (expected N,1,1,256)")
    if y.shape[0] != X.shape[0]:
        raise ValueError(f"X and y length mismatch: {X.shape[0]} vs {y.shape[0]}")

    n = X.shape[0]
    start = max(0, args.start)
    end = n if args.end == -1 else min(n, args.end)
    if end <= start:
        raise ValueError(f"Invalid range: start={start}, end={end}, n={n}")
    if args.every <= 0:
        raise ValueError("--every must be >= 1")

    os.makedirs(os.path.dirname(os.path.abspath(args.out_csv)) or ".", exist_ok=True)
    f = open(args.out_csv, "w", newline="", encoding="utf-8")
    w = csv.writer(f)
    w.writerow(["idx", "y_true", "y_pred", "score_normal", "score_anomaly", "ok"])

    total = 0
    correct = 0
    cm00 = cm01 = cm10 = cm11 = 0
    failed = 0
    t0 = time.time()

    try:
        for idx in range(start, end, args.every):
            if args.max_samples and total >= args.max_samples:
                break

            x = np.asarray(X[idx], dtype=np.float32)  # (1,1,256)
            x_uint8 = np.clip(x * 255.0, 0, 255).astype(np.uint8)
            write_tmp_bin(x_uint8, args.tmp_bin)

            proc = subprocess.run(
                [sim_path, args.model, args.tmp_bin],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
            )
            out = proc.stdout or ""
            pred, s0, s1 = parse_sim_output(out)
            y_true = int(y[idx])

            ok = (proc.returncode == 0) and (pred is not None)
            if ok:
                total += 1
                if pred == y_true:
                    correct += 1
                if y_true == 0 and pred == 0:
                    cm00 += 1
                elif y_true == 0 and pred == 1:
                    cm01 += 1
                elif y_true == 1 and pred == 0:
                    cm10 += 1
                elif y_true == 1 and pred == 1:
                    cm11 += 1
                w.writerow([idx, y_true, pred, s0, s1, 1])
            else:
                failed += 1
                w.writerow([idx, y_true, pred if pred is not None else "", s0 if s0 is not None else "", s1 if s1 is not None else "", 0])

            if not args.quiet and total and (total % 50 == 0):
                dt = time.time() - t0
                acc = correct / total if total else 0.0
                rate = total / dt if dt > 0 else 0.0
                print(f"[{total} samples] acc={acc:.4f} ({correct}/{total})  rate={rate:.2f} samp/s  failed={failed}")

    finally:
        f.close()

    dt = time.time() - t0
    acc = correct / total if total else 0.0
    print("\n=== Eval summary ===")
    print(f"evaluated: {total} (failed: {failed})")
    print(f"accuracy:  {acc:.6f} ({correct}/{total})")
    print("confusion_matrix (rows=true, cols=pred):")
    print(f"  true0: [pred0={cm00}, pred1={cm01}]")
    print(f"  true1: [pred0={cm10}, pred1={cm11}]")
    if dt > 0:
        print(f"elapsed:   {dt:.2f}s  ({(total/dt):.2f} samples/s)")
    print(f"csv:       {os.path.abspath(args.out_csv)}")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nInterrupted.", file=sys.stderr)
        raise

