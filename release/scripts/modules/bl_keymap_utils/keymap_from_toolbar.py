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

# Dynamically create a keymap which is used by the popup toolbar
# for accelerator key access.

__all__ = (
    "generate",
)

def generate(context, space_type):
    """
    Keymap for popup toolbar, currently generated each time.
    """
    from bl_ui.space_toolsystem_common import ToolSelectPanelHelper

    def modifier_keywords_from_item(kmi):
        kw = {}
        for (attr, default) in (
                ("any", False),
                ("shift", False),
                ("ctrl", False),
                ("alt", False),
                ("oskey", False),
                ("key_modifier", 'NONE'),
        ):
            val = getattr(kmi, attr)
            if val != default:
                kw[attr] = val
        return kw

    def dict_as_tuple(d):
        return tuple((k, v) for (k, v) in sorted(d.items()))

    tool_blacklist = set()

    use_simple_keymap = False

    # Press the toolbar popup key again to set the default tool,
    # this is useful because the select box tool is useful as a way
    # to 'drop' currently active tools (it's basically a 'none' tool).
    # so this allows us to quickly go back to a state that allows
    # a shortcut based workflow (before the tool system was added).
    use_tap_reset = True
    # TODO: support other tools for modes which don't use this tool.
    tap_reset_tool = "Cursor"
    # Check the tool is available in the current context.
    if ToolSelectPanelHelper._tool_get_by_name(context, space_type, tap_reset_tool)[1] is None:
        use_tap_reset = False

    from bl_operators.wm import use_toolbar_release_hack

    # Pie-menu style release to activate.
    use_release_confirm = True

    # Generate items when no keys are mapped.
    use_auto_keymap = True

    # Temporary, only create so we can pass 'properties' to find_item_from_operator.
    use_hack_properties = True

    km_name = "Toolbar Popup"
    wm = context.window_manager
    keyconf = wm.keyconfigs.active
    keymap = keyconf.keymaps.get(km_name)
    if keymap is None:
        keymap = keyconf.keymaps.new(km_name, space_type='EMPTY', region_type='TEMPORARY', tool=True)
    for kmi in keymap.keymap_items:
        keymap.keymap_items.remove(kmi)

    kmi_unique_args = set()

    cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)

    if use_hack_properties:
        kmi_hack = keymap.keymap_items.new("wm.tool_set_by_name", 'A', 'PRESS')
        kmi_hack_properties = kmi_hack.properties

        kmi_hack_brush_select = keymap.keymap_items.new("paint.brush_select", 'A', 'PRESS')
        kmi_hack_brush_select_properties = kmi_hack_brush_select.properties

    if use_release_confirm or use_tap_reset:
        kmi_toolbar = wm.keyconfigs.find_item_from_operator(idname="wm.toolbar")[1]
        kmi_toolbar_type = None if not kmi_toolbar else kmi_toolbar.type
        if use_tap_reset and kmi_toolbar_type is not None:
            kmi_toolbar_args_type_only = {"type": kmi_toolbar_type}
            kmi_toolbar_args = {**kmi_toolbar_args_type_only, **modifier_keywords_from_item(kmi_toolbar)}
        else:
            use_tap_reset = False
        del kmi_toolbar

    if use_tap_reset:
        kmi_found = None
        if use_hack_properties:
            # First check for direct assignment, if this tool already has a key, no need to add a new one.
            kmi_hack_properties.name = tap_reset_tool
            kmi_found = wm.keyconfigs.find_item_from_operator(
                idname="wm.tool_set_by_name",
                context='INVOKE_REGION_WIN',
                # properties={"name": item.text},
                properties=kmi_hack_properties,
            )[1]
            if kmi_found:
                use_tap_reset = False
        del kmi_found

    if use_tap_reset:
        kmi_toolbar_tuple = dict_as_tuple(kmi_toolbar_args)
        if kmi_toolbar_tuple not in kmi_unique_args:
            # Used after keymap is setup.
            kmi_unique_args.add(kmi_toolbar_tuple)
        else:
            use_tap_reset = False
        del kmi_toolbar_tuple

    if use_tap_reset:
        tool_blacklist.add(tap_reset_tool)

    items_all = [
        # 0: tool
        # 1: keymap item (direct access)
        # 2: keymap item (newly calculated for toolbar)
        [item, None, None]
        for item in ToolSelectPanelHelper._tools_flatten(cls.tools_from_context(context))
        if item is not None
        if item.text not in tool_blacklist
    ]

    if use_simple_keymap:
        # Simply assign a key from A-Z.
        for i, (item, _, _) in enumerate(items_all):
            key = chr(ord('A') + i)
            kmi = keymap.keymap_items.new("wm.tool_set_by_name", key, 'PRESS')
            kmi.properties.name = item.text
    else:
        for item_container in items_all:
            item = item_container[0]
            # Only check the first item in the tools key-map (a little arbitrary).
            if use_hack_properties:
                # First check for direct assignment.
                kmi_hack_properties.name = item.text
                kmi_found = wm.keyconfigs.find_item_from_operator(
                    idname="wm.tool_set_by_name",
                    context='INVOKE_REGION_WIN',
                    # properties={"name": item.text},
                    properties=kmi_hack_properties,
                )[1]

                if kmi_found is None:
                    if item.data_block:
                        # PAINT_OT_brush_select
                        mode = context.active_object.mode
                        # See: BKE_paint_get_tool_prop_id_from_paintmode
                        attr = {
                            'SCULPT': "sculpt_tool",
                            'VERTEX_PAINT': "vertex_tool",
                            'WEIGHT_PAINT': "weight_tool",
                            'TEXTURE_PAINT': "image_tool",
                            'GPENCIL_PAINT': "gpencil_tool",
                        }.get(mode, None)
                        if attr is not None:
                            setattr(kmi_hack_brush_select_properties, attr, item.data_block)
                            kmi_found = wm.keyconfigs.find_item_from_operator(
                                idname="paint.brush_select",
                                context='INVOKE_REGION_WIN',
                                properties=kmi_hack_brush_select_properties,
                            )[1]
                        else:
                            print("Unsupported mode:", mode)
                        del mode, attr

            else:
                kmi_found = None

            if kmi_found is not None:
                pass
            elif item.operator is not None:
                kmi_found = wm.keyconfigs.find_item_from_operator(
                    idname=item.operator,
                    context='INVOKE_REGION_WIN',
                )[1]
            elif item.keymap is not None:
                kmi_first = item.keymap[0].keymap_items
                kmi_first = kmi_first[0] if kmi_first else None
                if kmi_first is not None:
                    kmi_found = wm.keyconfigs.find_item_from_operator(
                        idname=kmi_first.idname,
                        # properties=kmi_first.properties,  # prevents matches, don't use.
                        context='INVOKE_REGION_WIN',
                    )[1]
                else:
                    kmi_found = None
                del kmi_first
            else:
                kmi_found = None
            item_container[1] = kmi_found

        # More complex multi-pass test.
        for item_container in items_all:
            item, kmi_found = item_container[:2]
            if kmi_found is None:
                continue
            kmi_found_type = kmi_found.type

            # Only for single keys.
            if (
                    (len(kmi_found_type) == 1) or
                    # When a tool is being activated instead of running an operator, just copy the shortcut.
                    (kmi_found.idname in {"wm.tool_set_by_name", "WM_OT_tool_set_by_name"})
            ):
                kmi_args = {"type": kmi_found_type, **modifier_keywords_from_item(kmi_found)}
                kmi = keymap.keymap_items.new(idname="wm.tool_set_by_name", value='PRESS', **kmi_args)
                kmi.properties.name = item.text
                item_container[2] = kmi
                if use_auto_keymap:
                    kmi_unique_args.add(dict_as_tuple(kmi_args))

        # Test for key_modifier, where alpha key is used as a 'key_modifier'
        # (grease pencil holding 'D' for example).
        for item_container in items_all:
            item, kmi_found, kmi_exist = item_container
            if kmi_found is None or kmi_exist:
                continue

            kmi_found_type = kmi_found.type
            if kmi_found_type in {
                    'LEFTMOUSE',
                    'RIGHTMOUSE',
                    'MIDDLEMOUSE',
                    'BUTTON4MOUSE',
                    'BUTTON5MOUSE',
                    'BUTTON6MOUSE',
                    'BUTTON7MOUSE',
            }:
                kmi_found_type = kmi_found.key_modifier
                # excludes 'NONE'
                if len(kmi_found_type) == 1:
                    kmi_args = {"type": kmi_found_type, **modifier_keywords_from_item(kmi_found)}
                    del kmi_args["key_modifier"]
                    kmi_tuple = dict_as_tuple(kmi_args)
                    if kmi_tuple in kmi_unique_args:
                        continue
                    kmi = keymap.keymap_items.new(idname="wm.tool_set_by_name", value='PRESS', **kmi_args)
                    kmi.properties.name = item.text
                    item_container[2] = kmi
                    if use_auto_keymap:
                        kmi_unique_args.add(kmi_tuple)

        if use_auto_keymap:
            # Map all unmapped keys to numbers,
            # while this is a bit strange it means users will not confuse regular key bindings to ordered bindings.

            # Free events (last used first).
            kmi_type_auto = ('ONE', 'TWO', 'THREE', 'FOUR', 'FIVE', 'SIX', 'SEVEN', 'EIGHT', 'NINE', 'ZERO')
            # Map both numbers and num-pad.
            kmi_type_dupe = {
                'ONE': 'NUMPAD_1',
                'TWO': 'NUMPAD_2',
                'THREE': 'NUMPAD_3',
                'FOUR': 'NUMPAD_4',
                'FIVE': 'NUMPAD_5',
                'SIX': 'NUMPAD_6',
                'SEVEN': 'NUMPAD_7',
                'EIGHT': 'NUMPAD_8',
                'NINE': 'NUMPAD_9',
                'ZERO': 'NUMPAD_0',
            }

            def iter_free_events():
                for mod in ({}, {"shift": True}, {"ctrl": True}, {"alt": True}):
                    for e in kmi_type_auto:
                        yield (e, mod)

            iter_events = iter(iter_free_events())

            for item_container in items_all:
                item, kmi_found, kmi_exist = item_container
                if kmi_exist:
                    continue
                kmi_args = None
                while True:
                    key, mod = next(iter_events, (None, None))
                    if key is None:
                        break
                    kmi_args = {"type": key, **mod}
                    kmi_tuple = dict_as_tuple(kmi_args)
                    if kmi_tuple in kmi_unique_args:
                        kmi_args = None
                    else:
                        break

                if kmi_args is not None:
                    kmi = keymap.keymap_items.new(idname="wm.tool_set_by_name", value='PRESS', **kmi_args)
                    kmi.properties.name = item.text
                    item_container[2] = kmi
                    if use_auto_keymap:
                        kmi_unique_args.add(kmi_tuple)

                    key = kmi_type_dupe.get(kmi_args["type"])
                    if key is not None:
                        kmi_args["type"] = key
                        kmi_tuple = dict_as_tuple(kmi_args)
                        if not kmi_tuple in kmi_unique_args:
                            kmi = keymap.keymap_items.new(idname="wm.tool_set_by_name", value='PRESS', **kmi_args)
                            kmi.properties.name = item.text
                            if use_auto_keymap:
                                kmi_unique_args.add(kmi_tuple)

    if use_hack_properties:
        keymap.keymap_items.remove(kmi_hack)


    # Keepo last so we can try add a key without any modifiers
    # in the case this toolbar was activated with modifiers.
    if use_tap_reset:
        if len(kmi_toolbar_args_type_only) == len(kmi_toolbar_args):
            kmi_toolbar_args_available = kmi_toolbar_args
        else:
            # We have modifiers, see if we have a free key w/o modifiers.
            kmi_toolbar_tuple = dict_as_tuple(kmi_toolbar_args_type_only)
            if kmi_toolbar_tuple not in kmi_unique_args:
                kmi_toolbar_args_available = kmi_toolbar_args_type_only
                kmi_unique_args.add(kmi_toolbar_tuple)
            else:
                kmi_toolbar_args_available = kmi_toolbar_args
            del kmi_toolbar_tuple

        kmi = keymap.keymap_items.new(
            "wm.tool_set_by_name",
            value='PRESS' if use_toolbar_release_hack else 'DOUBLE_CLICK',
            **kmi_toolbar_args_available,
        )
        kmi.properties.name = tap_reset_tool

    if use_release_confirm:
        kmi = keymap.keymap_items.new(
            "ui.button_execute",
            type=kmi_toolbar_type,
            value='RELEASE',
            any=True,
        )
        kmi.properties.skip_depressed = True

        if use_toolbar_release_hack:
            # ... or pass through to let the toolbar know we're released.
            # Let the operator know we're released.
            kmi = keymap.keymap_items.new(
                "wm.tool_set_by_name",
                type=kmi_toolbar_type,
                value='RELEASE',
                any=True,
            )

    wm.keyconfigs.update()
    return keymap
