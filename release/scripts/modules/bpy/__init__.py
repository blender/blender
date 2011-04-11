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

"""
Give access to blender data and utility functions.
"""

# internal blender C module
import _bpy
from _bpy import types, props, app

data = _bpy.data
context = _bpy.context

# python modules
from . import utils, path
from . import ops as _ops_module

# fake operator module
ops = _ops_module.ops_fake_module

import sys as _sys


def _main():

    # Possibly temp. addons path
    from os.path import join, dirname, normpath
    _sys.path.append(normpath(join(dirname(__file__), "..", "..", "addons", "modules")))

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
