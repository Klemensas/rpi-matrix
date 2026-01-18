# AI Benchmark Instructions

This document provides instructions for AI assistants working on the rpi-matrix codebase.

## Performance Testing Workflow

### Before Making Changes
If no baseline exists yet:
```bash
sudo ./benchmarks/benchmark.sh --save
```

### After Making Changes
```bash
sudo ./benchmarks/benchmark.sh --check
```

If the check **passes**: Your changes maintain performance. Proceed with commit.

If the check **fails**: Your changes caused a >15% FPS regression. Either:
1. Optimize your code to restore performance
2. If regression is intentional (feature tradeoff), document it and run `--save` to update baseline

### Quick Iteration
For rapid testing during development:
```bash
sudo ./benchmarks/benchmark.sh --quick
```

---

## Current Baseline (2026-01-16)

### EXTEND Mode (image spans across panels)

| Effect | FPS | Status |
|--------|-----|--------|
| Debug (Pass-through) | 55 | ✓ OK |
| Filled Silhouette | 38 | ✓ OK |
| Outline Only | 38 | ✓ OK |
| Motion Trails | 38 | ✓ OK |
| Rainbow Motion Trails | 31 | ✓ OK |
| Double Exposure | **29** | ⚠ Marginal |
| Procedural Shapes | 33 | ✓ OK |
| Wave Patterns | 52 | ✓ OK |
| Geometric Abstraction | 38 | ✓ OK |

### REPEAT Mode (same image per panel)

| Effect | FPS | Status |
|--------|-----|--------|
| Debug (Pass-through) | 39 | ✓ OK |
| Filled Silhouette | 38 | ✓ OK |
| Outline Only | 30 | ✓ OK |
| Motion Trails | 32 | ✓ OK |
| Rainbow Motion Trails | 36 | ✓ OK |
| Double Exposure | 39 | ✓ OK |
| Procedural Shapes | 38 | ✓ OK |
| Wave Patterns | 39 | ✓ OK |
| Geometric Abstraction | 38 | ✓ OK |

### Multi-Panel Mode (different effects per panel)

Testing multiple simultaneous effects. Baseline: **36 FPS** ✓ OK

---

## Performance Guidelines

### What Affects FPS

1. **Background Subtraction (MOG2)** - Most CPU-intensive, used by all motion effects
2. **Contour Finding** - O(n) with pixel count
3. **Morphological Operations** - Opens/closes for noise removal
4. **HSV↔BGR Conversions** - Rainbow effects
5. **Per-pixel Loops** - Avoid when possible

### Optimization Tips

When optimizing effects:

```cpp
// GOOD: Use OpenCV matrix operations
frame *= 0.9f;  // Vectorized

// BAD: Per-pixel loop
for (int y = 0; y < height; y++)
    for (int x = 0; x < width; x++)
        frame.at<Vec3b>(y,x) *= 0.9f;  // Slow!
```

- Reuse `cv::Mat` buffers instead of reallocating
- Use `CV_8UC*` types; avoid float conversions when possible
- Consider processing at lower resolution and upscaling
- Cache values that don't change per-frame

### Critical Effects to Watch

These effects are closest to the 30 FPS limit:

1. **Double Exposure (29 FPS in EXTEND)** - On the edge, occasional drops
2. **Outline Only (30 FPS in REPEAT)** - At the limit
3. **Rainbow Motion Trails (31 FPS in EXTEND)** - Just above target

Any changes to these effects must be carefully benchmarked.

---

## Hardware Configuration

The benchmark is configured for:
- Resolution: 576x192 (3 panels × 64x192 each)
- Sensor: 4608x2592
- LED Chain: 3 panels

If your hardware differs, edit `benchmarks/benchmark.sh`:
```bash
WIDTH=576
HEIGHT=192
SENSOR_WIDTH=4608
SENSOR_HEIGHT=2592
LED_CHAIN=3
```

---

## Understanding the Results

### FPS Thresholds

| FPS | Status | Meaning |
|-----|--------|---------|
| ≥30 | ✓ OK | Smooth playback |
| 25-29 | ⚠ Marginal | Occasional stutter possible |
| <25 | ✗ Slow | Visible frame drops |

### Regression Threshold

A **15% drop** in FPS triggers a failure. This is configurable:
```bash
REGRESSION_THRESHOLD=15  # in benchmark.sh
```

---

## Panel Modes Explained

### EXTEND Mode (default)
- Input image is split across panels horizontally
- Each panel shows a portion of the full image
- Best for wide/panoramic content

### REPEAT Mode
- Same image is shown on each panel (resized to fit)
- Good for symmetric displays or testing
- Toggle with `q` key in the app

### Multi-Panel Mode
- Different effects can run on different panels
- Toggle with `§` key in the app
- More CPU intensive (multiple effect pipelines)

---

## Updating This Document

After establishing a new baseline:
1. Run `sudo ./benchmarks/benchmark.sh --save`
2. Update the baseline tables above
3. Note any performance changes in the changelog below

## Changelog

| Date | Change | Notes |
|------|--------|-------|
| 2026-01-16 | Initial baseline | Full pipeline testing established |
