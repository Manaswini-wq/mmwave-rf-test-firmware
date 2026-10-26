#!/usr/bin/env python3
"""
SCPI Instrument Control for RF Test Equipment

Provides classes for controlling spectrum analyzers and signal generators
via VISA/SCPI for automated RF production testing.  Supports common
instruments from Keysight, R&S, and Tektronix.
"""

import time
import logging
from typing import Optional, List, Tuple

logger = logging.getLogger(__name__)

try:
    import pyvisa
    VISA_AVAILABLE = True
except ImportError:
    VISA_AVAILABLE = False
    logger.warning("pyvisa not available; instrument control will be simulated")


class SCPIInstrument:
    """Base class for SCPI-controlled instruments."""

    def __init__(self, visa_address: str, timeout_ms: int = 10000):
        self.visa_address = visa_address
        self.timeout_ms = timeout_ms
        self._resource = None
        self._rm = None
        self._simulated = not VISA_AVAILABLE

    def connect(self):
        """Establish VISA connection to instrument."""
        if self._simulated:
            logger.info(f"[SIM] Connected to {self.visa_address}")
            return

        self._rm = pyvisa.ResourceManager()
        self._resource = self._rm.open_resource(self.visa_address)
        self._resource.timeout = self.timeout_ms

        # Query identity
        idn = self.query("*IDN?")
        logger.info(f"Connected to: {idn.strip()}")

    def disconnect(self):
        """Close VISA connection."""
        if self._resource:
            self._resource.close()
        if self._rm:
            self._rm.close()
        logger.info(f"Disconnected from {self.visa_address}")

    def write(self, command: str):
        """Send SCPI command."""
        logger.debug(f"SCPI TX: {command}")
        if self._simulated:
            return
        self._resource.write(command)

    def query(self, command: str) -> str:
        """Send SCPI query and return response."""
        logger.debug(f"SCPI QRY: {command}")
        if self._simulated:
            return self._simulated_response(command)
        response = self._resource.query(command)
        logger.debug(f"SCPI RX: {response.strip()}")
        return response

    def query_float(self, command: str) -> float:
        """Send query and parse as float."""
        resp = self.query(command)
        try:
            return float(resp.strip())
        except ValueError:
            logger.error(f"Failed to parse float from: {resp}")
            return float("nan")

    def reset(self):
        """Reset instrument to default state."""
        self.write("*RST")
        self.write("*CLS")
        time.sleep(1.0)

    def wait_complete(self, timeout_s: float = 30.0):
        """Wait for pending operations to complete."""
        self.write("*OPC")
        start = time.time()
        while time.time() - start < timeout_s:
            try:
                resp = self.query("*OPC?")
                if resp.strip() == "1":
                    return
            except Exception:
                pass
            time.sleep(0.1)
        raise TimeoutError("Instrument operation timed out")

    def _simulated_response(self, command: str) -> str:
        """Provide simulated responses for testing without hardware."""
        if command == "*IDN?":
            return "Simulated Instrument,Model,Serial,1.0"
        elif "MARK" in command and "Y?" in command:
            return "-45.2"
        elif "FREQ" in command and "?" in command:
            return "77000000000"
        elif "POW" in command and "?" in command:
            return "-10.0"
        return "0"


class SpectrumAnalyzer(SCPIInstrument):
    """Spectrum Analyzer control for RF power and phase noise measurements."""

    def configure_span(self, center_hz: float, span_hz: float, rbw_hz: float = 0):
        """Configure frequency span."""
        self.write(f":FREQ:CENT {center_hz:.0f}")
        self.write(f":FREQ:SPAN {span_hz:.0f}")
        if rbw_hz > 0:
            self.write(f":BAND:RES {rbw_hz:.0f}")
        else:
            self.write(":BAND:RES:AUTO ON")

    def configure_for_power_measurement(self, freq_hz: float, span_hz: float = 1e6):
        """Configure SA for TX power measurement."""
        self.reset()
        self.write(":INST:SEL SA")
        self.configure_span(freq_hz, span_hz)
        self.write(":DET:FUNC RMS")
        self.write(":AVER:COUN 10")
        self.write(":AVER ON")
        self.write(f":CALC:MARK1:X {freq_hz:.0f}")
        self.write(":INIT:CONT OFF")

    def configure_for_phase_noise(self, carrier_hz: float):
        """Configure SA for phase noise measurement at carrier frequency."""
        self.reset()
        self.write(":INST:SEL SA")
        # Use log-frequency (phase noise) measurement mode if available
        self.configure_span(carrier_hz, 20e6, rbw_hz=1000)
        self.write(":DET:FUNC SAMP")
        self.write(":AVER:COUN 20")
        self.write(":AVER ON")

    def measure_peak_power(self) -> float:
        """Trigger sweep and measure peak power in dBm."""
        self.write(":INIT:IMM")
        self.wait_complete()
        self.write(":CALC:MARK1:MAX")
        return self.query_float(":CALC:MARK1:Y?")

    def measure_marker_power(self, marker: int = 1) -> float:
        """Read marker power level in dBm."""
        return self.query_float(f":CALC:MARK{marker}:Y?")

    def measure_channel_power(self, bw_hz: float = 1e6) -> float:
        """Measure integrated channel power."""
        self.write(":MEAS:CHP")
        self.write(f":CHP:BAND:INT {bw_hz:.0f}")
        self.write(":INIT:IMM")
        self.wait_complete()
        return self.query_float(":FETCH:CHP:CHP?")

    def measure_phase_noise(self, offset_hz: float) -> float:
        """Measure phase noise at given offset from carrier."""
        self.write(":INIT:IMM")
        self.wait_complete()

        # Place delta marker at offset
        self.write(":CALC:MARK1:MAX")
        carrier_freq = self.query_float(":CALC:MARK1:X?")
        self.write(f":CALC:MARK2:X {carrier_freq + offset_hz:.0f}")

        carrier_power = self.query_float(":CALC:MARK1:Y?")
        noise_power = self.query_float(":CALC:MARK2:Y?")

        # Normalize to 1 Hz BW
        rbw = self.query_float(":BAND:RES?")
        pn_dbc_hz = noise_power - carrier_power - 10 * __import__("math").log10(rbw)

        return pn_dbc_hz

    def get_trace_data(self) -> Tuple[List[float], List[float]]:
        """Read trace data (frequency and power arrays)."""
        self.write(":FORM:TRAC:DATA REAL,32")

        freq_start = self.query_float(":FREQ:STAR?")
        freq_stop = self.query_float(":FREQ:STOP?")

        power_data_str = self.query(":TRAC:DATA? TRACE1")
        powers = [float(x) for x in power_data_str.strip().split(",")]
        num_pts = len(powers)

        freqs = [
            freq_start + i * (freq_stop - freq_start) / (num_pts - 1)
            for i in range(num_pts)
        ]

        return freqs, powers


class SignalGenerator(SCPIInstrument):
    """Signal Generator control for RX sensitivity and NF measurements."""

    def configure_cw(self, freq_hz: float, power_dbm: float):
        """Configure CW output."""
        self.reset()
        self.write(f":FREQ:FIX {freq_hz:.0f}")
        self.write(f":POW:LEV {power_dbm:.1f}")
        self.write(":MOD:STAT OFF")

    def set_frequency(self, freq_hz: float):
        """Set output frequency."""
        self.write(f":FREQ:FIX {freq_hz:.0f}")

    def set_power(self, power_dbm: float):
        """Set output power level."""
        self.write(f":POW:LEV {power_dbm:.1f}")

    def rf_on(self):
        """Enable RF output."""
        self.write(":OUTP ON")
        logger.info("Signal generator RF ON")

    def rf_off(self):
        """Disable RF output."""
        self.write(":OUTP OFF")
        logger.info("Signal generator RF OFF")

    def configure_noise_source(self, enr_db: float = 15.0):
        """Configure for noise figure measurement (noise source mode)."""
        self.write(":SOUR:NOIS:STAT ON")
        self.write(f":SOUR:NOIS:ENR {enr_db:.1f}")

    def set_modulation(self, mod_type: str = "CW"):
        """Set modulation type."""
        if mod_type == "CW":
            self.write(":MOD:STAT OFF")
        elif mod_type == "AM":
            self.write(":AM:STAT ON")
            self.write(":AM:DEPT 50")
        elif mod_type == "FM":
            self.write(":FM:STAT ON")
            self.write(":FM:DEV 100000")

    def sweep_frequency(self, start_hz: float, stop_hz: float,
                        num_points: int = 100, dwell_ms: float = 10):
        """Configure frequency sweep."""
        self.write(f":FREQ:STAR {start_hz:.0f}")
        self.write(f":FREQ:STOP {stop_hz:.0f}")
        self.write(f":SWE:POIN {num_points}")
        self.write(f":SWE:DWEL {dwell_ms / 1000:.6f}")
        self.write(":FREQ:MODE SWE")

    def get_frequency(self) -> float:
        """Query current output frequency."""
        return self.query_float(":FREQ:FIX?")

    def get_power(self) -> float:
        """Query current output power."""
        return self.query_float(":POW:LEV?")
