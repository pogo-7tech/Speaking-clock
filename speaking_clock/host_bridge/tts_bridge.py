#!/usr/bin/env python3
"""
tts_bridge.py
-------------
Host-side companion for the STM32 speaking-clock firmware.

The firmware emits lines of the form
        TOKEN <word>
        ...
        END
over its UART (which QEMU routes to stdout).  This bridge
listens on stdin (or on a serial port), reassembles one
utterance per "END" marker, converts digits to English words,
and feeds the resulting sentence to eSpeak.

Usage
-----
# Pipe QEMU's output into the bridge:
    make run | python3 host_bridge/tts_bridge.py

# OR, if you are using a real serial link:
    python3 host_bridge/tts_bridge.py --port /dev/ttyUSB0 --baud 115200

Requires:
    - espeak-ng or espeak in PATH
    - (optional) pyserial if --port is used
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from typing import List, Optional


# ---------------------------------------------------------------------------
#  Number-to-word conversion (0-59 covers hours / minutes / seconds)
# ---------------------------------------------------------------------------
_UNITS = [
    "zero",  "one",    "two",      "three",    "four",
    "five",  "six",    "seven",    "eight",    "nine",
    "ten",   "eleven", "twelve",   "thirteen", "fourteen",
    "fifteen", "sixteen", "seventeen", "eighteen", "nineteen",
]
_TENS = [
    "", "", "twenty", "thirty", "forty",
    "fifty", "sixty", "seventy", "eighty", "ninety",
]


def number_to_words(n: int) -> str:
    """Convert 0..99 to English words."""
    if n < 0 or n > 99:
        return str(n)
    if n < 20:
        return _UNITS[n]
    tens, units = divmod(n, 10)
    if units == 0:
        return _TENS[tens]
    return f"{_TENS[tens]} {_UNITS[units]}"


def token_to_text(tok: str) -> str:
    """Translate a single TOKEN payload to a spoken word / phrase."""
    tok = tok.strip().upper()

    # Numeric token?
    if tok.isdigit():
        return number_to_words(int(tok))

    # Fixed vocabulary - just lowercase.
    return tok.lower()


# ---------------------------------------------------------------------------
#  eSpeak wrapper
# ---------------------------------------------------------------------------
def find_espeak() -> Optional[str]:
    for name in ("espeak-ng", "espeak"):
        path = shutil.which(name)
        if path:
            return path
    return None


def speak(text: str, engine: str, *, rate: int = 160, voice: str = "en") -> None:
    if not text:
        return
    print(f"[tts] -> \"{text}\"", file=sys.stderr, flush=True)
    try:
        subprocess.run(
            [engine, "-s", str(rate), "-v", voice, text],
            check=False,
        )
    except FileNotFoundError:
        print(f"[tts] ERROR: engine '{engine}' not found", file=sys.stderr)


# ---------------------------------------------------------------------------
#  Main stream processor
# ---------------------------------------------------------------------------
def run_bridge(stream, engine: str) -> None:
    buffer: List[str] = []

    for raw in stream:
        line = raw.strip()
        if not line:
            continue

        if line == "END":
            if buffer:
                speak(" ".join(buffer), engine)
                buffer = []
            continue

        if line.startswith("TOKEN "):
            payload = line[len("TOKEN "):].strip()
            buffer.append(token_to_text(payload))
        else:
            # Forward firmware debug/trace lines to the console.
            print(line, file=sys.stdout, flush=True)


# ---------------------------------------------------------------------------
#  CLI entry-point
# ---------------------------------------------------------------------------
def main() -> int:
    ap = argparse.ArgumentParser(description="TTS bridge for speaking-clock")
    ap.add_argument("--port", help="serial device (default: read stdin)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--engine", default=None,
                    help="path to espeak/espeak-ng (auto-detected)")
    args = ap.parse_args()

    engine = args.engine or find_espeak()
    if engine is None:
        print("WARNING: espeak not found; will just print utterances.",
              file=sys.stderr)
        engine = "echo"   # still testable

    if args.port:
        try:
            import serial  # type: ignore
        except ImportError:
            print("pyserial is required for --port mode", file=sys.stderr)
            return 2
        with serial.Serial(args.port, args.baud, timeout=None) as ser:
            stream = (ser.readline().decode("ascii", "ignore")
                      for _ in iter(int, 1))
            run_bridge(stream, engine)
    else:
        run_bridge(sys.stdin, engine)

    return 0


if __name__ == "__main__":
    sys.exit(main())
