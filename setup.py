from setuptools import setup


try:
    from wheel.bdist_wheel import bdist_wheel as _bdist_wheel
except Exception:
    _bdist_wheel = None


if _bdist_wheel is not None:
    class bdist_wheel(_bdist_wheel):
        def finalize_options(self):
            super().finalize_options()
            # Package contains native Windows DLLs; wheel must be platform-specific.
            self.root_is_pure = False

        def get_tag(self):
            # Pure-Python wrapper + bundled DLLs:
            # - platform-specific (win_amd64)
            # - Python-version agnostic for Python 3
            return "py3", "none", "win_amd64"


    setup(cmdclass={"bdist_wheel": bdist_wheel})
else:
    setup()
