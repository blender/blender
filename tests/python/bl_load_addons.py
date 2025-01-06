# SPDX-FileCopyrightText: 2011-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Simple script to enable all add-ons, and disable.

"""
./blender.bin --background --factory-startup --python tests/python/bl_load_addons.py
"""
__all__ = (
    "main",
)

import bpy
import addon_utils

import sys
import importlib

EXCLUDE_DIRECTORIES = (
    bpy.utils.user_resource('SCRIPTS'),
    *addon_utils.paths()[1:],
)
EXCLUDE_ADDONS = set()


def _init_addon_exclusions():

    # In case we built without cycles.
    if not bpy.app.build_options.cycles:
        EXCLUDE_ADDONS.add("cycles")

    # In case we built without freestyle.
    if not bpy.app.build_options.freestyle:
        EXCLUDE_ADDONS.add("render_freestyle_svg")

    if not bpy.app.build_options.xr_openxr:
        EXCLUDE_ADDONS.add("viewport_vr_preview")


def addon_modules_sorted():
    # Pass in an empty module cache to prevent `addon_utils` local module cache being manipulated.
    modules = [
        mod for mod in addon_utils.modules(module_cache={})
        if not (mod.__file__.startswith(EXCLUDE_DIRECTORIES))
        if not (mod.__name__ in EXCLUDE_ADDONS)
    ]
    modules.sort(key=lambda mod: mod.__name__)
    return modules


def disable_addons():
    # First disable all.
    addons = bpy.context.preferences.addons
    for mod_name in list(addons.keys()):
        addon_utils.disable(mod_name, default_set=True)
    assert bool(addons) is False


def test_load_addons(prefs):
    modules = addon_modules_sorted()

    disable_addons()

    addons = prefs.addons

    addons_fail = []

    for mod in modules:
        mod_name = mod.__name__
        print("\tenabling:", mod_name)
        addon_utils.enable(mod_name, default_set=True)
        if (mod_name not in addons) and (mod_name not in EXCLUDE_ADDONS):
            addons_fail.append(mod_name)

    if addons_fail:
        print("addons failed to load (%d):" % len(addons_fail))
        for mod_name in addons_fail:
            print("    %s" % mod_name)
    else:
        print("addons all loaded without errors!")
    print("")


def reload_addons(prefs, do_reload=True, do_reverse=True):
    modules = addon_modules_sorted()
    addons = prefs.addons

    disable_addons()

    # Run twice each time.
    for _ in (0, 1):
        for mod in modules:
            mod_name = mod.__name__
            print("\tenabling:", mod_name)
            addon_utils.enable(mod_name, default_set=True)
            assert mod_name in addons

        for mod in modules:
            mod_name = mod.__name__
            print("\tdisabling:", mod_name)
            addon_utils.disable(mod_name, default_set=True)
            assert not (mod_name in addons)

            # Now test reloading.
            if do_reload:
                sys.modules[mod_name] = importlib.reload(sys.modules[mod_name])

        if do_reverse:
            # In case order matters when it shouldn't.
            modules.reverse()


def main():
    from bpy import context
    # Remove repositories since these point to the users home directory by default.
    prefs = context.preferences
    extension_repos = prefs.extensions.repos
    while extension_repos:
        extension_repos.remove(extension_repos[0])

    _init_addon_exclusions()

    # First load add-ons, print a list of all add-ons that fail.
    test_load_addons(prefs)

    reload_addons(prefs, do_reload=False, do_reverse=False)
    reload_addons(prefs, do_reload=False, do_reverse=True)
    reload_addons(prefs, do_reload=True, do_reverse=True)


if __name__ == "__main__":
    main()
