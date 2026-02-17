import argparse
import sys
import time
from pathlib import Path

CURRENT_DIR = Path(__file__).resolve().parent
PYTHON_DIR = CURRENT_DIR.parent
if str(PYTHON_DIR) not in sys.path:
    sys.path.insert(0, str(PYTHON_DIR))

from mlrecorder import (  # noqa: E402
    FORMAT_FLAC,
    FORMAT_MP3,
    FORMAT_OPUS,
    FORMAT_WAV,
    MLRecorder,
    MLRecorderError,
)


FORMAT_MAP = {
    "wav": FORMAT_WAV,
    "mp3": FORMAT_MP3,
    "opus": FORMAT_OPUS,
    "flac": FORMAT_FLAC,
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Record isolated audio from a target process PID using mlrecorder_core.dll."
    )
    parser.add_argument("--pid", type=int, required=True, help="Target process ID")
    parser.add_argument(
        "--seconds",
        type=int,
        default=10,
        help="Duration in seconds to record (default: 10)",
    )
    parser.add_argument(
        "--output-dir",
        type=str,
        default=str((Path.cwd() / "recordings").resolve()),
        help="Directory where output file will be generated",
    )
    parser.add_argument(
        "--format",
        choices=sorted(FORMAT_MAP.keys()),
        default="wav",
        help="Output format (default: wav)",
    )
    parser.add_argument(
        "--bitrate",
        type=int,
        default=0,
        help="Bitrate for MP3/Opus or FLAC compression level (0 uses library defaults)",
    )
    parser.add_argument(
        "--skip-silence",
        action="store_true",
        help="Skip silent chunks when writing output",
    )
    parser.add_argument(
        "--no-strict-isolation",
        action="store_true",
        help="Allow fallback to system-wide capture if process loopback is unavailable",
    )
    parser.add_argument(
        "--dll",
        type=str,
        default=None,
        help="Path to mlrecorder_core.dll (optional if MLRECORDER_DLL env var is set)",
    )
    parser.add_argument(
        "--list-active",
        action="store_true",
        help="Print active-audio process list before recording",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    fmt = FORMAT_MAP[args.format]
    strict = not args.no_strict_isolation

    try:
        with MLRecorder(args.dll) as recorder:
            if args.list_active:
                print("Active-audio processes:")
                for proc in recorder.list_processes(only_active_audio=True):
                    print(f"  PID={proc.process_id:>6} name={proc.process_name}")

            all_processes = recorder.list_processes(only_active_audio=False)
            proc_map = {p.process_id: p for p in all_processes}
            if args.pid not in proc_map:
                print(f"PID {args.pid} not found in process list.")
                return 2

            proc = proc_map[args.pid]
            print(f"Target PID={proc.process_id} name={proc.process_name}")
            if proc.window_title:
                print(f"Window: {proc.window_title}")

            print(f"Starting isolated capture for {args.seconds}s...")
            recorder.start_capture_to_directory(
                process_id=args.pid,
                output_dir=args.output_dir,
                fmt=fmt,
                bitrate=args.bitrate,
                skip_silence=args.skip_silence,
                monitor_only=False,
                strict_process_isolation=strict,
            )

            try:
                time.sleep(max(1, args.seconds))
            finally:
                recorder.stop_capture(args.pid)

            print("Capture completed successfully.")
            print(f"Output directory: {args.output_dir}")
            return 0
    except MLRecorderError as exc:
        print(f"MLRecorder error: {exc}")
        return 1
    except KeyboardInterrupt:
        print("Interrupted.")
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
