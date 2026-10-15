# QComp — punch compressor

**Headroom Audio** · v0.1.0

A feedforward peak compressor with linked stereo detection, soft knee,
automatic makeup gain and a parallel "Punch" mix knob.

## Controls

| Knob      | Range            | What it does                                        |
|-----------|------------------|-----------------------------------------------------|
| Threshold | -60 … 0 dB       | Level where compression kicks in                    |
| Ratio     | 1 … 20           | How hard it squeezes past the threshold             |
| Attack    | 0.1 … 100 ms     | How fast it clamps down on transients               |
| Release   | 10 … 1000 ms     | How fast it lets go                                 |
| Knee      | 0 … 24 dB        | Soft-knee width (0 = hard knee)                     |
| Punch     | 0 … 100 %        | Parallel mix: dry ↔ fully compressed                |
| Makeup    | -12 … +12 dB     | Manual trim on top of the automatic makeup gain     |

The GR meter shows real-time gain reduction in dB (0 … -24).

## Building (Linux)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

The VST3 lands in `build/QComp_artefacts/Release/VST3/`, the standalone in
`build/QComp_artefacts/Release/Standalone/`. (AU is macOS-only and is skipped
on Linux; macOS/Windows builds run through the same CMake project.)

JUCE 7.0.12 is fetched automatically on first configure (~150 MB, cached after).
