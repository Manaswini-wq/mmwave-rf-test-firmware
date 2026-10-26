#!/usr/bin/env python3
"""
RF Data Analyzer - Signal Processing and Analysis

Provides FFT-based spectral analysis, power measurement, phase noise
computation, EVM calculation, and noise figure estimation from captured
I/Q data or instrument trace data.
"""

import logging
import math
from typing import Optional, Tuple, List

import numpy as np
from scipy import signal as sp_signal

logger = logging.getLogger(__name__)


class RFDataAnalyzer:
    """Signal processing and RF measurement analysis engine."""

    # Boltzmann constant (J/K)
    K_BOLTZMANN = 1.380649e-23
    # Standard temperature (K)
    T0 = 290.0

    def __init__(self, sample_rate_hz: float = 12.5e6):
        self.sample_rate = sample_rate_hz

    def compute_fft(self, data_i: np.ndarray, data_q: np.ndarray,
                    window: str = "hann") -> Tuple[np.ndarray, np.ndarray]:
        """
        Compute FFT of complex I/Q data with windowing.

        Returns:
            freq_axis: Frequency array in Hz
            magnitude_db: Magnitude spectrum in dBFS
        """
        n = len(data_i)
        complex_data = data_i.astype(np.float64) + 1j * data_q.astype(np.float64)

        # Apply window
        if window == "hann":
            win = np.hanning(n)
        elif window == "blackman":
            win = np.blackman(n)
        elif window == "hamming":
            win = np.hamming(n)
        else:
            win = np.ones(n)

        windowed = complex_data * win

        # Coherent gain normalization
        coherent_gain = np.sum(win) / n

        # Compute FFT
        fft_data = np.fft.fftshift(np.fft.fft(windowed)) / (n * coherent_gain)

        freq_axis = np.fft.fftshift(np.fft.fftfreq(n, 1.0 / self.sample_rate))

        # Convert to dBFS (relative to full-scale = 32768 for 16-bit ADC)
        full_scale = 32768.0
        magnitude = np.abs(fft_data) / full_scale
        magnitude_db = 20.0 * np.log10(magnitude + 1e-20)

        return freq_axis, magnitude_db

    def measure_power_dbm(self, data_i: np.ndarray, data_q: np.ndarray,
                          adc_fs_dbm: float = 4.0) -> float:
        """
        Compute RMS power from I/Q samples in dBm.

        Args:
            data_i, data_q: I and Q sample arrays (16-bit integer range)
            adc_fs_dbm: ADC full-scale power in dBm
        """
        rms_sq = np.mean(data_i.astype(np.float64)**2 + data_q.astype(np.float64)**2)
        fs_sq = 32768.0 ** 2
        power_dbfs = 10.0 * np.log10(rms_sq / fs_sq + 1e-20)
        return float(power_dbfs + adc_fs_dbm)

    def measure_phase_noise(self, data_i: np.ndarray, data_q: np.ndarray,
                            offset_hz: float, rbw_hz: float = 0) -> float:
        """
        Measure single-sideband phase noise L(f) at given offset.

        Returns phase noise in dBc/Hz.
        """
        freq_axis, mag_db = self.compute_fft(data_i, data_q, window="hann")

        # Find carrier (maximum peak)
        carrier_idx = np.argmax(mag_db)
        carrier_power_db = mag_db[carrier_idx]

        # Find bin closest to offset
        bin_spacing = self.sample_rate / len(data_i)
        carrier_freq = freq_axis[carrier_idx]
        offset_freq = carrier_freq + offset_hz
        offset_idx = np.argmin(np.abs(freq_axis - offset_freq))

        noise_power_db = mag_db[offset_idx]

        # Phase noise: noise relative to carrier, normalized to 1 Hz BW
        effective_rbw = rbw_hz if rbw_hz > 0 else bin_spacing
        pn_dbc_hz = noise_power_db - carrier_power_db - 10.0 * np.log10(effective_rbw)

        return float(pn_dbc_hz)

    def measure_frequency_error(self, data_i: np.ndarray, data_q: np.ndarray,
                                 expected_if_hz: float = 0.0) -> float:
        """
        Measure frequency error using parabolic interpolation around FFT peak.

        Returns frequency error in Hz.
        """
        freq_axis, mag_db = self.compute_fft(data_i, data_q, window="hann")

        # Find peak with parabolic interpolation
        peak_idx = np.argmax(mag_db)
        if 0 < peak_idx < len(mag_db) - 1:
            alpha = mag_db[peak_idx - 1]
            beta = mag_db[peak_idx]
            gamma = mag_db[peak_idx + 1]

            delta = 0.5 * (alpha - gamma) / (alpha - 2 * beta + gamma + 1e-20)
            refined_freq = freq_axis[peak_idx] + delta * (freq_axis[1] - freq_axis[0])
        else:
            refined_freq = freq_axis[peak_idx]

        return float(refined_freq - expected_if_hz)

    def measure_evm(self, data_i: np.ndarray, data_q: np.ndarray) -> float:
        """
        Compute Error Vector Magnitude for a CW signal.

        Returns EVM in dB.
        """
        complex_data = data_i.astype(np.float64) + 1j * data_q.astype(np.float64)

        # Ideal carrier = mean phasor
        ideal = np.mean(complex_data)

        # Error vector
        error = complex_data - ideal

        signal_power = np.mean(np.abs(complex_data) ** 2)
        error_power = np.mean(np.abs(error) ** 2)

        evm_db = 10.0 * np.log10(error_power / (signal_power + 1e-20))
        return float(evm_db)

    def measure_noise_figure_yfactor(self, p_hot_linear: float,
                                      p_cold_linear: float,
                                      enr_db: float = 15.0) -> float:
        """
        Compute noise figure from Y-factor measurement.

        NF = ENR - 10*log10(Y - 1)
        where Y = P_hot / P_cold
        """
        if p_cold_linear <= 0 or p_hot_linear <= 0:
            return float("nan")

        y_factor = p_hot_linear / p_cold_linear
        if y_factor <= 1.0:
            return float("nan")

        nf_db = enr_db - 10.0 * math.log10(y_factor - 1.0)
        return nf_db

    def compute_spectrogram(self, data_i: np.ndarray, data_q: np.ndarray,
                             nperseg: int = 256) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
        """
        Compute spectrogram of I/Q data for time-frequency analysis.

        Returns:
            times, frequencies, power_db arrays
        """
        complex_data = data_i.astype(np.float64) + 1j * data_q.astype(np.float64)

        freqs, times, sxx = sp_signal.spectrogram(
            complex_data, fs=self.sample_rate,
            nperseg=nperseg, noverlap=nperseg // 2,
            return_onesided=False
        )

        power_db = 10.0 * np.log10(np.abs(sxx) + 1e-20)
        return times, freqs, power_db

    def compute_ccdf(self, data_i: np.ndarray, data_q: np.ndarray,
                      num_bins: int = 200) -> Tuple[np.ndarray, np.ndarray]:
        """
        Compute Complementary Cumulative Distribution Function (CCDF)
        of instantaneous power (for crest factor / PAR analysis).

        Returns:
            power_db: Power levels in dB relative to average
            probability: Probability of exceeding each power level
        """
        power = data_i.astype(np.float64)**2 + data_q.astype(np.float64)**2
        avg_power = np.mean(power)
        power_db_rel = 10.0 * np.log10(power / (avg_power + 1e-20) + 1e-20)

        db_range = np.linspace(-10, 15, num_bins)
        ccdf = np.array([np.mean(power_db_rel > threshold) for threshold in db_range])

        return db_range, ccdf
