# SPDX-FileCopyrightText: 2010-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Redirect the module loader to the user scripts directory.

# Thus feature set modules can be added to this package without
# writing to the actual Rigify installation directory.

def _install_path():
    import bpy
    import os
    return os.path.join(bpy.utils.script_path_user(), 'rigify')


__path__ = [_install_path()]
