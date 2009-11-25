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
