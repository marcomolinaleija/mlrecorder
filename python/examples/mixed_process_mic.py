import argparse
import sys
import time
from pathlib import Path

CURRENT_DIR = Path(__file__).resolve().parent
PYTHON_DIR = CURRENT_DIR.parent
if str(PYTHON_DIR) not in sys.path:
    sys.path.insert(0, str(PYTHON_DIR))

from mlrecorder import (  # noqa: E402
    MLRecorderError,
    list_active_processes,
    list_microphones,
    shutdown,
    start_mixed_recorder,
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Record a mixed file from process audio + microphone.")
    parser.add_argument("--pid", type=int, required=True, help="Target process ID")
    parser.add_argument("--seconds", type=int, default=10, help="Duration in seconds")
    parser.add_argument("--output-dir", type=str, default="recordings", help="Output directory")
    parser.add_argument("--format", choices=["wav", "mp3", "opus", "flac"], default="wav")
    parser.add_argument("--bitrate", type=int, default=0, help="Bitrate (or FLAC compression level)")
    parser.add_argument("--no-microphone", action="store_true", help="Mix process audio only")
    parser.add_argument("--microphone-id", type=str, default=None, help="Specific input device ID")
    parser.add_argument("--list-active", action="store_true", help="Show active-audio processes and exit")
    parser.add_argument("--list-mics", action="store_true", help="Show input devices and exit")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.list_active:
            for p in list_active_processes():
                print(f"PID={p.process_id:>6}  {p.process_name}")
            return 0

        if args.list_mics:
            for m in list_microphones():
                mark = " (default)" if m.is_default else ""
                print(f"{m.device_id} | {m.friendly_name}{mark}")
            return 0

        session = start_mixed_recorder(
            pid=args.pid,
            output_dir=args.output_dir,
            fmt=args.format,
            bitrate=args.bitrate,
            include_microphone=not args.no_microphone,
            input_device_id=args.microphone_id,
        )
        print("Mixed recording started...")
        time.sleep(max(1, args.seconds))
        session.stop()
        print("Mixed recording finished.")
        return 0
    except MLRecorderError as exc:
        print(f"MLRecorder error: {exc}")
        return 1
    finally:
        shutdown()


if __name__ == "__main__":
    raise SystemExit(main())
