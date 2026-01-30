# import os
# import subprocess
# import shutil
# from setuptools import setup, Extension
# from setuptools.command.build_ext import build_ext
# import tvm_ffi

# # --- Dependency Setup ---
# FASTFLOW_REPO = "https://github.com/fastflow/fastflow.git"
# FASTFLOW_TAG = "master" 
# DEPS_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "build/_deps"))

# def get_tvm_ffi_include_paths():
#     tvm_ffi_root = os.path.dirname(os.path.abspath(tvm_ffi.__file__))
#     tvm_ffi_base = os.path.abspath(os.path.join(tvm_ffi_root, os.path.pardir))
#     ffi_include = os.path.join(tvm_ffi_base, "include")
#     dlpack_include = os.path.join(tvm_ffi_base, "3rdparty", "dlpack", "include")
#     return [ffi_include, dlpack_include]

# class CustomBuildExt(build_ext):
#     def run(self):
#         ff_dir = os.path.join(DEPS_DIR, "fastflow")
#         if not os.path.exists(ff_dir):
#             os.makedirs(DEPS_DIR, exist_ok=True)
#             subprocess.check_call(["git", "clone", "--depth", "1", "--branch", FASTFLOW_TAG, FASTFLOW_REPO, ff_dir])
#             subprocess.check_call(["/bin/bash", "./mapping_string.sh"], cwd=os.path.join(ff_dir, "ff/"))

#         # Map include paths
#         paths = get_tvm_ffi_include_paths() + [ff_dir, "src/"]
#         for ext in self.extensions:
#             ext.include_dirs.extend(paths)
        
#         super().run()
        
#         # After build, copy the .cpp to the package as a .hpp for header-only usage
#         pkg_dir = os.path.join(self.build_lib, "fftvm")
#         os.makedirs(pkg_dir, exist_ok=True)

#         ff_src_headers = os.path.join(ff_dir, "ff")
#         ff_dst_headers = os.path.join(pkg_dir, "ff")
#         if os.path.exists(ff_dst_headers):
#             shutil.rmtree(ff_dst_headers)
#         shutil.copytree(ff_src_headers, ff_dst_headers)
#         shutil.copyfile("src/libfftvm.cpp", os.path.join(pkg_dir, "libfftvm.hpp"))

# fftvm_ext = Extension(
#     name="fftvm._lib",
#     sources=["src/libfftvm.cpp"],
#     language="c++",
#     extra_compile_args=["-std=c++17", "-O3", "-fPIC"],
#     # This triggers the interleaved implementation during the .so build
#     define_macros=[("FFTVM_IMPLEMENTATION", "1")] 
# )

# setup(
#     name="fftvm",
#     packages=["fftvm"],
#     ext_modules=[fftvm_ext],
#     cmdclass={"build_ext": CustomBuildExt},
# )


import os
import subprocess
from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
import shutil

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
        pkg_dir = os.path.join(self.build_lib, "fftvm")
        os.makedirs(pkg_dir, exist_ok=True)

        ff_src_headers = os.path.join(ff_dir, "ff")
        ff_dst_headers = os.path.join(pkg_dir, "ff")
        if os.path.exists(ff_dst_headers):
            shutil.rmtree(ff_dst_headers)
        shutil.copytree(ff_src_headers, ff_dst_headers)
        shutil.copyfile("src/libfftvm.cpp", os.path.join(pkg_dir, "libfftvm.hpp"))


# --- Define the Extension ---
fftvm_ext = Extension(
    name="fftvm._lib",  # The importable module name
    sources=["src/libfftvm.cpp"],  # Your C++ source
    language="c++",
    extra_compile_args=["-std=c++17", "-O0", "-fPIC"],
    define_macros=[("FFTVM_IMPL", "1"), ("FFTVM_REG_FFI", "1")] 
)

setup(
    name="fftvm",
    packages=["fftvm"],
    ext_modules=[fftvm_ext],
    cmdclass={"build_ext": CustomBuildExt},
)
