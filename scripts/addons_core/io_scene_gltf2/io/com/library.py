# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import os
import sys
from pathlib import Path


def dll_path(lib_name, lib_display_name) -> Path | None:
    """
    Get the library path, that should be at addon root
    :return: library path.
    """
    import bpy

    if sys.platform == 'win32':
        library_name = '{}.dll'.format(lib_name)
        # Bundled beside the add-on's `__init__.py`.
        base = os.path.dirname(sys.modules['io_scene_gltf2'].__file__)
    elif sys.platform == 'darwin':
        library_name = 'lib{}.dylib'.format(lib_name)
        # Bundled beside the add-on's `__init__.py`.
        base = os.path.dirname(sys.modules['io_scene_gltf2'].__file__)
    else:
        # Linux, BSD & other UNIX-like systems.
        library_name = 'lib{}.so'.format(lib_name)
        if (system_libs := bpy.utils.resource_path('SYSTEM_LIBS')):
            # System installs place the library beside the add-on, under the system-libs path.
            base = os.path.join(system_libs, 'scripts', 'addons_core', 'io_scene_gltf2')
        elif local := bpy.utils.resource_path('LOCAL'):
            # Portable builds bundle the library in `lib`, beside the shared libraries it depends on.
            base = os.path.join(os.path.dirname(local), 'lib')
        else:
            return None

    return Path(os.path.join(base, library_name))


def dll_exists(lib_name, lib_display_name) -> bool:
    """
    Checks whether the DLL path exists.
    :return: True if the DLL exists.
    """
    path = dll_path(lib_name, lib_display_name)
    if path is None:
        print('ERROR', '{} is not available because its location could not be determined'.format(lib_display_name))
        return False
    exists = path.exists() and path.is_file()
    if exists:
        print('INFO', '{} is available, use library at %s'.format(lib_display_name) % path.absolute())
    else:
        print(
            'ERROR',
            '{} is not available because library could not be found at %s'.format(lib_display_name) %
            path.absolute())
    return exists
