# SPDX-License-Identifier: GPL-2.0-or-later

# <pep8 compliant>

# Update Blender version this key-map was written in:
#
# When the version is `(0, 0, 0)`, the key-map being loaded didn't contain any versioning information.
# This will older than `(2, 92, 0)`.

def keyconfig_update(keyconfig_data, keyconfig_version):
    from bpy.app import version_file as blender_version
    if keyconfig_version >= blender_version:
        return keyconfig_data

    # Version the key-map.
    import copy
    # Only copy once.
    has_copy = False

    # Default repeat to false.
    if keyconfig_version <= (2, 92, 0):
        if not has_copy:
            keyconfig_data = copy.deepcopy(keyconfig_data)
            has_copy = True

        for _km_name, _km_parms, km_items_data in keyconfig_data:
            for (_item_op, item_event, _item_prop) in km_items_data["items"]:
                if item_event.get("value") == 'PRESS':
                    # Unfortunately we don't know the 'map_type' at this point.
                    # Setting repeat true on other kinds of events is harmless.
                    item_event["repeat"] = True

    if keyconfig_version <= (3, 2, 5):
        if not has_copy:
            keyconfig_data = copy.deepcopy(keyconfig_data)
            has_copy = True

        for _km_name, _km_parms, km_items_data in keyconfig_data:
            for (_item_op, item_event, _item_prop) in km_items_data["items"]:
                if ty_new := {
                        'EVT_TWEAK_L': 'LEFTMOUSE',
                        'EVT_TWEAK_M': 'MIDDLEMOUSE',
                        'EVT_TWEAK_R': 'RIGHTMOUSE',
                }.get(item_event.get("type")):
                    item_event["type"] = ty_new
                    if (value := item_event["value"]) != 'ANY':
                        item_event["direction"] = value
                    item_event["value"] = 'CLICK_DRAG'

    if keyconfig_version <= (3, 2, 6):
        if not has_copy:
            keyconfig_data = copy.deepcopy(keyconfig_data)
            has_copy = True

        for _km_name, _km_parms, km_items_data in keyconfig_data:
            for (_item_op, item_event, _item_prop) in km_items_data["items"]:
                if ty_new := {
                        'NDOF_BUTTON_ESC': 'ESC',
                        'NDOF_BUTTON_ALT': 'LEFT_ALT',
                        'NDOF_BUTTON_SHIFT': 'LEFT_SHIFT',
                        'NDOF_BUTTON_CTRL': 'LEFT_CTRL',
                }.get(item_event.get("type")):
                    item_event["type"] = ty_new

    return keyconfig_data
