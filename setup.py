import os
import subprocess
from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
import tvm_ffi

print("Building fftvm!!")

FASTFLOW_REPO = "https://github.com/fastflow/fastflow.git"
FASTFLOW_TAG = "master"  # Pin a specific tag for stability
DEPS_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "build/_deps"))


def get_tvm_ffi_include_paths():
    """Dynamically finds the necessary C++ headers for tvm_ffi and its dependencies."""
    tvm_ffi_root = os.path.dirname(os.path.abspath(tvm_ffi.__file__))
    tvm_ffi_base = os.path.abspath(os.path.join(tvm_ffi_root, os.path.pardir))
    ffi_include = os.path.join(tvm_ffi_base, "include")
    dlpack_include = os.path.join(tvm_ffi_base, "3rdparty", "dlpack", "include")
    if not os.path.exists(ffi_include):
        ffi_include = os.path.join(tvm_ffi_root, "include")

    return [ffi_include, dlpack_include]


class CustomBuildExt(build_ext):
    def run(self):
        ff_dir = os.path.join(DEPS_DIR, "fastflow")
        if not os.path.exists(ff_dir):
            print(f"--- Downloading FastFlow ({FASTFLOW_TAG}) ---")
            os.makedirs(DEPS_DIR, exist_ok=True)
            subprocess.check_call(
                [
                    "git",
                    "clone",
                    "--depth",
                    "1",
                    "--branch",
                    FASTFLOW_TAG,
                    FASTFLOW_REPO,
                    ff_dir,
                ]
            )

            subprocess.check_call(
                ["/bin/bash", "./mapping_string.sh"], cwd=os.path.join(ff_dir, "ff/")
            )

        ffi_include, dlpack_include = get_tvm_ffi_include_paths()

        for ext in self.extensions:
            print(f"adding include dirs: ", ffi_include)
            print(f"adding include dirs: ", dlpack_include)
            print(f"adding include dirs: ", ff_dir)

            ext.include_dirs.append(ffi_include)
            ext.include_dirs.append(dlpack_include)
            ext.include_dirs.append(ff_dir)

        super().run()


# --- Define the Extension ---
fftvm_ext = Extension(
    name="fftvm._lib",  # The importable module name
    sources=["src/libfftvm.cpp"],  # Your C++ source
    language="c++",
    extra_compile_args=["-std=c++17", "-O0", "-fPIC"],
)

setup(
    name="fftvm",
    packages=["fftvm"],
    ext_modules=[fftvm_ext],
    cmdclass={"build_ext": CustomBuildExt},
)
