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
        for km_name, _km_params, km_items_data in keyconfig_data:
            if km_name == "Transform Modal Map":
                return km_items_data

    def remove_properties(op_prop_map):
        nonlocal keyconfig_data
        nonlocal has_copy

        changed_items = []
        for km_index, (km_name, _km_parms, km_items_data) in enumerate(keyconfig_data):
            for kmi_item_index, (item_op, item_event, item_prop) in enumerate(km_items_data["items"]):
                if item_prop and item_op in op_prop_map:
                    properties = item_prop.get("properties", [])
                    filtered_properties = [
                        prop for prop in properties if not any(
                            key in prop for key in op_prop_map[item_op])
                    ]

                    if not filtered_properties:
                        filtered_properties = None

                    if filtered_properties is None or len(filtered_properties) < len(properties):
                        changed_items.append((km_index, kmi_item_index, filtered_properties))

        if changed_items:
            if not has_copy:
                keyconfig_data = copy.deepcopy(keyconfig_data)
                has_copy = True

            for km_index, kmi_item_index, filtered_properties in changed_items:
                item_op, item_event, item_prop = keyconfig_data[km_index][2]["items"][kmi_item_index]
                item_prop["properties"] = filtered_properties
                keyconfig_data[km_index][2]["items"][kmi_item_index] = (item_op, item_event, item_prop)

    def rename_keymap(km_name_map):
        nonlocal keyconfig_data
        nonlocal has_copy

        for km_index, (km_name, km_parms, km_items_data) in enumerate(keyconfig_data):
            km_name_dst = km_name_map.get(km_name)
            if km_name_dst is None:
                continue
            if not has_copy:
                keyconfig_data = copy.deepcopy(keyconfig_data)
                has_copy = True
            keyconfig_data[km_index] = (km_name_dst, km_parms, km_items_data)

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
        remove_properties({
            "transform.edge_slide": ["alt_navigation"],
            "transform.resize": ["alt_navigation"],
            "transform.rotate": ["alt_navigation"],
            "transform.shrink_fatten": ["alt_navigation"],
            "transform.transform": ["alt_navigation"],
            "transform.translate": ["alt_navigation"],
            "transform.vert_slide": ["alt_navigation"],
            "view3d.edit_mesh_extrude_move_normal": ["alt_navigation"],
            "armature.extrude_move": ["TRANSFORM_OT_translate"],
            "curve.extrude_move": ["TRANSFORM_OT_translate"],
            "gpencil.extrude_move": ["TRANSFORM_OT_translate"],
            "mesh.rip_edge_move": ["TRANSFORM_OT_translate"],
            "mesh.duplicate_move": ["TRANSFORM_OT_translate"],
            "object.duplicate_move": ["TRANSFORM_OT_translate"],
            "object.duplicate_move_linked": ["TRANSFORM_OT_translate"],
        })

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

    if keyconfig_version <= (4, 1, 21):
        rename_keymap({"NLA Channels": "NLA Tracks"})

    return keyconfig_data
