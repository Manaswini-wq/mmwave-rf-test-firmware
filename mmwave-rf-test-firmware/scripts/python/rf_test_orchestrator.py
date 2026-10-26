#!/usr/bin/env python3
"""
RF Test Orchestrator - Production Test Automation for mmWave Devices

Orchestrates the full production test sequence by communicating with the
firmware via UART, controlling external instruments via SCPI, capturing
measurement data, performing pass/fail analysis, and generating reports.

Usage:
    python rf_test_orchestrator.py --port COM3 \
        --profile ../../config/test_profiles/production_test.json \
        --report output/test_report.html
"""

import json
import time
import logging
import argparse
import sys
from pathlib import Path
from datetime import datetime
from dataclasses import dataclass, field, asdict
from typing import List, Optional, Dict, Any

import serial

# Local imports
from instrument_control import SpectrumAnalyzer, SignalGenerator
from data_analyzer import RFDataAnalyzer
from report_generator import ReportGenerator

logger = logging.getLogger(__name__)


@dataclass
class TestResult:
    """Single test result entry."""
    test_name: str
    channel: int
    frequency_ghz: float
    measured_value: float
    lower_limit: float
    upper_limit: float
    unit: str
    result: str  # PASS, FAIL, MARGINAL
    timestamp: str = ""
    duration_ms: float = 0.0
    raw_data: Optional[Dict[str, Any]] = None

    def __post_init__(self):
        if not self.timestamp:
            self.timestamp = datetime.now().isoformat()


@dataclass
class TestSession:
    """Complete test session with metadata and results."""
    session_id: str
    device_serial: str = ""
    operator: str = ""
    station_id: str = ""
    start_time: str = ""
    end_time: str = ""
    firmware_version: str = ""
    profile_name: str = ""
    results: List[TestResult] = field(default_factory=list)
    overall_result: str = "PENDING"

    @property
    def pass_count(self) -> int:
        return sum(1 for r in self.results if r.result == "PASS")

    @property
    def fail_count(self) -> int:
        return sum(1 for r in self.results if r.result == "FAIL")

    @property
    def yield_pct(self) -> float:
        if not self.results:
            return 0.0
        return 100.0 * self.pass_count / len(self.results)


class DUTConnection:
    """Manages UART connection to the Device Under Test (DUT)."""

    def __init__(self, port: str, baud_rate: int = 921600, timeout: float = 5.0):
        self.port = port
        self.baud_rate = baud_rate
        self.timeout = timeout
        self._serial: Optional[serial.Serial] = None

    def connect(self):
        """Open serial connection to DUT."""
        logger.info(f"Connecting to DUT on {self.port} @ {self.baud_rate} baud")
        self._serial = serial.Serial(
            port=self.port,
            baudrate=self.baud_rate,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=self.timeout,
        )
        time.sleep(0.5)  # Wait for device reset
        self._flush()
        logger.info("DUT connected successfully")

    def disconnect(self):
        """Close serial connection."""
        if self._serial and self._serial.is_open:
            self._serial.close()
            logger.info("DUT disconnected")

    def send_command(self, cmd: str, wait_response: bool = True) -> str:
        """Send command and optionally wait for response."""
        if not self._serial or not self._serial.is_open:
            raise ConnectionError("DUT not connected")

        self._flush()
        full_cmd = f"{cmd}\r\n"
        self._serial.write(full_cmd.encode("ascii"))
        logger.debug(f"TX: {cmd}")

        if wait_response:
            return self._read_response()
        return ""

    def _read_response(self) -> str:
        """Read response until prompt character '>' or timeout."""
        response_lines = []
        deadline = time.time() + self.timeout

        while time.time() < deadline:
            line = self._serial.readline().decode("ascii", errors="replace").strip()
            if line:
                response_lines.append(line)
                logger.debug(f"RX: {line}")
            if line.startswith(">") or line.endswith(">"):
                break

        return "\n".join(response_lines)

    def _flush(self):
        """Flush input buffer."""
        if self._serial:
            self._serial.reset_input_buffer()

    def get_firmware_version(self) -> str:
        """Query firmware version from DUT banner."""
        resp = self.send_command("status")
        for line in resp.split("\n"):
            if "version" in line.lower() or "v" in line:
                return line.strip()
        return "unknown"


class RFTestOrchestrator:
    """Main orchestrator for RF production test sequences."""

    def __init__(self, config_path: str, port: str, report_path: str = ""):
        self.config = self._load_config(config_path)
        self.dut = DUTConnection(port, self.config.get("uart_baud_rate", 921600))
        self.report_path = report_path or "output/test_report.html"
        self.session: Optional[TestSession] = None

        # Initialize external instruments if configured
        self.spectrum_analyzer: Optional[SpectrumAnalyzer] = None
        self.signal_generator: Optional[SignalGenerator] = None
        self.analyzer = RFDataAnalyzer()
        self.reporter = ReportGenerator()

    @staticmethod
    def _load_config(path: str) -> dict:
        """Load test profile configuration from JSON."""
        config_path = Path(path)
        if not config_path.exists():
            logger.error(f"Config file not found: {path}")
            sys.exit(1)

        with open(config_path, "r") as f:
            config = json.load(f)

        logger.info(f"Loaded test profile: {config.get('profile_name', 'unknown')}")
        return config

    def _init_instruments(self):
        """Initialize external test instruments if configured."""
        instr_cfg = self.config.get("instruments", {})

        sa_addr = instr_cfg.get("spectrum_analyzer", {}).get("visa_address")
        if sa_addr:
            self.spectrum_analyzer = SpectrumAnalyzer(sa_addr)
            self.spectrum_analyzer.connect()
            logger.info(f"Spectrum analyzer connected: {sa_addr}")

        sg_addr = instr_cfg.get("signal_generator", {}).get("visa_address")
        if sg_addr:
            self.signal_generator = SignalGenerator(sg_addr)
            self.signal_generator.connect()
            logger.info(f"Signal generator connected: {sg_addr}")

    def run(self):
        """Execute the complete test sequence."""
        session_id = datetime.now().strftime("TST-%Y%m%d-%H%M%S")
        self.session = TestSession(
            session_id=session_id,
            profile_name=self.config.get("profile_name", ""),
            start_time=datetime.now().isoformat(),
        )

        try:
            # Phase 1: Connect to DUT
            self.dut.connect()
            self.session.firmware_version = self.dut.get_firmware_version()

            # Phase 2: Initialize instruments
            self._init_instruments()

            # Phase 3: Run configured test sequence
            test_sequence = self.config.get("test_sequence", [])
            total_tests = len(test_sequence)

            logger.info(f"Starting test sequence: {total_tests} tests")

            for idx, test_cfg in enumerate(test_sequence, 1):
                test_name = test_cfg.get("name", "unknown")
                logger.info(f"[{idx}/{total_tests}] Running: {test_name}")

                result = self._execute_single_test(test_cfg)
                self.session.results.append(result)

                logger.info(
                    f"  Result: {result.result} "
                    f"({result.measured_value:.2f} {result.unit})"
                )

            # Phase 4: Determine overall result
            self.session.end_time = datetime.now().isoformat()
            self.session.overall_result = (
                "PASS" if self.session.fail_count == 0 else "FAIL"
            )

            # Phase 5: Generate report
            self._generate_report()

            logger.info(
                f"Test complete: {self.session.overall_result} "
                f"({self.session.pass_count}/{len(self.session.results)} passed, "
                f"yield: {self.session.yield_pct:.1f}%)"
            )

        except Exception as e:
            logger.error(f"Test sequence failed: {e}")
            if self.session:
                self.session.overall_result = "ERROR"
            raise

        finally:
            self.dut.disconnect()

    def _execute_single_test(self, test_cfg: dict) -> TestResult:
        """Execute a single test and return the result."""
        test_name = test_cfg["name"]
        channel = test_cfg.get("channel", 0)
        freq_ghz = test_cfg.get("frequency_ghz", 62.0)
        limits = test_cfg.get("limits", {})

        # Build firmware command
        cmd = f"test {test_name} --channel {channel} --freq {freq_ghz}"

        start_time = time.time()
        response = self.dut.send_command(cmd)
        elapsed_ms = (time.time() - start_time) * 1000.0

        # Parse response for measured value
        measured = self._parse_measurement(response)

        # Determine pass/fail
        lower = limits.get("lower", float("-inf"))
        upper = limits.get("upper", float("inf"))
        unit = limits.get("unit", "dB")

        if lower <= measured <= upper:
            result_str = "PASS"
        else:
            result_str = "FAIL"

        return TestResult(
            test_name=test_name,
            channel=channel,
            frequency_ghz=freq_ghz,
            measured_value=measured,
            lower_limit=lower,
            upper_limit=upper,
            unit=unit,
            result=result_str,
            duration_ms=elapsed_ms,
        )

    @staticmethod
    def _parse_measurement(response: str) -> float:
        """Parse measured value from firmware response."""
        for line in response.split("\n"):
            # Look for patterns like "Measured Power: 11.2 dBm" or "VALUE: 11.2"
            for keyword in ["measured", "value", "result"]:
                if keyword.lower() in line.lower():
                    tokens = line.split(":")
                    if len(tokens) >= 2:
                        try:
                            num_str = tokens[-1].strip().split()[0]
                            return float(num_str)
                        except (ValueError, IndexError):
                            continue
        return float("nan")

    def _generate_report(self):
        """Generate HTML test report."""
        if self.session:
            output_path = Path(self.report_path)
            output_path.parent.mkdir(parents=True, exist_ok=True)

            report_data = {
                "session": asdict(self.session),
                "summary": {
                    "total": len(self.session.results),
                    "passed": self.session.pass_count,
                    "failed": self.session.fail_count,
                    "yield_pct": self.session.yield_pct,
                },
            }

            self.reporter.generate_html(report_data, str(output_path))
            logger.info(f"Report saved: {output_path}")


def main():
    """CLI entry point."""
    parser = argparse.ArgumentParser(
        description="mmWave RF Test Orchestrator"
    )
    parser.add_argument(
        "--port", required=True, help="Serial port (e.g., COM3 or /dev/ttyUSB0)"
    )
    parser.add_argument(
        "--profile", required=True, help="Path to test profile JSON"
    )
    parser.add_argument(
        "--report", default="output/test_report.html",
        help="Output report file path"
    )
    parser.add_argument(
        "--log-level", default="INFO",
        choices=["DEBUG", "INFO", "WARNING", "ERROR"],
    )
    parser.add_argument(
        "--serial-number", default="", help="Device serial number"
    )

    args = parser.parse_args()

    logging.basicConfig(
        level=getattr(logging, args.log_level),
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    )

    orchestrator = RFTestOrchestrator(
        config_path=args.profile,
        port=args.port,
        report_path=args.report,
    )
    orchestrator.run()


if __name__ == "__main__":
    main()
