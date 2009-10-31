# This software is distributable under the terms of the GNU
# General Public License (GPL) v2, the text of which can be found at
# http://www.gnu.org/copyleft/gpl.html. Installing, importing or otherwise
# using this module constitutes acceptance of the terms of this License.

import bpy
import os

def expandpath(path):
    if path.startswith("//"):
        return os.path.join(os.path.dirname(bpy.data.filename), path[2:])

    return path

import types
bpy.sys = types.ModuleType("bpy.sys")
bpy.sys.expandpath = expandpath
