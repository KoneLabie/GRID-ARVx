#!/usr/bin/env python3
"""Pixel regressions for the independent ReferenceFx renderer, not Vel Engine."""
from pathlib import Path
import json
import sys
import numpy as np
from PIL import Image

def run(folder):
    backend = folder / "backend.json"
    if backend.exists() and json.loads(backend.read_text(encoding="utf-8")).get("pixelExpectations") != "reference-fx":
        print("NOT APPLICABLE: ReferenceFx pixel expectations do not describe the original native engine. Use native_fx_image_checks.py.")
        return 2
    images = {}
    for line in (folder / "inventory.txt").read_text().splitlines():
        filename, name = line.split("\t", 1)
        images[name] = np.asarray(Image.open(folder / filename).convert("RGB"), dtype=np.float64)
    height, width, _ = next(iter(images.values())).shape
    rows = slice(int(height * .30), int(height * .65))
    middle = slice(int(width * .41), int(width * .59))
    def roi(name, columns=middle):
        return images[name][rows, columns]
    report = []
    def check(condition, label, detail=""):
        report.append(("PASS " if condition else "FAIL ") + label + (" / " + detail if detail else ""))
    baseline = roi("gray-baseline")
    for kind in ("noise", "grain"):
        check(np.max(np.abs(roi(kind + "-amount-0") - baseline)) <= 1,
              kind + " CC zero bypass")
        deviations = [np.mean(np.abs(roi(f"{kind}-amount-{amount}") - baseline)) for amount in (32, 80, 127)]
        check(min(deviations) > 10, kind + " produces palette quantization at nonzero strength",
              ", ".join(f"{number:.2f}" for number in deviations))
        check(np.mean(np.abs(roi(kind + "-amount-32") - roi(kind + "-amount-127"))) > 5,
              kind + " strength changes the quantization pattern")
        # Native Grain is deterministic error diffusion, not synthetic film grain.
        # Only native Noise must generate an independent random pattern each frame.
        if kind == "grain":
            check(max(np.max(np.abs(roi(f"grain-time-{frame}") - roi("grain-time-0")))
                      for frame in range(1, 4)) <= 1,
                  "Grain error diffusion stays stable for a stationary source")
            continue
        temporal = [roi(f"{kind}-time-{frame}") - baseline for frame in range(4)]
        correlations = []
        for previous, following in zip(temporal, temporal[1:]):
            # Check both stationary correlation and the one-pixel translations
            # that expose a scrolling static texture.
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    a = previous[1:-1, 1:-1].ravel()
                    b = following[1 + dy:following.shape[0] - 1 + dy,
                                  1 + dx:following.shape[1] - 1 + dx].ravel()
                    correlations.append(abs(float(np.corrcoef(a, b)[0, 1])))
        maximum = max(correlations)
        check(np.isfinite(maximum) and maximum < .10, kind + " regenerates without translating a fixed pattern",
              f"maximum temporal/shift correlation {maximum:.4f}")
    noise = roi("noise-amount-127")
    grain = roi("grain-amount-127")
    check(np.mean(np.abs(noise - grain)) > 10,
          "Noise random quantization differs from Grain error diffusion")
    body = (slice(int(height * .16), int(height * .72)), slice(int(width * .02), int(width * .98)))
    grid = images["glass-baseline"][body]
    check(np.max(np.abs(images["glass-ball-amount-0"][body] - grid)) <= 1, "Glass CC zero bypass")
    ball = images["glass-ball-amount-127"][body]
    chromatic_pixels = int(np.count_nonzero(np.ptp(ball, axis=2) > 24))
    check(chromatic_pixels > 300, "Glass disperses RGB rays from a monochrome source",
          f"{chromatic_pixels} chromatic pixels")
    # The recovered maximum-radius Ball reaches roughly 8% from the edge;
    # use the outermost 4% so this test does not sample the refracting rim.
    edge = (rows, slice(int(width * .005), int(width * .045)))
    outside = images["glass-ball-amount-127"][edge]
    outside_source = images["glass-baseline"][edge]
    # Enabling native FX downsamples to the original 600x500 working buffer.
    # Allow its presentation filtering, while detecting blackout or lost grid.
    outside_correlation = float(np.corrcoef(outside.ravel(), outside_source.ravel())[0, 1])
    outside_brightness = float(np.mean(outside) / max(1, np.mean(outside_source)))
    check(outside_correlation > .95 and .9 < outside_brightness < 1.1,
          "Glass preserves the scene outside the lens at maximum strength",
          f"pattern correlation {outside_correlation:.4f}, brightness ratio {outside_brightness:.4f}")
    difference = float(np.mean(np.abs(ball - grid)))
    check(difference > 8, "Glass refracts grid geometry", f"mean absolute difference {difference:.2f}")
    names = ["glass-ball-amount-127", "glass-2x2", "glass-Full", "glass-Flower", "glass-Cross", "glass-Burst"]
    distinct = min(float(np.mean(np.abs(images[a][body] - images[b][body])))
                   for i, a in enumerate(names) for b in names[i + 1:])
    check(distinct > 2, "All six glass shapes produce distinct optics", f"minimum pair difference {distinct:.2f}")
    check(np.max(np.abs(roi("symbol-amount-0") - baseline)) <= 1, "Symbol CC zero bypass")
    symbols = roi("symbol-amount-127")
    check(np.std(symbols) > 12 and np.mean(symbols < 20) > .3 and np.mean(symbols > 80) > .05,
          "Symbol dithers the source into visible character strokes")
    changes = [np.mean(np.abs(roi(f"symbol-time-{n}") - roi(f"symbol-time-{n+1}"))) for n in range(2)]
    check(min(changes) > 3, "Symbol changes its character selection over time")
    text = "\n".join(report) + "\n"
    (folder / "image-checks.txt").write_text(text)
    print(text, end="")
    return 1 if any(line.startswith("FAIL") for line in report) else 0

if __name__ == "__main__":
    raise SystemExit(run(Path(sys.argv[1])))
