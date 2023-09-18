# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Make shared libraries needed by modules available in standalone Python binary.

import sys
import os

exe_dir, exe_file = os.path.split(sys.executable)
is_python = exe_file.startswith("python")

# Path to Blender shared libraries.
shared_lib_dirname = "blender.shared" if sys.platform == "win32" else "lib"
if is_python:
    shared_lib_dir = os.path.abspath(os.path.join(exe_dir, "..", "..", "..", shared_lib_dirname))
else:
    shared_lib_dir = os.path.abspath(os.path.join(exe_dir, shared_lib_dirname))

if sys.platform == "win32":
    # Directory for extensions to find DLLs.
    if is_python:
        os.add_dll_directory(shared_lib_dir)

    # Directory for USD extension to find DLLs.
    import_paths = os.getenv("PXR_USD_WINDOWS_DLL_PATH")
    if import_paths is None:
        os.environ["PXR_USD_WINDOWS_DLL_PATH"] = shared_lib_dir

    # OIIO will by default add all paths from the path variable to add_dll_directory
    # problem there is that those folders will be searched before ours and versions of
    # some DLL files may be found that are not blenders and may not even be the right version
    # causing compatibility issues.
    os.environ["OIIO_LOAD_DLLS_FROM_PATH"] = "0"

# MaterialX libraries, append if already specified.
materialx_libs_dir = os.path.abspath(os.path.join(shared_lib_dir, "materialx", "libraries"))
materialx_libs_env = os.getenv("MATERIALX_SEARCH_PATH")
if materialx_libs_env is None:
    os.environ["MATERIALX_SEARCH_PATH"] = materialx_libs_dir
else:
    os.environ["MATERIALX_SEARCH_PATH"] = materialx_libs_env + os.pathsep + materialx_libs_dir

materialx_libs_env = os.getenv("PXR_MTLX_STDLIB_SEARCH_PATHS")
if materialx_libs_env is None:
    os.environ["PXR_MTLX_STDLIB_SEARCH_PATHS"] = materialx_libs_dir
else:
    os.environ["PXR_MTLX_STDLIB_SEARCH_PATHS"] = materialx_libs_env + os.pathsep + materialx_libs_dir
