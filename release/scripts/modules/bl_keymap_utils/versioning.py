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

# Update Blender version this key-map was written in:
#
# When the version is ``(0, 0, 0)``, the key-map being loaded didn't contain any versioning information.
# This will older than ``(2, 92, 0)``.

def keyconfig_update(keyconfig_data, keyconfig_version):
    from bpy.app import version_file as blender_version
    if keyconfig_version >= blender_version:
        return keyconfig_data

    # Version the key-map.
    import copy
    has_copy = False

    # Default repeat to false.
    if keyconfig_version <= (2, 92, 0):
        # Only copy once.
        if not has_copy:
            keyconfig_data = copy.deepcopy(keyconfig_data)
            has_copy = True

        for _km_name, _km_parms, km_items_data in keyconfig_data:
            for (_item_op, item_event, _item_prop) in km_items_data["items"]:
                if item_event.get("value") == 'PRESS':
                    # Unfortunately we don't know the 'map_type' at this point.
                    # Setting repeat true on other kinds of events is harmless.
                    item_event["repeat"] = True

    return keyconfig_data
