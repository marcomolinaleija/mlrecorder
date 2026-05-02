"""
Demostración de pause/resume para mlrecorder.

Uso:
    python pause_resume_demo.py [--pid PID] [--mic] [--format wav|mp3|opus|flac]

Sin argumentos: lista los procesos con audio activo y pide elegir uno.
Con --mic: también graba el micrófono por separado y demuestra su pausa.
"""

import argparse
import sys
import time

import mlrecorder


def list_and_choose_process() -> int:
    print("\nProcesos con audio activo:")
    processes = mlrecorder.list_active_processes()
    if not processes:
        print("  (ninguno encontrado)")
        sys.exit(1)

    for i, p in enumerate(processes):
        label = p.window_title or p.process_name
        print(f"  [{i}] PID {p.process_id:>6}  {label}")

    while True:
        raw = input("\nElige número (o q para salir): ").strip()
        if raw.lower() == "q":
            sys.exit(0)
        try:
            idx = int(raw)
            if 0 <= idx < len(processes):
                return processes[idx].process_id
        except ValueError:
            pass
        print("Opción inválida, intenta de nuevo.")


def demo_process(pid: int, fmt: str) -> None:
    print(f"\n--- Grabando PID {pid} en formato {fmt.upper()} ---")
    session = mlrecorder.start_recorder(pid, output_dir="recordings_demo", fmt=fmt)
    print(f"  Grabando... (is_paused={session.is_paused()}, is_recording={session.is_recording()})")

    time.sleep(3)

    print("  [PAUSE]")
    session.pause()
    print(f"  Pausado   (is_paused={session.is_paused()}, is_recording={session.is_recording()})")

    time.sleep(2)

    print("  [RESUME]")
    session.resume()
    print(f"  Reanudado (is_paused={session.is_paused()}, is_recording={session.is_recording()})")

    time.sleep(3)

    print("  [STOP]")
    session.stop()
    print(f"  Detenido  (is_recording={session.is_recording()})")


def demo_microphone(fmt: str) -> None:
    mics = mlrecorder.list_microphones()
    if not mics:
        print("\n  No se encontraron micrófonos, omitiendo demo de micrófono.")
        return

    default = next((m for m in mics if m.is_default), mics[0])
    print(f"\n--- Grabando micrófono: {default.friendly_name} ---")

    session = mlrecorder.start_microphone_recorder(output_dir="recordings_demo", fmt=fmt)
    print(f"  Grabando... (is_paused={session.is_paused()})")

    time.sleep(2)

    print("  [PAUSE]")
    session.pause()
    print(f"  Pausado   (is_paused={session.is_paused()})")

    time.sleep(1.5)

    print("  [RESUME]")
    session.resume()
    print(f"  Reanudado (is_paused={session.is_paused()})")

    time.sleep(2)

    print("  [STOP]")
    session.stop()


def demo_double_pause(pid: int) -> None:
    """Verifica que llamar pause dos veces seguidas no explota."""
    print("\n--- Prueba: pause doble y resume doble ---")
    session = mlrecorder.start_recorder(pid, output_dir="recordings_demo", fmt="wav")
    time.sleep(0.5)

    session.pause()
    assert session.is_paused(), "Debería estar pausado"
    session.pause()  # segunda llamada — debe ser no-op
    assert session.is_paused(), "Sigue pausado tras doble pause"

    session.resume()
    assert not session.is_paused(), "Debería estar activo"
    session.resume()  # segunda llamada — debe ser no-op
    assert not session.is_paused(), "Sigue activo tras doble resume"

    session.stop()
    print("  OK — doble pause/resume sin errores.")


def main() -> None:
    parser = argparse.ArgumentParser(description="Demo de pause/resume")
    parser.add_argument("--pid", type=int, default=None, help="PID a grabar")
    parser.add_argument("--mic", action="store_true", help="Incluir demo de micrófono")
    parser.add_argument("--format", default="wav", choices=["wav", "mp3", "opus", "flac"])
    args = parser.parse_args()

    mlrecorder.initialize()

    pid = args.pid if args.pid else list_and_choose_process()

    demo_process(pid, args.format)
    demo_double_pause(pid)

    if args.mic:
        demo_microphone(args.format)

    mlrecorder.shutdown()
    print("\nDemo completado. Archivos en recordings_demo/")


if __name__ == "__main__":
    main()
