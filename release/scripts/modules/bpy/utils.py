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

import bpy
import os

def expandpath(path):
    if path.startswith("//"):
        return os.path.join(os.path.dirname(bpy.data.filename), path[2:])

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
    '''
    All characters besides A-Z/a-z, 0-9 are replaced with "_"
    or the replace argumet if defined.
    '''
    for ch in _unclean_chars:
        name = name.replace(ch,  replace)
    return name


# base scripts
_scripts = os.path.join(os.path.dirname(__file__), os.path.pardir, os.path.pardir)
_scripts = (os.path.normpath(_scripts), )

def script_paths(*args):
    if not args:
        return _scripts

    subdir = os.path.join(*args)
    script_paths = []
    for path in _scripts:
        script_paths.append(os.path.join(path, subdir))

    return script_paths


_presets = os.path.join(_scripts[0], "presets") # FIXME - multiple paths 

def preset_paths(subdir):
	'''
	Returns a list of paths for a spesific preset.
	'''
	
	return (os.path.join(_presets, subdir), )
