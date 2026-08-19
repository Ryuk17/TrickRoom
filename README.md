<!--
 * @Author: Ryuk
 * @Date: 2026-07-02 23:04:01
 * @LastEditors: Ryuk
 * @LastEditTime: 2026-08-16 10:30:00
 * @Description: TrickRoom English README
-->

# TrickRoom

<p align="center">
  <img src="assets/Trick_Room_IX.png" alt="TrickRoom logo" width="360">
</p>

TrickRoom is a real-time voice chat room. Its voice pipeline is powered by `src/audio_engine` —
a set of audio speech-enhancement algorithms wrapped into a unified C interface.

> **Note:** Some features are still under development and the public interfaces
> may evolve. Contributions and pull requests are welcome!

> [中文文档](README_zh.md)

## Audio Engine Overview

`src/audio_engine` wraps audio-processing algorithms as
self-contained C libraries named `libAE_xxx` (`.a` / `.so` / `.dll`), where `xxx` is an
uppercase abbreviation of the algorithm. Every library exposes the same design pattern:
an opaque handle, an init config, frame-based `Process` calls, and unified error codes —
so the whole pipeline (e.g. AEC → NS → AGC2 → SRC) can be chained from plain C.

### Available Libraries

| Library      | Algorithm                        | Description                                          |
|--------------|----------------------------------|------------------------------------------------------|
| `libAE_VAD`  | Voice Activity Detection         | Standalone VAD, voice probability + threshold      |
| `libAE_SRC`  | Sample Rate Conversion           | Sinc-based resampler (push / pull)                 |
| `libAE_NS`   | Noise Suppression                | Noise suppressor (speech-preserving)               |
| `libAE_AGC`  | Automatic Gain Control (legacy)  | Analog / digital AGC                                 |
| `libAE_AGC2` | Automatic Gain Control 2         | Adaptive digital gain controller with RNN-VAD        |
| `libAE_AECM` | Acoustic Echo Cancellation Mobile| Low-resource echo canceller                          |
| `libAE_AEC`  | Acoustic Echo Cancellation (AEC3)| Full-band echo canceller (AVX2-optimized)            |
| `libAE_BF`   | Beamforming                      | Nonlinear beamformer                                 |
| `libAE_HS`   | Howling Suppression              | Goertzel tone detection + notch filters              |
| `libAE_IE`   | Intelligibility Enhancement      | Spectral intelligibility improvement                 |
| `libAE_TS`   | Transient Suppression            | Click / keyboard-noise suppression (WPD tree)        |
| `libAE_DR`   | Dereverberation                  | Doire 2017 EKF dereverberation, 16 kHz               |

## Repository Layout

```
TrickRoom/
├── README.md                       # English documentation (this file)
├── README_zh.md                    # Chinese documentation
├── LICENSE                         # Apache License 2.0
├── assets/                         # Icons and static resources
├── bin/                            # Prebuilt binaries and configuration files
├── third_party/                    # Dependencies as git submodules (abseil, eigen, NE10, pffft, OpenAL, opus)
└── src/
    ├── *.cpp / *.h                 # Chat room application (client/server)
    └── audio_engine/               # Unified audio algorithm libraries (libAE_xxx)
        ├── interface/              # Public C interfaces and shared definitions
        ├── audio_processing/       # Algorithm core implementations
        ├── signal_processing/      # Shared DSP code (windowing, filters, FFT glue)
        ├── utils/                  # Non-DSP utilities (WAV I/O, ring buffers, ...)
        ├── unitest/
        │   ├── test_ae_*.cc        # Interface tests (link against libAE_xxx)
        │   └── internal/test_*.cc  # Reference tests calling the algorithm classes directly
        ├── data/                   # WAV test vectors and expected outputs
        ├── model_weights/          # Neural-network weights (e.g. RNN-VAD)
        ├── toolchains/             # CMake toolchain files (Windows / ARM Linux)
        └── CMakeLists.txt          # Builds all libAE_xxx libraries and tests
```

## Building the Audio Engine

### Prerequisites

- CMake ≥ 3.10 and a C++20 compiler
- Git submodules initialized: `git submodule update --init`
- **abseil-cpp** built and installed to `third_party/abseil-cpp/install`
  (the build locates it through `find_package(absl)` via `CMAKE_PREFIX_PATH`)
- **Eigen** is provided as a submodule (`third_party/eigen`, pinned to the 5.0.0 tag).
  It is required by `libAE_DR` only; point `-DEIGEN3_ROOT=<path>` at another copy if desired.
- NE10 (`neon-fft`) and pffft submodules are used by the FFT-based modules
  (`libAE_VAD`, `libAE_NS`, `libAE_AEC`, `libAE_AECM`, `libAE_BF`, `libAE_IE`, `libAE_TS`).
- **OpenAL Soft** (`third_party/openal-soft`) and **Opus** (`third_party/opus`)
  submodules are used by the chat-room application, not by the engine libraries.

### Configure and Build

Windows (MinGW):

```bash
cmake -S src/audio_engine -B build -G "MinGW Makefiles" \
      -DCMAKE_TOOLCHAIN_FILE=src/audio_engine/toolchains/x86_64-windows.cmake
cmake --build build -j
```

Cross-compiling for embedded Linux (aarch64 / armv7-NEON):

```bash
cmake -S src/audio_engine -B build-arm64 \
      -DCMAKE_TOOLCHAIN_FILE=src/audio_engine/toolchains/aarch64-linux-gnu.toolchain.cmake
cmake --build build-arm64 -j
```

All libraries are enabled by default. Disable individual ones with
`-DBUILD_AE_<XXX>=OFF`, e.g. `-DBUILD_AE_DR=OFF`. Available options:
`BUILD_AE_VAD`, `BUILD_AE_SRC`, `BUILD_AE_NS`, `BUILD_AE_AGC`, `BUILD_AE_AGC2`,
`BUILD_AE_AECM`, `BUILD_AE_AEC`, `BUILD_AE_BF`, `BUILD_AE_HS`, `BUILD_AE_IE`,
`BUILD_AE_TS`, `BUILD_AE_DR`.

Outputs:

- Libraries: `src/audio_engine/lib/libAE_*.a`
- Test executables: `src/audio_engine/bin/test_ae_*.exe`

## Unified C Interface

All libraries follow one pattern (see `src/audio_engine/interface/audio_engine_def.h`
for shared status codes and `audio_engine_vad.h` as the reference example):

```c
typedef void* VadHandle;

typedef struct {
    int sample_rate;   /* e.g. 16000                          */
    int frame_len;     /* samples per frame, 10 ms constraint */
} VadInitConfig;

typedef struct {
    float threshold;   /* voice probability threshold, (0.0, 1.0] */
} VadRtConfig;

VadHandle AudioEngine_Vad_Create(void);
int AudioEngine_Vad_Init(VadHandle h, const VadInitConfig* cfg);
int AudioEngine_Vad_SetParam(VadHandle h, const VadRtConfig* cfg);    /* incremental: overwrite passed fields */
int AudioEngine_Vad_ResetParam(VadHandle h, const VadRtConfig* cfg);  /* full: restore defaults, then apply */
int AudioEngine_Vad_Process(VadHandle h, const short* audio_in, int samples, int* vad_flag);
int AudioEngine_Vad_Deinit(VadHandle h);
int AudioEngine_Vad_Reset(VadHandle h);   /* rebuild internal state, keep configuration */
int AudioEngine_Vad_Destroy(VadHandle h);
```

Conventions shared by every module:

- Opaque handle per instance; `Create` / `Init` / `Process` / `Deinit` / `Destroy`
  lifecycle plus `Reset` (recreates the internal state, keeps the configuration).
- Optional runtime config for tunable algorithms: `SetParam` updates incrementally,
  `ResetParam` restores factory defaults first and then applies the new values.
- Unified status codes: `AUDIO_ENGINE_SUCCESS`, `ERR_INVALID_HANDLE`, `ERR_NULL_POINTER`,
  `ERR_NOT_INITIALIZED`, `ERR_INVALID_PARAM`, `ERR_INIT_FAILED`, `ERR_PROCESS_FAILED`.
- 16-bit PCM in / 16-bit PCM out; frame length per algorithm
  (e.g. VAD/NS/AEC use 10 ms frames, DR uses 64 samples at 16 kHz).
- Every API is instrumented with debug/warn/error logging.
- FFT-based modules use the NE10 NEON FFT for ARM-friendly performance.
  `libAE_DR` is the deliberate exception: its EKF requires double-precision FFT
  (Eigen), see `audio_processing/dereverberation/real_fft.h`.

## Testing

Two test layers under `src/audio_engine/unitest`:

- `internal/test_*.cc` — reference tests that call the algorithm classes directly
  (the ground truth for behavior).
- `test_ae_*.cc` — interface tests that link against `libAE_xxx` and exercise the
  C API: normal flow, `Reset`, and error cases.

Run an interface test from `src/audio_engine` (test vectors are read from `data/`):

```bash
./bin/test_ae_vad.exe
```

Consistency is enforced: interface output must be **byte-identical** to the
reference output produced by the corresponding internal test
(see `data/*_out.wav`).

## Cross-Platform

The audio engine is designed for Windows, desktop Linux and embedded Linux:

- No OS-specific code in the public interfaces.
- Toolchain files for `x86_64-windows` (MinGW), `aarch64-linux-gnu` and
  `arm-linux-gnueabihf` (NEON) under `src/audio_engine/toolchains/`.
- AVX2 intrinsics are used in dedicated per-file implementations on x86-64
  targets (e.g. `sinc_resampler_avx2.cc`, AEC3 vector math).

## License

TrickRoom itself is released under the [Apache License 2.0](LICENSE).

The project depends on third-party components, and their licenses apply to the
respective portions of this repository as well. Please review the license of
every dependency before distributing or embedding:

| Component                                          | License                                                          |
|----------------------------------------------------|------------------------------------------------------------------|
| This repository (TrickRoom)                        | Apache License 2.0                                               |
| abseil-cpp                                         | Apache License 2.0                                               |
| NE10 / neon-fft                                    | Apache License 2.0                                               |
| WebRTC (`audio_processing`, `signal_processing`)   | BSD 3-Clause; a few routines are public domain (see `src/audio_engine/signal_processing/LICENSE`) |
| pffft                                              | BSD-like permissive license                                      |
| Eigen                                              | MPL 2.0 (a few files under BSD or other MPL2-compatible terms)   |
| Voicebox v_spendred (libAE_DR reference)           | See the upstream [Voicebox](https://github.com/ImperialCollegeLondon/sap-voicebox) project |
| OpenAL Soft (`third_party/openal-soft`)            | LGPL 2.0 or later                                                |
| Opus (`third_party/opus`)                          | BSD 3-Clause                                                     |

> This overview is provided for convenience only and does not constitute legal advice.

## Reference

1. <https://github.com/akw0088/zoomy> — the chat-room code base TrickRoom builds on
2. <https://webrtc.googlesource.com/src/> — WebRTC, source of most wrapped algorithms
3. <https://github.com/ImperialCollegeLondon/sap-voicebox> — MATLAB Voicebox toolbox
   (v_spendred), the reference for libAE_DR
4. C. S. J. Doire et al., "Single-channel online enhancement of speech corrupted by
   reverberation and noise", IEEE Trans. ASLP 25(3), 2017 (libAE_DR)
