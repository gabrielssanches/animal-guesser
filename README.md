# Animal Guesser

An ESP32-based, screen-free audio toy for young children. The device reads RFID/NFC tags associated with physical figurines and plays matching sounds from a microSD card. The initial focus is on learning animal names and sounds in Portuguese, with room to expand into other languages, stories, and interactive games.

The project uses the **Arduino framework** on an **ESP32-WROOM-32**. Firmware is built from the command line with **Arduino CLI**, and third-party Arduino library source is kept in this Git repository.

## Hardware overview

| Component | Purpose |
| --- | --- |
| ESP32-WROOM-32 development board | Main controller |
| MFRC522 RFID reader | Detects tags placed on the toy |
| microSD card | Stores audio files |
| MAX98357A I²S amplifier and speaker | Audio output |
| Push button | User input |

Future versions may add illuminated buttons, an accelerometer, and additional games. Those features are not required for the current firmware build.

## Source code structure

```text
animal-guesser/
├── Makefile                          # Convenience commands for Arduino CLI
├── README.md
└── firmware/
    ├── guesser/
    │   ├── guesser.ino               # Main application: setup and main loop
    │   ├── ID3.cpp                   # MP3/ID3 metadata support
    │   ├── ID3.h
    │   ├── ID3Callback.cpp
    │   ├── ID3Callback.h
    │   └── sketch.yaml               # Pinned build profile (reference)
    └── third_party/
        ├── BackgroundAudio/          # Vendored audio library, v1.4.3
        └── MFRC522/                  # Vendored RFID library, v1.4.12
```

The Arduino sketch folder and its main `.ino` file must have the same name (`guesser`). The `third_party` libraries are committed with the project, so building does not depend on libraries installed through the Arduino IDE or Arduino Library Manager.

The ESP32 Arduino core and its compiler tools are **not** committed to Git; their version is pinned separately.

## Arduino CLI setup (Ubuntu/Linux)

### 1. Install Arduino CLI

Install the CLI using the [official Arduino instructions](https://docs.arduino.cc/arduino-cli/installation/). For example:

```bash
mkdir -p "$HOME/.local/bin"
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh \
  | BINDIR="$HOME/.local/bin" sh

export PATH="$HOME/.local/bin:$PATH"
arduino-cli version
```

Add `$HOME/.local/bin` to your shell's `PATH` permanently if needed.

### 2. Install the ESP32 Arduino core

```bash
arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.2.1
```

Verify the installation:

```bash
arduino-cli core list
```

The firmware currently targets the generic ESP32 Dev Module board identifier:

```text
esp32:esp32:esp32
```

### 3. Compile

From the repository root, use the Makefile:

```bash
make
```

Or invoke Arduino CLI directly:

```bash
arduino-cli compile \
  --fqbn esp32:esp32:esp32 \
  --libraries "$PWD/firmware/third_party" \
  --build-path "$PWD/build/guesser" \
  firmware/guesser
```

**Important:** Use the **classic CLI build**, not `--profile wroom32`, for now. The profile-based build was found to locate the vendored library headers without compiling their implementation files, causing linker errors for `MFRC522::*` and `mad_*`. The classic build with `--libraries` has been confirmed to work.

If `sketch.yaml` contains `default_profile: wroom32`, remove that line so the classic CLI command does not automatically select the profile.

### 4. Upload and monitor

Connect the ESP32 by USB and identify the serial port:

```bash
arduino-cli board list
```

Using the Makefile:

```bash
make flash PORT=/dev/ttyUSB0
make monitor PORT=/dev/ttyUSB0
```

Or upload directly after compiling:

```bash
arduino-cli upload \
  --fqbn esp32:esp32:esp32 \
  --port /dev/ttyUSB0 \
  --build-path "$PWD/build/guesser" \
  firmware/guesser
```

Replace `/dev/ttyUSB0` with the actual device path. The serial monitor is configured for 115200 baud in the Makefile.

## Dependency versions and reproducibility

Known dependency versions for this setup:

| Dependency | Version | Location |
| --- | --- | --- |
| ESP32 Arduino core | 3.2.1 | Installed by Arduino CLI |
| BackgroundAudio | 1.4.3 | `firmware/third_party/BackgroundAudio` |
| MFRC522 | 1.4.12 | `firmware/third_party/MFRC522` |
| SD / FS / SPI | Bundled with ESP32 core 3.2.1 | ESP32 Arduino core |

To inspect your installed and vendored versions:

```bash
make versions
```

The two third-party libraries are stored in Git, preserving their exact source. Keep the core version pinned to **3.2.1** when rebuilding an older revision of the project. The normal (non-profile) `arduino-cli compile` command uses whichever ESP32 core is installed, so check `arduino-cli core list` before rebuilding on another machine.

Build artifacts should stay out of version control (for example, add `/build/` to `.gitignore`).

## Updating third-party libraries

All third-party Arduino libraries are vendored under `firmware/third_party/` and tracked directly in Git using **Git subtree**.

This means that building Box does not require installing these libraries through the Arduino IDE or Arduino Library Manager.

### Libraries

| Library | Upstream repository | Local directory |
|---|---|---|
| BackgroundAudio | https://github.com/earlephilhower/BackgroundAudio | `firmware/third_party/BackgroundAudio` |
| MFRC522 | https://github.com/miguelbalboa/rfid | `firmware/third_party/MFRC522` |

### 1. Find available releases

List the available upstream tags:

```bash
git ls-remote --tags --refs \
    https://github.com/earlephilhower/BackgroundAudio.git

git ls-remote --tags --refs \
    https://github.com/miguelbalboa/rfid.git
```

Check the upstream release notes and choose a specific, tested release rather than automatically using the latest commit.

### 2. Update a library

Run these commands from the repository root, on a clean working tree.

**BackgroundAudio:**

```bash
git subtree pull \
    --prefix=firmware/third_party/BackgroundAudio \
    https://github.com/earlephilhower/BackgroundAudio.git \
    <release-tag> --squash
```

**MFRC522:**

```bash
git subtree pull \
    --prefix=firmware/third_party/MFRC522 \
    https://github.com/miguelbalboa/rfid.git \
    <release-tag> --squash
```

Replace `<release-tag>` with the desired upstream version, such as `1.4.12` for an applicable MFRC522 release.

Update one library at a time to simplify troubleshooting.

**Note:** These commands require the libraries to have originally been added using `git subtree`. If they were manually copied into the repository, migrate them to subtree management first or update their files manually.

### 3. Compile and test

After updating:

```bash
make build
```

To compile with debug logging:

```bash
make DEBUG=1 build
```

Flash the firmware and test the relevant functionality:

- RFID detection and UID reading with custom NFC tags and amiibos.
- SD-card initialization and file reading.
- MP3 playback through the MAX98357A amplifier.
- Audio playback interruption and restart.

Pay particular attention to compatibility between BackgroundAudio and the installed ESP32 Arduino core.

### 4. Commit the tested update

`git subtree pull` creates Git history for the vendor update. After testing, check the repository history:

```bash
git log --oneline -5
```

Record the chosen release tag in the update commit or release documentation so the exact dependency versions remain easy to identify.

Avoid modifying vendored library sources directly. Prefer making changes in the Box application. If a library requires a local patch, document it so future subtree updates can preserve or reapply it.

### Arduino ESP32 core

The ESP32 Arduino core is **not vendored** in `firmware/third_party/`. It is installed separately through Arduino CLI.

To install a specific version:

```bash
arduino-cli core install esp32:esp32@<version>
```

Keep the version pinned in the project's build configuration or Makefile. Test core upgrades independently from third-party library upgrades to make compatibility issues easier to identify.
