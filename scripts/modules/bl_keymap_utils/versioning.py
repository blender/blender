# SPDX-FileCopyrightText: 2020-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

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

    def get_transform_modal_map():
        for km_name, _km_parms, km_items_data in keyconfig_data:
            if km_name == "Transform Modal Map":
                return km_items_data
        print("not found")

    # Default repeat to false.
    if keyconfig_version <= (2, 92, 0):
        if not has_copy:
            keyconfig_data = copy.deepcopy(keyconfig_data)
            has_copy = True

        for _km_name, _km_parms, km_items_data in keyconfig_data:
            for (_item_op, item_event, _item_prop) in km_items_data["items"]:
                if item_event.get("value") == 'PRESS':
                    # Unfortunately we don't know the `map_type` at this point.
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

    if keyconfig_version <= (3, 6, 0):
        # The modal keys "Vert/Edge Slide" and "TrackBall" didn't exist until then.
        # The operator reused the "Move" and "Rotate" respectively.
        if not has_copy:
            keyconfig_data = copy.deepcopy(keyconfig_data)
            has_copy = True

        if km_items_data := get_transform_modal_map():
            km_items = km_items_data["items"]
            for (item_modal, item_event, _item_prop) in km_items:
                if item_modal == 'TRANSLATE':
                    km_items.append(('VERT_EDGE_SLIDE', item_event, None))
                elif item_modal == 'ROTATE':
                    km_items.append(('TRACKBALL', item_event, None))

            # The modal key for "Rotate Normals" also didn't exist until then.
            km_items.append(('ROTATE_NORMALS', {"type": 'N', "value": 'PRESS'}, None))

    if keyconfig_version <= (4, 0, 3):
        if not has_copy:
            keyconfig_data = copy.deepcopy(keyconfig_data)
            has_copy = True

        # "Snap Source Toggle" did not exist until then.
        if km_items_data := get_transform_modal_map():
            km_items_data["items"].append(("EDIT_SNAP_SOURCE_ON", {"type": 'B', "value": 'PRESS'}, None))
            km_items_data["items"].append(("EDIT_SNAP_SOURCE_OFF", {"type": 'B', "value": 'PRESS'}, None))

    if keyconfig_version <= (4, 1, 5):
        if km_items_data := get_transform_modal_map():
            def use_alt_navigate():
                km_item = next((i for i in km_items_data["items"] if i[0] ==
                                "PROPORTIONAL_SIZE" and i[1]["type"] == 'TRACKPADPAN'), None)
                if km_item:
                    return "alt" not in km_item[1] or km_item[1]["alt"] is False

                # Fallback.
                import bpy
                return getattr(
                    bpy.context.window_manager.keyconfigs.active.preferences,
                    "use_alt_navigation",
                    False)

            if use_alt_navigate():
                if not has_copy:
                    keyconfig_data = copy.deepcopy(keyconfig_data)
                    has_copy = True
                    km_items_data = get_transform_modal_map()

                km_items_data["items"].append(
                    ("PASSTHROUGH_NAVIGATE", {"type": 'LEFT_ALT', "value": 'ANY', "any": True}, None))

    return keyconfig_data
