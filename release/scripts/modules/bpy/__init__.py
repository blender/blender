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
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

# internal blender C module
import _bpy
from _bpy import types, props, app

data = _bpy.data
context = _bpy.context

# python modules
from bpy import utils

from bpy import ops as _ops_module

# fake operator module
ops = _ops_module.ops_fake_module

import sys as _sys


def _main():

    ## security issue, dont allow the $CWD in the path.
    ## note: this removes "" but not "." which are the same, security
    ## people need to explain how this is even a fix.
    # _sys.path[:] = filter(None, _sys.path)

    # a bit nasty but this prevents help() and input() from locking blender
    # Ideally we could have some way for the console to replace sys.stdin but
    # python would lock blender while waiting for a return value, not easy :|
    _sys.stdin = None

    # if "-d" in sys.argv: # Enable this to measure startup speed
    if 0:
        import cProfile
        cProfile.run('import bpy; bpy.utils.load_scripts()', 'blender.prof')

        import pstats
        p = pstats.Stats('blender.prof')
        p.sort_stats('cumulative').print_stats(100)

    else:
        utils.load_scripts()


_main()
