# Bio-impedance task

This repository contains an ESP-IDF firmware project and Python GUI prototypes for an AD5933-based impedance monitor. The firmware configures an AD5933 over I2C, performs a calibration sweep and repeated measurement sweeps, and prints impedance magnitude and phase-related values. The Python programs read those serial log lines, display the latest impedance and phase, plot recent impedance samples, and—in the later versions—record measurements with user-entered labels.

## Context and credits

The checked-in files identify the project as `bio-impedance_task`, label its records `ESP32_Impedance_Monitor`, and use `TPT` as the data-file investigator initials. They do not name the UW BioRobotics Lab, Blake Hannaford, or an individual author/contributor. Therefore, this checkout alone cannot substantiate an affiliation with the UW BioRobotics Lab (PI: Blake Hannaford), and this README does not make that attribution as a repository-derived fact.

## Hardware and tools

- `main/main.c` targets an ESP32-C6 configuration with I2C SCL on GPIO 20 and SDA on GPIO 21. It initializes an AD5933 at I2C address `0x0D`, uses the AD5933 internal 16 MHz clock setting, and configures the measurement code with a 33 kHz start frequency.
- `main/AD5933.c` and `main/AD5933.h` implement AD5933 register access, frequency-sweep setup, raw real/imaginary sample reads, impedance calculation, phase calculation, and phase-compensated real/imaginary outputs.
- The GUI scripts import PyQt5, pyqtgraph, pyserial, and an external `brl_data` module. They are configured for `/dev/ttyACM0` at 115200 baud and parse the firmware log messages `impedance magnitude:` and `Calculated phase:`.
- The project uses ESP-IDF CMake files. The top-level CMake configuration imports `$ENV{IDF_PATH}/tools/cmake/project.cmake`.

## Files and recorded data

- `main/main.c` initializes the I2C bus and AD5933, obtains a calibration sweep, computes a gain factor from a 2200-ohm calibration value, then repeatedly runs a one-increment measurement sweep and logs the results.
- `main/AD5933.c` is the AD5933 driver and calculation helper used by `main.c`.
- `gui.py` continuously records parsed impedance and phase values through `brl_data` and displays a live plot.
- `gui_V2.py` adds a material input and records only while recording is toggled with the space bar.
- `gui_V3.py` adds a per-material trial number.
- `gui_V4.py` adds a test-taker input and records impedance, phase, material, trial, and test-taker fields.
- `gui_V5.py` adds a duration input, stops recording with a timer, and writes material labels suffixed with a per-session counter plus the test taker. Its metadata schema contains impedance, phase, material, and test taker.
- `data/YYYY-MM-DD/` contains dated, headerless measurement CSV files and paired `*_meta.json` sidecars. The sidecars describe three-, four-, or five-column schemas using the fields `Impedance`, `Phase`, `Material`, `Trial`, and `TestTaker`; some pairs have zero rows. The stored dates range from 2025-11-06 through 2026-01-24.
- `captures/` contains CSV captures whose explicit header is `Time (s),Voltage (V)`.

The CSV field layouts changed with the GUI revisions, so use the paired JSON sidecar rather than assuming one fixed measurement schema.

## Run

Firmware build configuration requires an ESP-IDF environment with `IDF_PATH` set. From the repository root, the checked-in ESP-IDF CMake project can be built with:

```sh
idf.py build
```

The GUI entry points are invoked directly, for example:

```sh
python gui_V5.py
```

Before running a GUI, provide its imported Python dependencies and the external `brl_data` module. The scripts append `/home/tpt-finder/Desktop/brl_data/brl_data` to `sys.path`; that module is not included in this checkout. Connect the serial device expected by the code at `/dev/ttyACM0`, and ensure the firmware is producing the two parsed log messages. Hardware operation was not performed while documenting this repository.

## Relationship to similarly named repositories

No tracked file, directory, commit message, or source reference in this checkout names `Data-for-BioZ` or `Data-processing-for-BioZ`. This repository contains firmware, GUI prototypes, dated measurement CSV/JSON pairs, and voltage captures; it is not possible from this checkout alone to establish whether the other repositories overlap with it or how they differ.
