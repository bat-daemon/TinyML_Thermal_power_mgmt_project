#!/usr/bin/env python3
"""
Capture STM32 UART DATA rows into a validated CSV file.

Expected firmware row (17 fields after DATA, see DataRecord_t in logging.h):
DATA,timestamp_ms,power_mw,voltage_v,current_ma,temp_die_c,temp_rise_c,
     temp_ambient_c,workload_req,workload_eff,cpu_freq_hz,stack_used_bytes,
     heap_used_bytes,fan_pct,temp_risk,power_risk,rule_temp_risk,mode

Example:
    python capture_data.py --port COM5 --samples 1000
    python capture_data.py --port COM5 --samples 3600 --output data/run1.csv
    python capture_data.py --port COM5 --samples 14400 --output data/auto1.csv --send "MODE AUTO"

Install dependency:
    python -m pip install pyserial
"""

import argparse
import csv
import math
import sys
from datetime import datetime
from pathlib import Path

try:
    import serial
    from serial import SerialException
except ImportError:
    print(
        "Error: pyserial is not installed. Install it with:\n"
        "  python -m pip install pyserial",
        file=sys.stderr,
    )
    raise SystemExit(2)


CSV_FIELDS = [
    "timestamp_ms",
    "power_mw",
    "voltage_v",
    "current_ma",
    "temp_die_c",
    "temp_rise_c",
    "temp_ambient_c",
    "workload_req",
    "workload_eff",
    "cpu_freq_hz",
    "stack_used_bytes",
    "heap_used_bytes",
    "fan_pct",
    "temp_risk",
    "power_risk",
    "rule_temp_risk",
    "mode",
]
INT_FIELDS = (0,) + tuple(range(7, 17))
FLOAT_FIELDS = tuple(range(1, 7))

EXPECTED_DATA_FIELDS = len(CSV_FIELDS)
DEFAULT_BAUD = 115200


def parse_args():
    parser = argparse.ArgumentParser(
        description="Capture validated STM32 DATA, UART rows into CSV."
    )
    parser.add_argument(
        "--port",
        required=True,
        help="Serial port, e.g. COM5 on Windows or /dev/ttyACM0 on Linux.",
    )
    parser.add_argument(
        "--samples",
        type=int,
        required=True,
        help="Number of valid DATA rows to capture before stopping.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help=(
            "CSV output path. If omitted, a timestamped filename is created "
            "in the current directory. Existing files are appended to only "
            "when their header exactly matches the expected schema."
        ),
    )
    parser.add_argument(
        "--send",
        action="append",
        default=[],
        help='Command to send to the board before capturing, e.g. --send "MODE AUTO". Repeatable.',
    )
    parser.add_argument(
        "--baud",
        type=int,
        default=DEFAULT_BAUD,
        help=f"UART baud rate (default: {DEFAULT_BAUD}).",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=1.0,
        help="Serial read timeout in seconds (default: 1.0).",
    )
    return parser.parse_args()


def default_output_path():
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    return Path(f"stm32_capture_{stamp}.csv")


def prepare_output(path: Path):
    """
    Open a CSV safely:
    - Create a new file with a header if it does not exist.
    - Append only if an existing file has the exact expected header.
    - Refuse to overwrite or append to a mismatched schema.
    """
    path = path.expanduser()
    path.parent.mkdir(parents=True, exist_ok=True)

    if not path.exists():
        # Exclusive creation avoids overwriting a file created concurrently.
        handle = path.open("x", newline="", encoding="utf-8")
        writer = csv.writer(handle)
        writer.writerow(CSV_FIELDS)
        handle.flush()
        return handle, writer, 0

    if path.stat().st_size == 0:
        handle = path.open("a", newline="", encoding="utf-8")
        writer = csv.writer(handle)
        writer.writerow(CSV_FIELDS)
        handle.flush()
        return handle, writer, 0

    with path.open("r", newline="", encoding="utf-8-sig") as check:
        reader = csv.reader(check)
        existing_header = next(reader, None)
        if existing_header != CSV_FIELDS:
            raise ValueError(
                f"Refusing to append to '{path}': CSV header does not match "
                f"the expected {EXPECTED_DATA_FIELDS}-field schema.\n"
                f"Expected: {','.join(CSV_FIELDS)}\n"
                f"Found:    {','.join(existing_header or [])}\n"
                "Choose a new --output path or move the old file aside."
            )
        existing_rows = sum(1 for row in reader if row)

    handle = path.open("a", newline="", encoding="utf-8")
    return handle, csv.writer(handle), existing_rows


def parse_data_line(line: str):
    """
    Return a list of the data fields, or raise ValueError if malformed.
    Non-DATA lines should be filtered before calling this function.
    """
    try:
        row = next(csv.reader([line], skipinitialspace=False))
    except (csv.Error, StopIteration) as exc:
        raise ValueError(f"CSV parse error: {exc}") from exc

    if not row or row[0] != "DATA":
        raise ValueError("row does not begin with DATA")

    fields = row[1:]
    if len(fields) != EXPECTED_DATA_FIELDS:
        raise ValueError(
            f"expected {EXPECTED_DATA_FIELDS} data fields after DATA, "
            f"received {len(fields)}"
        )

    fields = [field.strip() for field in fields]
    if any(field == "" for field in fields):
        raise ValueError("one or more fields are empty")

    for index in INT_FIELDS:
        name = CSV_FIELDS[index]
        try:
            value = int(fields[index], 10)
        except ValueError as exc:
            raise ValueError(f"{name} is not an integer: {fields[index]!r}") from exc
        if value < 0:
            raise ValueError(f"{name} must be non-negative")
        fields[index] = str(value)
    # Floating-point measurement fields. Current/power are not forced positive,
    # because reversed sensor wiring can produce signed measurements.
    for index in FLOAT_FIELDS:
        name = CSV_FIELDS[index]
        try:
            value = float(fields[index])
        except ValueError as exc:
            raise ValueError(f"{name} is not numeric: {fields[index]!r}") from exc
        if not math.isfinite(value):
            raise ValueError(f"{name} must be finite, got {fields[index]!r}")
    return fields


def main():
    args = parse_args()

    if args.samples <= 0:
        print("Error: --samples must be greater than zero.", file=sys.stderr)
        return 2
    if args.baud <= 0:
        print("Error: --baud must be greater than zero.", file=sys.stderr)
        return 2
    if args.timeout <= 0:
        print("Error: --timeout must be greater than zero.", file=sys.stderr)
        return 2

    output_path = args.output if args.output is not None else default_output_path()

    try:
        output_handle, writer, previous_rows = prepare_output(output_path)
    except (OSError, ValueError) as exc:
        print(f"Output error: {exc}", file=sys.stderr)
        return 2

    valid_count = 0
    malformed_count = 0
    ignored_count = 0

    print(f"Port:   {args.port} @ {args.baud} baud")
    print(f"Output: {output_path.resolve()}")
    print(f"Target: {args.samples} valid samples")
    if previous_rows:
        print(f"Appending to existing CSV ({previous_rows} existing non-empty rows).")
    print("Waiting for DATA, rows... Press Ctrl+C to stop early.")

    try:
        with output_handle:
            with serial.Serial(
                port=args.port,
                baudrate=args.baud,
                timeout=args.timeout,
            ) as ser:
                # Discard stale bytes buffered before capture begins.
                ser.reset_input_buffer()
                for command in args.send:
                    ser.write((command.strip() + "\r\n").encode("ascii"))
                    ser.flush()
                    print(f"Sent: {command.strip()}")

                while valid_count < args.samples:
                    try:
                        raw = ser.readline()
                    except SerialException as exc:
                        print(f"\nSerial read error: {exc}", file=sys.stderr)
                        return 1

                    if not raw:
                        continue

                    line = raw.decode("utf-8", errors="replace").strip()
                    if not line.startswith("DATA,"):
                        ignored_count += 1
                        continue

                    try:
                        fields = parse_data_line(line)
                    except ValueError as exc:
                        malformed_count += 1
                        print(
                            f"Skipping malformed DATA row "
                            f"({malformed_count}): {exc} | {line!r}",
                            file=sys.stderr,
                        )
                        continue

                    writer.writerow(fields)
                    output_handle.flush()
                    valid_count += 1

                    print(
                        f"\rCaptured {valid_count}/{args.samples} valid samples "
                        f"(malformed DATA rows: {malformed_count})",
                        end="",
                        flush=True,
                    )

    except KeyboardInterrupt:
        print("\nCapture interrupted by user; saved rows were flushed.")
    except (SerialException, OSError) as exc:
        print(f"\nCapture error: {exc}", file=sys.stderr)
        return 1
    finally:
        print(
            f"\nFinished. Valid rows captured this run: {valid_count}; "
            f"malformed DATA rows skipped: {malformed_count}; "
            f"non-DATA lines ignored: {ignored_count}."
        )
        print(f"CSV: {output_path.resolve()}")

    return 0 if valid_count == args.samples else 1


if __name__ == "__main__":
    raise SystemExit(main())
