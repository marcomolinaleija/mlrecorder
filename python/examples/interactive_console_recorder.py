"""
Demo interactivo de consola para mlrecorder.

Este script guia al usuario para:
1) elegir modo de grabacion,
2) mapear procesos y microfonos por menu numerico,
3) configurar formato/salida, y
4) iniciar y detener la grabacion de forma interactiva.

Ejecutar desde la raiz del repo:
    py -3 python\\examples\\interactive_console_recorder.py
"""

from __future__ import annotations

import sys
import time
from pathlib import Path
from typing import Optional, Sequence, Tuple, TypeVar

CURRENT_DIR = Path(__file__).resolve().parent
PYTHON_DIR = CURRENT_DIR.parent
if str(PYTHON_DIR) not in sys.path:
    sys.path.insert(0, str(PYTHON_DIR))

from mlrecorder import (  # noqa: E402
    InputDeviceInfo,
    MLRecorderError,
    ProcessInfo,
    list_active_processes,
    list_microphones,
    list_processes,
    shutdown,
    start_microphone_recorder,
    start_mixed_recorder,
    start_recorder,
)


T = TypeVar("T")


def ask_int(
    prompt: str,
    *,
    minimum: int,
    maximum: int,
    default: Optional[int] = None,
) -> int:
    """Pide un entero en un rango fijo y reintenta hasta ser valido."""
    while True:
        raw = input(prompt).strip()
        if not raw and default is not None:
            return default

        try:
            value = int(raw)
        except ValueError:
            print("Entrada invalida. Escribe un numero.")
            continue

        if value < minimum or value > maximum:
            print(f"Elige un valor entre {minimum} y {maximum}.")
            continue
        return value


def ask_yes_no(prompt: str, *, default: bool = True) -> bool:
    """Pregunta si/no con valor por defecto."""
    suffix = "[S/n]" if default else "[s/N]"
    while True:
        raw = input(f"{prompt} {suffix}: ").strip().lower()
        if not raw:
            return default
        if raw in {"s", "si", "y", "yes"}:
            return True
        if raw in {"n", "no"}:
            return False
        print("Respuesta invalida. Escribe 's' o 'n'.")


def choose_from_menu(
    title: str,
    options: Sequence[Tuple[str, T]],
    *,
    default_index: int = 1,
) -> T:
    """Muestra un menu numerico y retorna el valor seleccionado."""
    print(f"\n{title}")
    for i, (label, _) in enumerate(options, start=1):
        print(f"  {i}. {label}")

    choice = ask_int(
        f"Selecciona una opcion [default {default_index}]: ",
        minimum=1,
        maximum=len(options),
        default=default_index,
    )
    return options[choice - 1][1]


def choose_process() -> ProcessInfo:
    """Permite elegir un proceso desde la lista activa o lista completa."""
    only_active = ask_yes_no("Mostrar solo procesos con audio activo?", default=True)
    processes = list_active_processes() if only_active else list_processes()

    if not processes and only_active:
        print("No hay procesos con audio activo. Mostrando todos los procesos.")
        processes = list_processes()

    if not processes:
        raise MLRecorderError("No se encontraron procesos para seleccionar.")

    # Orden fijo para que la seleccion numerica sea predecible.
    processes = sorted(processes, key=lambda p: (p.process_name.lower(), p.process_id))

    print("\nProcesos disponibles:")
    for i, proc in enumerate(processes, start=1):
        active = "si" if proc.has_active_audio else "no"
        title = f" | {proc.window_title}" if proc.window_title else ""
        print(f"  {i:>2}. PID {proc.process_id:<6} audio={active:<2} {proc.process_name}{title}")

    idx = ask_int(
        "Selecciona proceso por numero: ",
        minimum=1,
        maximum=len(processes),
    )
    return processes[idx - 1]


def choose_microphone() -> InputDeviceInfo:
    """Permite elegir un dispositivo de entrada por numero."""
    microphones = list_microphones()
    if not microphones:
        raise MLRecorderError("No se detectaron microfonos.")

    default_index = 1
    for i, mic in enumerate(microphones, start=1):
        if mic.is_default:
            default_index = i
            break

    print("\nMicrofonos disponibles:")
    for i, mic in enumerate(microphones, start=1):
        mark = " (default)" if mic.is_default else ""
        print(f"  {i:>2}. {mic.friendly_name}{mark}")
        print(f"      id: {mic.device_id}")

    idx = ask_int(
        f"Selecciona microfono [default {default_index}]: ",
        minimum=1,
        maximum=len(microphones),
        default=default_index,
    )
    return microphones[idx - 1]


def ask_duration_seconds() -> int:
    """Pide duracion en segundos; 0 significa detener manualmente."""
    return ask_int(
        "Duracion en segundos (0 = detener manualmente) [10]: ",
        minimum=0,
        maximum=86400,
        default=10,
    )


def run_timer_or_wait(duration_seconds: int) -> None:
    """Bloquea hasta detener: cuenta regresiva o ENTER manual."""
    if duration_seconds == 0:
        input("Grabando. Presiona ENTER para detener...")
        return

    for remaining in range(duration_seconds, 0, -1):
        print(f"\rGrabando... {remaining:>4}s restantes", end="", flush=True)
        time.sleep(1)
    print()


def main() -> int:
    """Punto de entrada interactivo para grabar proceso/microfono."""
    print("=== MLRecorder Interactive Console Demo ===")
    print("Este asistente iniciara una grabacion y la detendra al final.")

    mode = choose_from_menu(
        "Modo de grabacion",
        [
            ("Solo audio del proceso", "process"),
            ("Mezcla: proceso + microfono (archivo unico)", "mixed"),
            ("Solo microfono", "microphone"),
        ],
        default_index=1,
    )

    fmt = choose_from_menu(
        "Formato de salida",
        [
            ("WAV (recomendado para pruebas)", "wav"),
            ("MP3", "mp3"),
            ("Opus", "opus"),
            ("FLAC", "flac"),
        ],
        default_index=1,
    )

    output_default = str((Path.cwd() / "recordings").resolve())
    output_dir = input(f"Directorio de salida [{output_default}]: ").strip() or output_default

    bitrate = ask_int(
        "Bitrate (MP3/Opus) o compresion FLAC (0 = default) [0]: ",
        minimum=0,
        maximum=512,
        default=0,
    )
    skip_silence = ask_yes_no("Omitir silencios?", default=False)
    duration = ask_duration_seconds()

    selected_process: Optional[ProcessInfo] = None
    selected_microphone: Optional[InputDeviceInfo] = None
    strict_process_isolation = True

    if mode in {"process", "mixed"}:
        selected_process = choose_process()
        strict_process_isolation = ask_yes_no(
            "Forzar aislamiento estricto por proceso?",
            default=True,
        )

    if mode in {"mixed", "microphone"}:
        selected_microphone = choose_microphone()

    print("\nResumen:")
    print(f"  modo: {mode}")
    print(f"  formato: {fmt}")
    print(f"  output_dir: {output_dir}")
    print(f"  bitrate: {bitrate}")
    print(f"  skip_silence: {skip_silence}")
    if selected_process:
        print(f"  proceso: PID {selected_process.process_id} ({selected_process.process_name})")
        print(f"  strict_process_isolation: {strict_process_isolation}")
    if selected_microphone:
        print(f"  microfono: {selected_microphone.friendly_name}")

    if not ask_yes_no("Iniciar grabacion ahora?", default=True):
        print("Cancelado por usuario.")
        return 0

    session = None
    try:
        if mode == "process":
            assert selected_process is not None
            session = start_recorder(
                pid=selected_process.process_id,
                output_dir=output_dir,
                fmt=fmt,
                bitrate=bitrate,
                skip_silence=skip_silence,
                strict_process_isolation=strict_process_isolation,
            )
        elif mode == "mixed":
            assert selected_process is not None
            assert selected_microphone is not None
            session = start_mixed_recorder(
                pid=selected_process.process_id,
                output_dir=output_dir,
                fmt=fmt,
                bitrate=bitrate,
                skip_silence=skip_silence,
                strict_process_isolation=strict_process_isolation,
                include_microphone=True,
                input_device_id=selected_microphone.device_id,
                base_name="MixedInteractive",
            )
        else:
            assert selected_microphone is not None
            session = start_microphone_recorder(
                output_dir=output_dir,
                fmt=fmt,
                bitrate=bitrate,
                skip_silence=skip_silence,
                input_device_id=selected_microphone.device_id,
            )

        print("Grabacion iniciada.")
        run_timer_or_wait(duration)
        print("Deteniendo grabacion...")
        session.stop()
        print("Grabacion finalizada correctamente.")
        print(f"Revisa los archivos en: {output_dir}")
        return 0

    except KeyboardInterrupt:
        print("\nInterrumpido por usuario.")
        if session is not None:
            try:
                session.stop()
            except Exception:
                pass
        return 130
    except MLRecorderError as exc:
        print(f"MLRecorder error: {exc}")
        return 1
    finally:
        # Libera recursos del runtime y cualquier grafo de captura en memoria.
        shutdown()


if __name__ == "__main__":
    raise SystemExit(main())
