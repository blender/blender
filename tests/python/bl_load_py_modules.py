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

"""
./blender.bin --background -noaudio --factory-startup --python tests/python/bl_load_py_modules.py
"""

import bpy
import addon_utils

import sys
import os

BLACKLIST = {
    "bl_i18n_utils",
    "bl_previews_utils",
    "cycles",
    "io_export_dxf",  # TODO, check on why this fails
    'io_import_dxf',  # Because of cydxfentity.so dependency

    # The unpacked wheel is only loaded when actually used, not directly on import:
    os.path.join("io_blend_utils", "blender_bam-unpacked.whl"),
}

# Some modules need to add to the `sys.path`.
MODULE_SYS_PATHS = {
    # Runs in a Python subprocess, so its expected its basedir can be imported.
    "io_blend_utils.blendfile_pack": ".",
}

if not bpy.app.build_options.freestyle:
    BLACKLIST.add("render_freestyle_svg")

BLACKLIST_DIRS = (
    os.path.join(bpy.utils.resource_path('USER'), "scripts"),
) + tuple(addon_utils.paths()[1:])


def module_names_recursive(mod_dir, *, parent=None):
    """
    a version of bpy.path.module_names that includes non-packages
    """

    is_package = os.path.exists(os.path.join(mod_dir, "__init__.py"))

    for n in os.listdir(mod_dir):
        if not n.startswith((".", "_")):
            submod_full = os.path.join(mod_dir, n)
            if os.path.isdir(submod_full):
                if not parent:
                    subparent = n
                else:
                    subparent = parent + "." + n
                yield from module_names_recursive(submod_full, parent=subparent)
            elif n.endswith(".py") and is_package is False:
                submod = n[:-3]
                if parent:
                    submod = parent + "." + submod
                yield submod, submod_full


def module_names_all(mod_dir):
    yield from bpy.path.module_names(mod_dir)
    yield from module_names_recursive(mod_dir)


def addon_modules_sorted():
    modules = addon_utils.modules({})
    modules[:] = [mod for mod in modules if not mod.__file__.startswith(BLACKLIST_DIRS)]
    modules.sort(key=lambda mod: mod.__name__)
    return modules


def source_list(path, filename_check=None):
    from os.path import join
    for dirpath, dirnames, filenames in os.walk(path):
        # skip '.git'
        dirnames[:] = [d for d in dirnames if not d.startswith(".")]

        for filename in filenames:
            filepath = join(dirpath, filename)
            if filename_check is None or filename_check(filepath):
                yield filepath


def load_addons():
    modules = addon_modules_sorted()
    addons = bpy.context.user_preferences.addons

    # first disable all
    for mod_name in list(addons.keys()):
        addon_utils.disable(mod_name, default_set=True)

    assert(bool(addons) is False)

    for mod in modules:
        mod_name = mod.__name__
        if mod_name in BLACKLIST:
            continue
        addon_utils.enable(mod_name, default_set=True)
        if not (mod_name in addons):
            raise Exception("'addon_utils.enable(%r)' call failed" % mod_name)


def load_modules():
    VERBOSE = os.environ.get("BLENDER_VERBOSE") is not None

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
                if not mod_dir.startswith(BLACKLIST_DIRS):
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
    # test we tested all files except for presets and templates
    ignore_paths = [
        os.sep + "presets" + os.sep,
        os.sep + "templates" + os.sep,
    ] + ([(os.sep + f + os.sep) for f in BLACKLIST] +
         [(os.sep + f + ".py") for f in BLACKLIST])

    #
    # now submodules
    for m in modules:
        filepath = m.__file__
        if os.path.basename(filepath).startswith("__init__."):
            mod_dir = os.path.dirname(filepath)
            for submod, submod_full in module_names_all(mod_dir):
                # fromlist is ignored, ugh.
                mod_name_full = m.__name__ + "." + submod

                sys_path_back = sys.path[:]

                sys.path.extend([
                    os.path.normpath(os.path.join(mod_dir, f))
                    for f in MODULE_SYS_PATHS.get(mod_name_full, ())
                ])

                try:
                    __import__(mod_name_full)
                    mod_imp = sys.modules[mod_name_full]

                    sys.path[:] = sys_path_back

                    # check we load what we ask for.
                    assert(os.path.samefile(mod_imp.__file__, submod_full))

                    modules.append(mod_imp)
                except Exception as e:
                    import traceback
                    # Module might fail to import, but we don't want whole test to fail here.
                    # Reasoning:
                    # - This module might be in ignored list (for example, preset or template),
                    #   so failing here will cause false-positive test failure.
                    # - If this is module which should not be ignored, it is not added to list
                    #   of successfully loaded modules, meaning the test will catch this
                    #   import failure.
                    # - We want to catch all failures of this script instead of stopping on
                    #   a first big failure.
                    do_print = True
                    if not VERBOSE:
                        for ignore in ignore_paths:
                            if ignore in submod_full:
                                do_print = False
                                break
                    if do_print:
                        traceback.print_exc()

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

    for f in source_files:
        for ignore in ignore_paths:
            if ignore in f:
                break
        else:
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
