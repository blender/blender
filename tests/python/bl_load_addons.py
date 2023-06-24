# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

# simple script to enable all addons, and disable

"""
./blender.bin --background -noaudio --factory-startup --python tests/python/bl_load_addons.py
"""

import bpy
import addon_utils

import os
import sys
import importlib

BLACKLIST_DIRS = (
    bpy.utils.user_resource('SCRIPTS'),
) + tuple(addon_utils.paths()[1:])
BLACKLIST_ADDONS = set()


def _init_addon_blacklist():

    # in case we built without cycles
    if not bpy.app.build_options.cycles:
        BLACKLIST_ADDONS.add("cycles")

    # in case we built without freestyle
    if not bpy.app.build_options.freestyle:
        BLACKLIST_ADDONS.add("render_freestyle_svg")

    # netrender has known problems re-registering
    BLACKLIST_ADDONS.add("netrender")

    if not bpy.app.build_options.xr_openxr:
        BLACKLIST_ADDONS.add("viewport_vr_preview")

    for mod in addon_utils.modules():
        if addon_utils.module_bl_info(mod)['blender'] < (2, 80, 0):
            BLACKLIST_ADDONS.add(mod.__name__)


def addon_modules_sorted():
    # Pass in an empty module cache to prevent `addon_utils` local module cache being manipulated.
    modules = addon_utils.modules(module_cache={})
    modules[:] = [
        mod for mod in modules
        if not (mod.__file__.startswith(BLACKLIST_DIRS))
        if not (mod.__name__ in BLACKLIST_ADDONS)
    ]
    modules.sort(key=lambda mod: mod.__name__)
    return modules


def disable_addons():
    # first disable all
    addons = bpy.context.preferences.addons
    for mod_name in list(addons.keys()):
        addon_utils.disable(mod_name, default_set=True)
    assert bool(addons) is False


def test_load_addons():
    modules = addon_modules_sorted()

    disable_addons()

    addons = bpy.context.preferences.addons

    addons_fail = []

    for mod in modules:
        mod_name = mod.__name__
        print("\tenabling:", mod_name)
        addon_utils.enable(mod_name, default_set=True)
        if (mod_name not in addons) and (mod_name not in BLACKLIST_ADDONS):
            addons_fail.append(mod_name)

    if addons_fail:
        print("addons failed to load (%d):" % len(addons_fail))
        for mod_name in addons_fail:
            print("    %s" % mod_name)
    else:
        print("addons all loaded without errors!")
    print("")


def reload_addons(do_reload=True, do_reverse=True):
    modules = addon_modules_sorted()
    addons = bpy.context.preferences.addons

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

            # now test reloading
            if do_reload:
                sys.modules[mod_name] = importlib.reload(sys.modules[mod_name])

        if do_reverse:
            # in case order matters when it shouldn't
            modules.reverse()


def main():

    _init_addon_blacklist()

    # first load addons, print a list of all addons that fail
    test_load_addons()

    reload_addons(do_reload=False, do_reverse=False)
    reload_addons(do_reload=False, do_reverse=True)
    reload_addons(do_reload=True, do_reverse=True)


if __name__ == "__main__":
    main()
