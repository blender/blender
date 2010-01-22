# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

"""
This module contains utility functions spesific to blender but
not assosiated with blenders internal data.
"""

import bpy as _bpy
import os as _os


def expandpath(path):
    if path.startswith("//"):
        return _os.path.join(_os.path.dirname(_bpy.data.filename), path[2:])

    return path


_unclean_chars = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, \
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, \
    35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 46, 47, 58, 59, 60, 61, 62, 63, \
    64, 91, 92, 93, 94, 96, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, \
    133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, \
    147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, \
    161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, \
    175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, \
    189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, \
    203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, \
    217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, \
    231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, \
    245, 246, 247, 248, 249, 250, 251, 252, 253, 254]

_unclean_chars = ''.join([chr(i) for i in _unclean_chars])


def clean_name(name, replace="_"):
    """
    Returns a name with characters replaced that may cause problems under various circumstances, such as writing to a file.
    All characters besides A-Z/a-z, 0-9 are replaced with "_"
    or the replace argumet if defined.
    """
    for ch in _unclean_chars:
        name = name.replace(ch, replace)
    return name


def display_name(name):
    """
    Creates a display string from name to be used menus and the user interface.
    Capitalize the first letter in all lowercase names, mixed case names are kept as is.
    Intended for use with filenames and module names.
    """
    name_base = _os.path.splitext(name)[0]

    # string replacements
    name_base = name_base.replace("_colon_", ":")

    name_base = name_base.replace("_", " ")

    if name_base.lower() == name_base:
        return ' '.join([w[0].upper() + w[1:] for w in name_base.split()])
    else:
        return name_base


# base scripts
_scripts = _os.path.join(_os.path.dirname(__file__), _os.path.pardir, _os.path.pardir)
_scripts = (_os.path.normpath(_scripts), )


def script_paths(*args):
    """
    Returns a list of valid script paths from the home directory and user preferences.

    Accepts any number of string arguments which are joined to make a path.
    """
    scripts = list(_scripts)

    # add user scripts dir
    user_script_path = _bpy.context.user_preferences.filepaths.python_scripts_directory

    if not user_script_path:
        # XXX - WIN32 needs checking, perhaps better call a blender internal function.
        user_script_path = _os.path.join(_os.path.expanduser("~"), ".blender", "scripts")

    user_script_path = _os.path.normpath(user_script_path)

    if user_script_path not in scripts and _os.path.isdir(user_script_path):
        scripts.append(user_script_path)

    if not args:
        return scripts

    subdir = _os.path.join(*args)
    script_paths = []
    for path in scripts:
        path_subdir = _os.path.join(path, subdir)
        if _os.path.isdir(path_subdir):
            script_paths.append(path_subdir)

    return script_paths


_presets = _os.path.join(_scripts[0], "presets") # FIXME - multiple paths


def preset_paths(subdir):
    '''
    Returns a list of paths for a spesific preset.
    '''

    return (_os.path.join(_presets, subdir), )
