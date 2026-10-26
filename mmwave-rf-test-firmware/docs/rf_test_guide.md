# RF Test Guide

## Overview

This guide describes the RF production tests implemented in the mmWave RF test firmware. Each test is designed to verify a critical RF performance parameter of the IWR6843/AWR2243/AWR2944 mmWave radar sensor.

## Test Descriptions

### 1. TX Power Test
**Purpose:** Measure the transmit output power at the antenna port.

**Method:**
1. Configure synthesizer to CW mode at the test frequency
2. Enable the TX channel under test with a known power DAC code
3. Enable internal loopback path (TX -> RX via on-chip coupler)
4. Capture ADC samples and compute RMS power
5. Compensate for loopback path attenuation to obtain antenna-port power

**Key Parameters:**
- Frequency: 60-64 GHz (IWR6843), 76-81 GHz (AWR2243/AWR2944)
- Typical power: 12 dBm
- Limits: 10-13 dBm per channel

**Failure Analysis:**
- Power too low: Check PA bias, TX DAC code, loopback path
- Power too high: Check backoff settings, PA saturation
- No output: Verify synth lock, TX enable, loopback path

### 2. RX Sensitivity Test
**Purpose:** Measure the receiver noise floor to verify sensitivity.

**Method:**
1. Disable all TX channels (no signal at RX input)
2. Configure RX chain with maximum gain settings
3. Capture ADC samples and compute integrated noise power
4. Input-refer the noise power by subtracting total RX gain

**Key Parameters:**
- LNA gain: Maximum
- PGA gain: 42 dB
- IF filter: HPF=175 kHz, LPF=5 MHz
- Limit: < -100 dBm noise floor

### 3. Phase Noise Test
**Purpose:** Measure the synthesizer phase noise at standard offset frequencies.

**Method:**
1. Generate CW tone via loopback
2. Capture long I/Q record (4096+ samples)
3. Apply Hanning window and compute FFT
4. Read PSD at offset bins relative to carrier peak
5. Normalize to 1 Hz bandwidth

**Measurement Offsets:**
| Offset | Typical | Limit |
|--------|---------|-------|
| 100 kHz | -82 dBc/Hz | < -75 dBc/Hz |
| 1 MHz | -95 dBc/Hz | < -85 dBc/Hz |
| 10 MHz | -110 dBc/Hz | < -100 dBc/Hz |

### 4. Frequency Accuracy Test
**Purpose:** Verify the synthesizer output frequency matches the programmed value.

**Method:**
1. Program synthesizer to a known frequency
2. Capture loopback signal and compute FFT
3. Locate the spectral peak using parabolic interpolation
4. Compare measured IF frequency to expected (should be ~0 Hz)

### 5. Noise Figure Test
**Purpose:** Measure the receiver noise figure using the Y-factor method.

**Method:**
1. Configure RX chain with known gain settings
2. Measure noise power with external noise source ON (hot)
3. Measure noise power with noise source OFF (cold)
4. Compute Y-factor = P_hot / P_cold
5. NF = ENR - 10*log10(Y - 1)

### 6. TX EVM Test
**Purpose:** Measure the error vector magnitude of the transmitter.

**Method:**
1. Transmit CW tone via loopback
2. Capture I/Q samples at the RX ADC
3. Estimate ideal carrier as mean phasor
4. Compute error vector for each sample
5. EVM = 20*log10(RMS_error / RMS_signal)

## Running Tests

### Via UART Console
```
test tx_power --channel 0 --freq 62.0
test rx_noise --channel 0
test phase_noise --channel 0 --freq 62.0
test all
```

### Via Python Orchestrator
```bash
python scripts/python/rf_test_orchestrator.py \
    --port COM3 \
    --profile config/test_profiles/production_test.json
```

## Interpreting Results

Each test reports:
- **Measured value** with units
- **Pass/fail limits** from test_limits.h
- **Result**: PASS, FAIL, or MARGINAL

A MARGINAL result indicates the measured value is within limits but close to a boundary, warranting further investigation.

## Troubleshooting

| Symptom | Possible Cause | Action |
|---------|---------------|--------|
| PLL lock failure | VCO out of range | Check synth trim values |
| Low TX power | PA not biased | Verify factory trim loaded |
| High noise floor | LNA not enabled | Check RX frontend init |
| EVM degradation | Phase noise issue | Run phase noise test first |
