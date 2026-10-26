# Production Test Specification

## Document Information
- **Version:** 1.0
- **Status:** Released
- **Applicable Devices:** IWR6843, AWR2243, AWR2944

## 1. Scope

This document specifies the RF production test requirements for mmWave radar sensor devices. It defines the test sequence, measurement methods, pass/fail criteria, and equipment requirements for production-line testing.

## 2. Test Equipment Requirements

| Equipment | Specification | Example |
|-----------|--------------|---------|
| Spectrum Analyzer | 60-81 GHz, RBW < 1 kHz | Keysight N9041B |
| Signal Generator | 60-81 GHz, phase noise < -100 dBc/Hz @ 1 MHz | R&S SMW200A |
| Noise Source | ENR 15 dB, 60-81 GHz | Keysight 346C |
| Power Supply | 3.3V/5V, current limit | Keysight E36312A |
| DUT Interface Board | UART, JTAG, RF connectors | Custom PCB |

## 3. Test Sequence

### 3.1 Production Test Flow
```
Power On -> Boot -> Init Check -> Cal Sequence -> Test Sequence -> Report -> Power Off
```

### 3.2 Test Order
1. Synthesizer lock verification
2. TX power (all channels)
3. TX power flatness
4. RX noise floor (all channels)
5. Noise figure (all channels)
6. Phase noise
7. Frequency accuracy
8. TX EVM
9. Inter-channel matching

### 3.3 Estimated Test Time
- Per-device: < 30 seconds
- Including calibration: < 60 seconds

## 4. Pass/Fail Criteria

| Parameter | Min | Max | Unit |
|-----------|-----|-----|------|
| TX Power (per CH) | 10.0 | 13.0 | dBm |
| TX Power Variation | - | 1.5 | dB |
| TX Flatness | - | 1.0 | dB pk-pk |
| RX Noise Floor | - | -100 | dBm |
| Noise Figure | - | 14.0 | dB |
| Phase Noise @1MHz | - | -85 | dBc/Hz |
| Frequency Error | -50 | +50 | kHz |
| TX EVM | - | -25 | dB |

## 5. Test Station Setup

### 5.1 Hardware Connections
- DUT power supply via bench supply (3.3V, current-limited to 500 mA)
- UART via USB-to-serial adapter (921600 baud)
- RF output via SMA connector to spectrum analyzer
- Noise source connected to RX input for NF measurement

### 5.2 Software Configuration
- Load `production_test.json` profile
- Set COM port and instrument VISA addresses
- Configure output report directory

### 5.3 Environment
- Temperature: 25 +/- 5 C
- Humidity: < 70% RH
- RF shielded enclosure recommended

## 6. Data Logging

All test results are logged in JSON format with:
- Device serial number
- Test station ID
- Operator ID
- Timestamp
- Per-test measured values and pass/fail status
- Overall device verdict

## 7. Yield Monitoring

- Target first-pass yield: > 95%
- Yield data is aggregated by the `yield_analyzer.pl` script
- Weekly Pareto analysis of top failure modes
- SPC charts for key parameters (TX power, noise figure)
