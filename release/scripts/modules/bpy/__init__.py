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

# internal blender C module
import _bpy
from _bpy import types, props

data = _bpy.data
context = _bpy.context

# python modules
from bpy import utils

from bpy import ops as _ops_module

# fake operator module
ops = _ops_module.ops_fake_module

def load_scripts(reload_scripts=False):
    import os
    import sys
    import traceback

    def test_import(module_name):
        try:
            return __import__(module_name)
        except:
            traceback.print_exc()
            return None

    for base_path in utils.script_paths():
        for path_subdir in ("ui", "op", "io"):
            path = os.path.join(base_path, path_subdir)
            sys.path.insert(0, path)
            for f in sorted(os.listdir(path)):
                if f.endswith(".py"):
                    # python module
                    mod = test_import(f[0:-3])
                elif "." not in f:
                    # python package
                    mod = test_import(f)
                else:
                    mod = None

                if reload_scripts and mod:
                    print("Reloading:", mod)
                    reload(mod)

def _main():

    # a bit nasty but this prevents help() and input() from locking blender
    # Ideally we could have some way for the console to replace sys.stdin but
    # python would lock blender while waiting for a return value, not easy :|
    import sys
    sys.stdin = None

    if "-d" in sys.argv and False: # Enable this to measure startup speed
        import cProfile
        cProfile.run('import bpy; bpy.load_scripts()', 'blender.prof')

        import pstats
        p = pstats.Stats('blender.prof')
        p.sort_stats('cumulative').print_stats(100)

    else:
        load_scripts()

_main()


