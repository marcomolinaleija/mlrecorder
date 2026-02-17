# Paquete `pip` de MLRecorder (DLL + API Python simple)

Este documento explica cómo instalar y usar el paquete Python que envuelve `mlrecorder_core.dll`.

Nombre del paquete (pip): `mlrecorder`  
Nombre de importación (Python): `mlrecorder`

## 1) Qué se instala

El wheel incluye:

- `mlrecorder_core.dll`
- `FLAC.dll`
- `ogg.dll`
- `opus.dll`
- API Python:
  - Capa simple: `list_processes()`, `start_recorder(pid)`, `stop_recorder(pid)`, etc.
  - Capa avanzada: clase `MLRecorder`.

## 2) Requisitos

- Windows 10/11 x64.
- Python 3.10+.
- Microsoft Visual C++ Redistributable 2015-2022 x64.

## 3) Construir wheel instalable

### Paso A: compilar binarios Release

```bat
cmake -S . -B build-release -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows

cmake --build build-release
```

### Paso B: construir wheel `pip`

```bat
scripts\build_python_wheel.bat
```

Esto hace:

1. Copia las DLLs Release a `python/mlrecorder/bin`.
2. Genera wheel en `dist/`.

Salida esperada (ejemplo):

```text
dist/mlrecorder-0.1.1-py3-none-win_amd64.whl
```

## 4) Checklist de release

Checklist completo de publicación:

```text
python/PYPI_RELEASE_CHECKLIST.md
```

## 5) Instalar con pip

Desde la raíz del repo:

```bat
pip install dist\mlrecorder-0.1.1-py3-none-win_amd64.whl
```

Para reinstalar en desarrollo:

```bat
pip install --force-reinstall dist\mlrecorder-0.1.1-py3-none-win_amd64.whl
```

## 6) API simple (la que pediste)

### Listar procesos

```python
from mlrecorder import list_processes, list_active_processes

todos = list_processes()
activos = list_active_processes()
print(len(todos), len(activos))
```

### Iniciar grabación por PID con una llamada

```python
import time
from mlrecorder import start_recorder

session = start_recorder(pid=1234, output_dir="recordings", fmt="wav")
time.sleep(10)
session.stop()
```

### Grabar solo micrófono

```python
import time
from mlrecorder import start_microphone_recorder

mic = start_microphone_recorder(output_dir="recordings", fmt="wav")
time.sleep(10)
mic.stop()
```

### Mezclar audio interno + micrófono en un solo archivo

```python
import time
from mlrecorder import start_mixed_recorder

mixed = start_mixed_recorder(
    pid=1234,
    output_dir="recordings",
    fmt="wav",
    include_microphone=True,
)
time.sleep(10)
mixed.stop()
```

### Control rápido

```python
from mlrecorder import stop_recorder, stop_microphone, stop_all, is_recording, set_volume

print(is_recording(1234))
set_volume(1234, 0.7)
stop_recorder(1234)
stop_microphone()  # detiene micrófono predeterminado (si está activo)
stop_all()
```

Funciones simples disponibles:

- `initialize(dll_path=None)`
- `shutdown()`
- `list_processes()`
- `list_active_processes()`
- `list_microphones()`
- `start_recorder(pid, output_dir="recordings", fmt="wav", ...)`
- `start_recorder_to_file(pid, output_file, ...)`
- `start_microphone_recorder(...)`
- `start_mixed_recorder(pid, include_microphone=True, ...)`
- `stop_recorder(pid)`
- `stop_microphone(device_id=None)`
- `stop_all_microphones()`
- `stop_all()`
- `is_recording(pid)`
- `set_volume(pid, volume_0_to_1)`
- `active_session_count()`

## 7) API avanzada (opcional)

Si necesitas control fino, usa `MLRecorder`:

```python
from mlrecorder import MLRecorder, FORMAT_WAV

with MLRecorder() as rec:
    procesos = rec.list_processes(only_active_audio=True)
    rec.start_capture_to_directory(
        process_id=1234,
        output_dir=r"C:\Temp\grabaciones",
        fmt=FORMAT_WAV,
        strict_process_isolation=True,
    )
    rec.stop_capture(1234)
```

## 8) Ejemplos incluidos

- `python/examples/record_pid.py`: ejemplo CLI completo.
- `python/examples/simple_api_demo.py`: ejemplo usando funciones simples.
- `python/examples/mixed_process_mic.py`: mezcla proceso + micrófono en un archivo.

## 9) Aislamiento estricto

Por defecto se usa aislamiento estricto por proceso:

- Si no hay loopback aislado para ese PID/OS, falla con error.
- No cae silenciosamente a captura global.

Si quieres permitir fallback global, usa `strict_process_isolation=False`.

## 10) Publicar en TestPyPI/PyPI

```bat
scripts\publish_testpypi.bat
scripts\publish_pypi.bat
```

## 11) Errores frecuentes

`VCRUNTIME140D.dll` o `ucrtbased.dll`:

- Se está usando build Debug.
- Debes usar binarios Release.

`DLL not found`:

- No se instaló correctamente el wheel o faltan DLLs.
- Reinstala wheel y verifica `site-packages/mlrecorder/bin`.

`Process not found`:

- El PID cambió o ya no existe.

`Process isolation unavailable`:

- El proceso/OS no soporta aislamiento por PID en ese escenario.
