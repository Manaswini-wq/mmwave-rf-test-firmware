# mmWave RF Test Firmware - Production Test & Evaluation Platform

![Build Status](https://img.shields.io/badge/build-passing-brightgreen)
![Tests](https://img.shields.io/badge/tests-142%20passed-brightgreen)
![Python](https://img.shields.io/badge/python-3.8%2B-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Platform](https://img.shields.io/badge/platform-IWR6843%20%7C%20AWR2243%20%7C%20AWR2944-orange)

## Overview

Production-grade RF evaluation and test firmware for **Texas Instruments mmWave radar sensors** (IWR6843, AWR2243, AWR2944). This platform provides a comprehensive bare-metal C firmware framework for RF production testing, device characterization, calibration algorithm development, and RF evaluation applications.

The firmware interfaces directly with the mixed-signal analog and digital SoC subsystems through custom analog circuit FW drivers, providing precise control over the entire RF signal chain -- from the frequency synthesizer and TX power amplifiers through the RX low-noise amplifiers, mixers, IF filters, PGAs, and high-speed ADCs.

### Key Capabilities

- **RF Test Firmware for Production Tests**: Automated test sequences for TX power, RX sensitivity, phase noise, frequency accuracy, noise figure, and TX EVM measurements
- **Calibration Algorithms**: Iterative TX power flatness calibration, RX gain/phase calibration, LO leakage nulling, and inter-channel calibration
- **Analog Circuit FW Drivers**: Register-level drivers for synthesizer, PGA, IF filter, ADC, DAC, TX frontend, and RX frontend subsystems
- **ROM Firmware**: Boot hook integration and factory trim loading from OTP memory
- **Mixed-Signal Analog and Digital SoC**: Direct hardware abstraction for the IWR6843/AWR2243/AWR2944 RF subsystem
- **Scripting & Automation**: Python, Perl, and Lua scripts for test orchestration, instrument control, data analysis, and debug
- **CI/CD Pipelines**: GitHub Actions workflows for continuous integration and nightly regression testing
- **Automated Testing Framework**: Unit, integration, and qualification test suites with pass/fail limit checking

## Architecture

```
+------------------------------------------------------------------+
|                    Host PC (Python/Perl/Lua)                      |
|  +-------------------+  +------------------+  +----------------+ |
|  | RF Test           |  | Instrument       |  | Report         | |
|  | Orchestrator      |  | Control (SCPI)   |  | Generator      | |
|  +--------+----------+  +--------+---------+  +-------+--------+ |
|           |                      |                     |          |
+-----------+----------------------+---------------------+----------+
            |          UART / USB              |
            v                                  v
+------------------------------------------------------------------+
|                   mmWave SoC (IWR6843 R5F)                       |
|  +-----------------------------------------------------------+  |
|  |                  UART Command Handler                      |  |
|  +----------------------------+------------------------------+  |
|  |    RF Test Engine          |    Calibration Engine         |  |
|  |  +--------------------+   |   +----------------------+    |  |
|  |  | TX Power Test      |   |   | TX Flatness Cal      |    |  |
|  |  | RX Sensitivity     |   |   | RX Gain/Phase Cal    |    |  |
|  |  | Phase Noise        |   |   | LO Leakage Nulling   |    |  |
|  |  | Freq Accuracy      |   |   | Inter-Channel Cal    |    |  |
|  |  | Noise Figure       |   |   +----------------------+    |  |
|  |  | TX EVM             |   |                                |  |
|  |  +--------------------+   |                                |  |
|  +----------------------------+------------------------------+  |
|  |               Hardware Abstraction Layer                   |  |
|  |  +--------+ +-----+ +--------+ +-----+ +-----+ +-------+ |  |
|  |  | Synth  | | PGA | | IF Flt | | ADC | | DAC | | DMA   | |  |
|  |  | Driver | | Drv | | Driver | | Drv | | Drv | | Mgr   | |  |
|  |  +--------+ +-----+ +--------+ +-----+ +-----+ +-------+ |  |
|  |  +------------------+  +------------------+                |  |
|  |  | TX Frontend Drv  |  | RX Frontend Drv  |                |  |
|  |  +------------------+  +------------------+                |  |
|  +-----------------------------------------------------------+  |
|  |              ROM / Factory Trim / Boot Hooks               |  |
|  +-----------------------------------------------------------+  |
+------------------------------------------------------------------+
            |                                  |
            v                                  v
+------------------------------------------------------------------+
|                    RF Hardware (Analog)                           |
|  TX Chain: PA -> Antenna    RX Chain: Antenna -> LNA -> Mixer    |
|  Synth: VCO -> PLL -> LO distribution                           |
|  ADC/DAC: High-speed data conversion                             |
+------------------------------------------------------------------+
```

## Features

### RF Production Tests
| Test | Description | Typical Limit |
|------|-------------|---------------|
| TX Power | Measures transmitted power per channel | 10-13 dBm |
| RX Sensitivity | Noise floor and minimum detectable signal | < -100 dBm |
| Phase Noise | Phase noise at 100 kHz, 1 MHz, 10 MHz offsets | < -85 dBc/Hz @ 1 MHz |
| Frequency Accuracy | Carrier frequency vs. programmed frequency | +/- 50 kHz |
| Noise Figure | Receiver noise figure via Y-factor method | < 14 dB |
| TX EVM | Error vector magnitude of TX signal | < -25 dB |

### Calibration Algorithms
- **TX Power Flatness**: Iterative DAC/gain adjustment across frequency for flat TX power
- **RX Gain/Phase Calibration**: Per-channel gain and phase offset correction
- **LO Leakage Nulling**: Iterative I/Q DC offset adjustment to minimize LO feedthrough
- **Inter-Channel Calibration**: Timing, gain, and phase alignment across RX channels

### Firmware-Hardware Interfaces
- Direct register-level access to all RF subsystem blocks
- DMA-based high-speed ADC data capture engine
- Interrupt-driven UART command protocol for host communication
- ROM boot hook integration for early initialization
- Factory trim and OTP memory management

### Software Development Life Cycle
- **Version Control**: Git-based workflow with feature branches and code reviews
- **CI/CD**: Automated build, lint, unit test on every push; nightly regression
- **Testing Pyramid**: Unit tests (per-module), integration tests (full chain), qualification tests (spec compliance)
- **Documentation**: Inline Doxygen comments, architecture guides, test specifications

### Scripting Languages
- **Python**: Test orchestration, SCPI instrument control, FFT-based data analysis, HTML/JSON report generation
- **Perl**: Log parsing, yield analysis for production data
- **Lua**: On-target debug console, rapid RF verification scripts

### Customer Engagement & Technical Expertise
- Comprehensive test guide documentation for customer onboarding
- Configurable test profiles (production, characterization, evaluation)
- Instrument configuration templates for common lab equipment
- Detailed calibration procedures with step-by-step instructions

## Building

### Prerequisites
- ARM GCC toolchain (arm-none-eabi-gcc 10.3+)
- TI mmWave SDK 3.x (for device headers)
- CMake 3.16+ or GNU Make 4.x
- Python 3.8+ (for scripts)

### Build with Make
```bash
make all           # Build firmware binary
make flash         # Flash to target via JTAG
make test          # Run host-side unit tests
make clean         # Clean build artifacts
```

### Build with CMake
```bash
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-none-eabi.cmake ..
cmake --build .
```

### Python Environment Setup
```bash
cd scripts/python
pip install -r requirements.txt
python rf_test_orchestrator.py --config ../../config/test_profiles/production_test.json
```

## Usage

### Running Production Tests
```bash
# Connect to DUT via UART
python scripts/python/rf_test_orchestrator.py \
    --port COM3 \
    --profile config/test_profiles/production_test.json \
    --report output/test_report.html
```

### Running Individual Tests via UART Console
```
> test tx_power --channel 0 --freq 77.0
TX Power Test: CH0 @ 77.000 GHz
  Measured Power: 11.2 dBm
  Limit: [10.0, 13.0] dBm
  Result: PASS

> cal tx_flatness --channel 0
TX Power Flatness Calibration: CH0
  Iteration 1: max ripple = 1.8 dB
  Iteration 2: max ripple = 0.6 dB
  Iteration 3: max ripple = 0.2 dB
  Result: PASS (ripple < 0.5 dB)
```

### Debug Console (Lua)
```bash
lua scripts/lua/debug_console.lua --port COM3
```

## Directory Structure

| Directory | Description |
|-----------|-------------|
| `src/rf_tests/` | RF production test implementations |
| `src/calibration/` | Calibration algorithm modules |
| `src/drivers/` | Analog circuit FW drivers for SoC peripherals |
| `src/rom/` | ROM firmware hooks and factory trim |
| `src/data/` | DMA-based data capture engine |
| `src/protocol/` | UART command handler and test protocol |
| `include/` | Platform types, register definitions, configuration |
| `tests/` | Unit, integration, and qualification tests |
| `scripts/` | Python, Perl, Lua automation scripts |
| `config/` | Test profiles and instrument configurations |
| `docs/` | Technical documentation and guides |
| `linker/` | Linker scripts for target devices |

## Supported Platforms

| Device | Core | RF Band | Status |
|--------|------|---------|--------|
| IWR6843 | ARM Cortex-R5F | 60-64 GHz | Primary |
| AWR2243 | External host | 76-81 GHz | Supported |
| AWR2944 | ARM Cortex-R5F | 76-81 GHz | Supported |

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/new-test`)
3. Write tests for new functionality
4. Ensure CI passes (`make test && make lint`)
5. Submit a pull request with detailed description

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## References

- [TI mmWave Sensor Documentation](https://www.ti.com/sensors/mmwave-radar/overview.html)
- [IWR6843 Technical Reference Manual](https://www.ti.com/product/IWR6843)
- [mmWave SDK User Guide](https://www.ti.com/tool/MMWAVE-SDK)
