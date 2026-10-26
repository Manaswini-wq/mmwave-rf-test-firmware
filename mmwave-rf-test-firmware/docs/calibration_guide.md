# Calibration Guide

## Overview

This guide describes the calibration algorithms implemented in the mmWave RF test firmware. Calibration compensates for manufacturing variations in the analog RF components to ensure each device meets specification.

## Calibration Procedures

### 1. TX Power Flatness Calibration

**Purpose:** Achieve uniform TX output power across the operating bandwidth.

**Algorithm:**
1. Initialize all per-frequency DAC codes to a nominal value
2. Measure TX power at N frequency points across the band
3. Compute mean power and per-point deviation
4. Adjust DAC codes proportionally to reduce deviation
5. Repeat until peak-to-peak ripple < 0.5 dB or max iterations reached

**Parameters:**
- Frequency points: 20 (evenly spaced across band)
- Target ripple: < 0.5 dB peak-to-peak
- Max iterations: 20
- DAC step size: 1 code per iteration

**Storage:** Calibration coefficients are stored in the `g_cal_coeffs` array indexed by [channel][frequency_point].

### 2. RX Gain/Phase Calibration

**Purpose:** Align gain and phase response across all RX channels for accurate beamforming.

**Algorithm:**
1. Enable TX loopback tone to illuminate all RX channels
2. Capture I/Q data on each channel
3. Compute mean phasor (magnitude + phase) per channel
4. Calculate gain and phase offsets relative to reference channel (CH0)
5. Apply PGA gain corrections and phase-shift register adjustments
6. Verify convergence

**Tolerances:**
- Gain match: < 0.3 dB between channels
- Phase match: < 2.0 degrees between channels

### 3. LO Leakage Nulling

**Purpose:** Minimize LO feedthrough appearing as DC in the baseband signal.

**Algorithm (Successive Approximation):**
1. Set initial search range for I and Q DC offset DACs (range = 64 codes)
2. Binary search on I-axis: try center +/- step, keep lower DC
3. Binary search on Q-axis: same procedure
4. Fine gradient descent: try all 4 neighbors, keep minimum
5. Verify final LO leakage level

**Target:** LO leakage < -40 dBc after calibration

### 4. Inter-Channel Calibration

**Purpose:** Align timing, gain, and phase across RX channels for direction-of-arrival accuracy.

**Algorithm:**
1. Inject reference signal via TX loopback
2. Capture on all RX channels simultaneously
3. Cross-correlate each channel with reference to find timing offset
4. Use parabolic interpolation for sub-sample timing resolution
5. Compute gain and phase differences from mean phasor analysis
6. Apply corrections to DMA timing registers and phase registers

**Tolerances:**
- Timing: < 50 ps between channels
- Gain: < 0.2 dB between channels
- Phase: < 1.0 degree between channels

## Running Calibrations

### Via UART Console
```
cal tx_flatness --channel 0
cal rx_gp --channel 0
cal lo_null --channel 0
cal inter_ch
```

### Calibration Sequence
For a complete device calibration, run in this order:
1. LO Leakage Nulling (all RX channels)
2. RX Gain/Phase Calibration
3. Inter-Channel Calibration
4. TX Power Flatness Calibration (all TX channels)

## Calibration Data Management

Calibration results are stored in RAM during the session. For persistent storage, the host can read calibration coefficients via the `results` command and save them to a file. On subsequent sessions, coefficients can be loaded via the test protocol.

## Verification

After calibration, run the following tests to verify:
- TX power test (should show reduced channel variation)
- RX noise floor test (should not degrade)
- Phase noise test (should not change significantly)
