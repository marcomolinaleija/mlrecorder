import time

from mlrecorder import list_active_processes, start_recorder, stop_all


def main() -> int:
    processes = list_active_processes()
    if not processes:
        print("No hay procesos con audio activo en este momento.")
        return 1

    target = processes[0]
    print(f"Grabando PID={target.process_id} ({target.process_name}) por 5 segundos...")

    session = start_recorder(
        pid=target.process_id,
        output_dir="recordings",
        fmt="wav",
        strict_process_isolation=True,
    )

    try:
        time.sleep(5)
    finally:
        session.stop()
        stop_all()

    print("Captura finalizada.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
