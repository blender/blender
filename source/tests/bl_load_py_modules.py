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

# simple script to enable all addons, and disable

import bpy
import addon_utils

import sys
import os

BLACKLIST = {
    "bl_i18n_utils",
    "cycles",
    "io_export_dxf",  # TODO, check on why this fails
    }


def source_list(path, filename_check=None):
    from os.path import join
    for dirpath, dirnames, filenames in os.walk(path):
        # skip '.svn'
        if dirpath.startswith("."):
            continue

        for filename in filenames:
            filepath = join(dirpath, filename)
            if filename_check is None or filename_check(filepath):
                yield filepath


def load_addons():
    modules = addon_utils.modules({})
    modules.sort(key=lambda mod: mod.__name__)
    addons = bpy.context.user_preferences.addons

    # first disable all
    for mod_name in list(addons.keys()):
        addon_utils.disable(mod_name)

    assert(bool(addons) is False)

    for mod in modules:
        mod_name = mod.__name__
        addon_utils.enable(mod_name)
        assert(mod_name in addons)


def load_modules():
    modules = []
    module_paths = []

    # paths blender stores scripts in.
    paths = bpy.utils.script_paths()

    print("Paths:")
    for script_path in paths:
        print("\t'%s'" % script_path)

    #
    # find all sys.path we added
    for script_path in paths:
        for mod_dir in sys.path:
            if mod_dir.startswith(script_path):
                if mod_dir not in module_paths:
                    if os.path.exists(mod_dir):
                        module_paths.append(mod_dir)

    #
    # collect modules from our paths.
    module_names = {}
    for mod_dir in module_paths:
        # print("mod_dir", mod_dir)
        for mod, mod_full in bpy.path.module_names(mod_dir):
            if mod in BLACKLIST:
                continue
            if mod in module_names:
                mod_dir_prev, mod_full_prev = module_names[mod]
                raise Exception("Module found twice %r.\n    (%r -> %r, %r -> %r)" %
                                (mod, mod_dir, mod_full, mod_dir_prev, mod_full_prev))

            modules.append(__import__(mod))

            module_names[mod] = mod_dir, mod_full
    del module_names

    #
    # now submodules
    for m in modules:
        filepath = m.__file__
        if os.path.basename(filepath).startswith("__init__."):
            mod_dir = os.path.dirname(filepath)
            for submod, submod_full in bpy.path.module_names(mod_dir):
                # fromlist is ignored, ugh.
                mod_name_full = m.__name__ + "." + submod
                __import__(mod_name_full)
                mod_imp = sys.modules[mod_name_full]

                # check we load what we ask for.
                assert(os.path.samefile(mod_imp.__file__, submod_full))

                modules.append(mod_imp)

    #
    # check which filepaths we didn't load
    source_files = []
    for mod_dir in module_paths:
        source_files.extend(source_list(mod_dir, filename_check=lambda f: f.endswith(".py")))

    source_files = list(set(source_files))
    source_files.sort()

    #
    # remove loaded files
    loaded_files = list({m.__file__ for m in modules})
    loaded_files.sort()

    for f in loaded_files:
        source_files.remove(f)

    #
    # test we tested all files except for presets and templates
    ignore_paths = [
        os.sep + "presets" + os.sep,
        os.sep + "templates" + os.sep,
    ] + [(os.sep + f + os.sep) for f in BLACKLIST]

    for f in source_files:
        ok = False
        for ignore in ignore_paths:
            if ignore in f:
                ok = True
        if not ok:
            raise Exception("Source file %r not loaded in test" % f)

    print("loaded %d modules" % len(loaded_files))


def main():
    load_addons()
    load_modules()

if __name__ == "__main__":
    # So a python error exits(1)
    try:
        main()
    except:
        import traceback
        traceback.print_exc()
        sys.exit(1)
