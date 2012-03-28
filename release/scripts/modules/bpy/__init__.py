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

# <pep8-80 compliant>

"""
Give access to blender data and utility functions.
"""

__all__ = (
    "app",
    "context",
    "data",
    "ops",
    "path",
    "props",
    "types",
    "utils",
    )


# internal blender C module
from _bpy import types, props, app, data, context

# python modules
from . import utils, path, ops

# fake operator module
ops = ops.ops_fake_module


def main():
    import sys

    # Possibly temp. addons path
    from os.path import join, dirname, normpath
    sys.path.append(normpath(join(dirname(__file__),
                                  "..", "..", "addons", "modules")))
    sys.path.append(join(utils.user_resource('SCRIPTS'),
                         "addons", "modules"))

    # fake module to allow:
    #   from bpy.types import Panel
    sys.modules["bpy.app"] = app
    sys.modules["bpy.app.handlers"] = app.handlers
    sys.modules["bpy.types"] = types

    #~ if "-d" in sys.argv: # Enable this to measure start up speed
    if 0:
        import cProfile
        cProfile.run('import bpy; bpy.utils.load_scripts()', 'blender.prof')

        import pstats
        p = pstats.Stats('blender.prof')
        p.sort_stats('cumulative').print_stats(100)

    else:
        utils.load_scripts()


main()

del main
