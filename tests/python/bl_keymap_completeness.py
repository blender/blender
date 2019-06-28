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

# simple script to test 'bl_keymap_utils.keymap_hierarchy' contains correct values.

# Needed for 'bl_keymap_utils.keymap_hierarchy' which inspects tools.
import sys
import os
sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, "release", "scripts", "startup"))
del sys, os

from bl_keymap_utils import keymap_hierarchy


def check_maps():
    maps = {}

    def fill_maps(seq):
        for km_name, km_space_type, km_region_type, km_sub in seq:
            maps[km_name] = (km_space_type, km_region_type)
            fill_maps(km_sub)

    fill_maps(keymap_hierarchy.generate())

    import bpy
    keyconf = bpy.context.window_manager.keyconfigs.active
    maps_bl = set(keyconf.keymaps.keys())
    maps_py = set(maps.keys())

    err = False
    # Check keyconfig contains only maps that exist in blender
    test = maps_py - maps_bl
    if test:
        print("Keymaps that are in 'bl_keymap_utils.keymap_hierarchy' but not blender")
        for km_id in test:
            if callable(km_id):
                # Keymap functions of tools are not in blender anyway...
                continue
            print("\t%s" % km_id)
            # TODO T65963, broken keymap hierarchy tests disabled until fixed.
            # err = True

    test = maps_bl - maps_py
    if test:
        print("Keymaps that are in blender but not in 'bl_keymap_utils.keymap_hierarchy'")
        for km_id in test:
            km = keyconf.keymaps[km_id]
            print("    ('%s', '%s', '%s', [])," % (km_id, km.space_type, km.region_type))
        # TODO T65963, broken keymap hierarchy tests disabled until fixed.
        # err = True

    # Check space/region's are OK
    print("Comparing keymap space/region types...")
    for km_id, km in keyconf.keymaps.items():
        km_py = maps.get(km_id)
        if km_py is not None:
            km_space_type, km_region_type = km_py
            if km_space_type != km.space_type or km_region_type != km.region_type:
                print("  Error:")
                print("    expected -- ('%s', '%s', '%s', [])," % (km_id, km.space_type, km.region_type))
                print("    got      -- ('%s', '%s', '%s', [])," % (km_id, km_space_type, km_region_type))
    print("done!")

    return err


def main():
    err = check_maps()

    import bpy
    if err and bpy.app.background:
        # alert CTest we failed
        import sys
        sys.exit(1)


if __name__ == "__main__":
    main()
