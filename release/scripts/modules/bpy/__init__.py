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

# internal blender C module
import _bpy
from _bpy import types, props

data = _bpy.data
context = _bpy.context

# python modules
from bpy import utils
from bpy import app

from bpy import ops as _ops_module

# fake operator module
ops = _ops_module.ops_fake_module

import sys as _sys

def load_scripts(reload_scripts=False):
    import os
    import traceback
    import time


    t_main = time.time()

    loaded_modules = set()

    def test_import(module_name):
        if module_name in loaded_modules:
            return None
        if "." in module_name:
            print("Ignoring '%s', can't import files containing multiple periods." % module_name)
            return None

        try:
            t = time.time()
            ret = __import__(module_name)
            if app.debug:
                print("time %s %.4f" % (module_name, time.time() - t))
            return ret
        except:
            traceback.print_exc()
            return None

    def test_reload(module):
        try:
            reload(module)
        except:
            traceback.print_exc()

    if reload_scripts:
        # reload modules that may not be directly included
        for type_class_name in dir(types):
            type_class = getattr(types, type_class_name)
            module_name = getattr(type_class, "__module__", "")

            if module_name and module_name != "bpy.types": # hard coded for C types
               loaded_modules.add(module_name)

        for module_name in loaded_modules:
            print("Reloading:", module_name)
            test_reload(_sys.modules[module_name])

    for base_path in utils.script_paths():
        for path_subdir in ("ui", "op", "io"):
            path = os.path.join(base_path, path_subdir)
            if os.path.isdir(path):
                if path not in _sys.path: # reloading would add twice
                    _sys.path.insert(0, path)
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
                        test_reload(mod)

    if app.debug:
        print("Time %.4f" % (time.time() - t_main))


def _main():

    # security issue, dont allow the $CWD in the path.
    _sys.path[:] = filter(None, _sys.path)

    # a bit nasty but this prevents help() and input() from locking blender
    # Ideally we could have some way for the console to replace sys.stdin but
    # python would lock blender while waiting for a return value, not easy :|
    _sys.stdin = None

    # if "-d" in sys.argv: # Enable this to measure startup speed
    if 0:
        import cProfile
        cProfile.run('import bpy; bpy.load_scripts()', 'blender.prof')

        import pstats
        p = pstats.Stats('blender.prof')
        p.sort_stats('cumulative').print_stats(100)

    else:
        load_scripts()


_main()
