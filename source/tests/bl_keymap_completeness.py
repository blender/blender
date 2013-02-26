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

# simple script to test 'keyconfig_utils' contains correct values.


from bpy_extras import keyconfig_utils


def check_maps():
    maps = set()

    def fill_maps(ls):
        for entry in ls:
            maps.add(entry[0])
            fill_maps(entry[3])

    fill_maps(keyconfig_utils.KM_HIERARCHY)

    import bpy
    maps_bl = set(bpy.context.window_manager.keyconfigs.active.keymaps.keys())

    err = False
    # Check keyconfig contains only maps that exist in blender
    test = maps - maps_bl
    if test:
        print("Keymaps that are in 'keyconfig_utils' but not blender")
        for km_id in sorted(test):
            print("\t%s" % km_id)
        err = True

    test = maps_bl - maps
    if test:
        print("Keymaps that are in blender but not in 'keyconfig_utils'")
        for km_id in sorted(test):
            print("\t%s" % km_id)
        err = True

    return err


def main():
    err = check_maps()

    if err and bpy.app.background:
        # alert CTest we failed
        import sys
        sys.exit(1)

if __name__ == "__main__":
    main()
