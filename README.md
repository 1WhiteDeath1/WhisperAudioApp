# Whisper Audio Editor

A desktop WAV editor written in C++ (SFML + Dear ImGui) where the signal processing is written from scratch: no FFT or DSP library is used.

> The name "Whisper" is just what the audio buffer class is called in this project. It has nothing to do with OpenAI's Whisper speech-to-text model.

<!-- add a screenshot or short demo gif here, for example docs/screenshot.png -->

## Features

- Load 8-bit or 16-bit PCM WAV files (mono, stereo or multichannel)
- Three views: waveform (with playhead, zoom and scrolling), spectrogram and spectrum (log frequency axis, hover for frequency and dB)
- Play the selected part of the audio (with looping), crop it and export it as a new WAV
- Undo and redo (`Ctrl+Z`, `Ctrl+Y`) for the last 10 edits
- A/B compare: switch between the original and the processed audio while listening (`A`)
- Live spectrum analyzer and level meters, plus peak, RMS, crest factor and clipping stats
- Time domain tools: reverse, peak normalize, fade in, fade out, gain
- Frequency domain tools: low-pass, high-pass, band-pass and a 10-band EQ (31 Hz to 16 kHz)
- Noise reduction: pick a quiet part of the file as a noise sample and it gets subtracted from the whole file
- Spectral gate: removes frequency bins that are much quieter than the loudest one in each frame
- Before/after spectrogram image export, and a window snapshot (`F12`)
- Light and dark theme

## How it works

- **FFT:** radix-2 Cooley-Tukey with bit-reversal reordering and a precomputed twiddle table. The inverse FFT uses the conjugate trick.
- **STFT:** 2048 sample Hann-windowed frames with a hop of 512. Audio is put back together with overlap-add, normalized by the sum of the squared windows.
- **Filters and EQ:** a weight for every frequency bin (mirrored for the negative frequencies) that gets multiplied into every frame.
- **Noise reduction:** spectral subtraction. The average magnitude of the noise sample is subtracted from each frame's magnitude and the phase is kept.
- **Stereo:** samples are stored interleaved, so every effect splits the audio into channels, processes each channel on its own and puts them back together.
- **WAV files:** the reader and writer are hand written. Extra chunks (LIST etc.) are skipped, and files it cannot handle are rejected with a message instead of crashing.

## Building (Windows)

You need Visual Studio with the "Desktop development with C++" workload. The project is x64 only, uses C++20 and the `v145` platform toolset. On an older Visual Studio, change the toolset in Project Properties > General.

Libraries used (not included in this repository, except ImGuiFileDialog):

| Library | Version |
| --- | --- |
| SFML (Visual C++, 64-bit) | 2.6.2 |
| Dear ImGui | 1.89.9 |
| imgui-SFML | 2.6.1 |
| ImGuiFileDialog | included in this repository |

The project expects the libraries at:

```
F:\lib\SFML-2.6.2
F:\lib\imgui-1.89.9\imgui-1.89.9
F:\lib\imgui-sfml-2.6.1\imgui-sfml-2.6.1
```

To use another location, update these in `WhisperAudioApp/WhisperAudioApp.vcxproj` (or in Project Properties):

- C/C++ > General > Additional Include Directories
- Linker > General > Additional Library Directories
- the five `ClCompile` entries that point at the ImGui and imgui-SFML source files

Then build and run. Copy the SFML DLLs (and `openal32.dll`) from SFML's `bin` folder next to the executable. The Debug build uses the `-d` versions (`sfml-graphics-d-2.dll` etc.).

## Using it

1. Click **Open...** (or press `Ctrl+O`) and pick a WAV file.
2. Apply effects from the tabs in the sidebar (Edit, Filter, EQ, Noise). Press `Ctrl+Z` to undo an edit, and `A` to compare with the original.
3. Drag on the waveform (or set **Start** and **End** in the Edit tab) to choose the part you want, press `Space` to listen to it, and use the **Export** tab to save it as a new WAV.

For noise reduction, `Shift`+drag on the waveform over a part of the file that only has background noise (or set **Start** and **End** in the Noise tab), then apply it. The noise sample must be longer than about 0.05 seconds.

Other shortcuts: `L` loop, mouse wheel zoom, `F12` window snapshot.

## Project layout

```
WhisperAudioApp/
  main.cpp                  window, UI and drawing
  Spectogram.cpp            STFT frames and overlap-add resynthesis
  AudioEngine/
    Audio/Whisper.*         WAV reading/writing and the audio buffer
    DSP/FFT.*               FFT, spectrum, noise profile, spectral subtraction/gate
    Math/Complex.*          complex numbers
    Math/Filter.*           low/high/band-pass and EQ weights
    Math/WavHeader.h        44 byte WAV header
  ImGuiFileDialog.*         file browser (third party)
```

## Known limitations

- WAV only, 8-bit or 16-bit PCM (no 24-bit, float or compressed files)
- Undo only goes back 10 steps, because each step stores a full copy of the audio
- Filters are brick-wall (bins are either kept or removed), so sharp filters can ring

## License

The code in this project is released under the [MIT License](LICENSE), except for the third-party code listed below.

## Third-party software

| Library | License | How it is used |
| --- | --- | --- |
| [ImGuiFileDialog](https://github.com/aiekick/ImGuiFileDialog) by Stephane Cuillerdier (aiekick) | MIT | Source files are included in this repository (`ImGuiFileDialog.*`). Its copyright and license notice is kept at the top of each file. |
| [Dear ImGui](https://github.com/ocornut/imgui) | MIT | Not included, built from your own copy. |
| [imgui-SFML](https://github.com/SFML/imgui-sfml) | MIT | Not included, built from your own copy. |
| [SFML](https://www.sfml-dev.org/) | zlib/libpng | Not included, linked dynamically. |
| [OpenAL Soft](https://openal-soft.org/) (`openal32.dll`, shipped with SFML) | LGPL 2.1 | Not included. If you distribute a build, ship it as a separate DLL together with its license text. |

Each library keeps its own license and copyright. Follow those terms if you redistribute them.
