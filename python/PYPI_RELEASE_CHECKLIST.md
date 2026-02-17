# Checklist de Publicación en PyPI

Este checklist asume que el paquete se publica con nombre `mlrecorder`.

## 1) Preparar versión

1. Actualiza versión en:
   - `pyproject.toml` (`[project].version`)
   - `python/mlrecorder/__init__.py` (`__version__`)
2. Revisa changelog/notas de cambios.
3. Confirma que la API pública no rompió compatibilidad sin aviso.

## 2) Compilar binarios Release

```bat
cmake -S . -B build-release -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows

cmake --build build-release
```

## 3) Construir wheel

```bat
scripts\build_python_wheel.bat
```

Resultado esperado en `dist/`:

- `mlrecorder-<version>-py3-none-win_amd64.whl`

Nota: para este proyecto, el wheel debe ser Windows específico (`win_amd64`), no `any`.

## 4) Verificaciones locales mínimas

1. Crear entorno limpio y probar instalación:

```bat
py -3 -m venv .venv-test
.venv-test\Scripts\activate
pip install --upgrade pip
pip install dist\mlrecorder-<version>-py3-none-win_amd64.whl
```

2. Probar import y API rápida:

```bat
python -c "import mlrecorder as m; print(m.__version__); print(len(m.list_active_processes())); print(len(m.list_microphones()))"
```

3. Probar grabación breve:

```bat
python -c "import time, mlrecorder as m; ps=m.list_active_processes(); s=m.start_recorder(ps[0].process_id, output_dir='recordings', fmt='wav'); time.sleep(1); s.stop(); m.shutdown()"
```

4. Probar mezcla proceso + mic:

```bat
python -c "import time, mlrecorder as m; ps=m.list_active_processes(); mx=m.start_mixed_recorder(ps[0].process_id, output_dir='recordings', include_microphone=True); time.sleep(1); mx.stop(); m.shutdown()"
```

## 5) Publicar en TestPyPI (recomendado)

```bat
scripts\publish_testpypi.bat
```

Probar instalación desde TestPyPI en entorno limpio.

## 6) Publicar en PyPI

```bat
scripts\publish_pypi.bat
```

## 7) Post-publicación

1. Verificar página del paquete en PyPI.
2. Verificar instalación real:

```bat
pip install --upgrade mlrecorder
```

3. Crear tag/release en Git.
4. Publicar notas de versión.
