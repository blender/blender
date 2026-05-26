# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import os
import sys
from pathlib import Path


def dll_path(lib_name, lib_display_name) -> Path:
    """
    Get the library path, that should be at addon root
    :return: library path.
    """
    library_name = {
        'win32': '{}.dll'.format(lib_name),
        'linux': 'lib{}.so'.format(lib_name),
        'darwin': 'lib{}.dylib'.format(lib_name)
    }.get(sys.platform)

    path = os.path.dirname(sys.modules['io_scene_gltf2'].__file__)
    if path is not None:
        return Path(os.path.join(path, library_name))

    if library_name is None:
        print('WARNING', 'Unsupported platform {}, {} is unavailable'.format(sys.platform, lib_display_name))


def dll_exists(lib_name, lib_display_name) -> bool:
    """
    Checks whether the DLL path exists.
    :return: True if the DLL exists.
    """
    path = dll_path(lib_name, lib_display_name)
    exists = path.exists() and path.is_file()
    if exists:
        print('INFO', '{} is available, use library at %s'.format(lib_display_name) % path.absolute())
    else:
        print(
            'ERROR',
            '{} is not available because library could not be found at %s'.format(lib_display_name) %
            path.absolute())
    return exists
