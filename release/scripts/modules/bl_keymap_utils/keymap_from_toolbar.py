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

    cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)

    items_all = [
        # 0: tool
        # 1: keymap item (direct access)
        # 2: keymap item (newly calculated for toolbar)
        [item, None, None]
        for item in ToolSelectPanelHelper._tools_flatten(cls.tools_from_context(context))
        if item is not None
    ]
    items_all_id = {item_container[0].idname for item_container in items_all}

    # Press the toolbar popup key again to set the default tool,
    # this is useful because the select box tool is useful as a way
    # to 'drop' currently active tools (it's basically a 'none' tool).
    # so this allows us to quickly go back to a state that allows
    # a shortcut based workflow (before the tool system was added).
    use_tap_reset = True
    # TODO: support other tools for modes which don't use this tool.
    tap_reset_tool = "builtin.cursor"
    # Check the tool is available in the current context.
    if tap_reset_tool not in items_all_id:
        use_tap_reset = False

    from bl_operators.wm import use_toolbar_release_hack

    # Pie-menu style release to activate.
    use_release_confirm = True

    # Generate items when no keys are mapped.
    use_auto_keymap_alpha = False  # Map manially in the default keymap
    use_auto_keymap_num = True

    # Temporary, only create so we can pass 'properties' to find_item_from_operator.
    use_hack_properties = True

    km_name_default = "Toolbar Popup"
    km_name = km_name_default + " <temp>"
    wm = context.window_manager
    keyconf_user = wm.keyconfigs.user
    keyconf_active = wm.keyconfigs.active

    keymap = keyconf_active.keymaps.get(km_name)
    if keymap is None:
        keymap = keyconf_active.keymaps.new(km_name, space_type='EMPTY', region_type='TEMPORARY')
    for kmi in keymap.keymap_items:
        keymap.keymap_items.remove(kmi)

    keymap_src = keyconf_user.keymaps.get(km_name_default)
    if keymap_src is not None:
        for kmi_src in keymap_src.keymap_items:
            # Skip tools that aren't currently shown.
            if (
                    (kmi_src.idname == "wm.tool_set_by_id") and
                    (kmi_src.properties.name not in items_all_id)
            ):
                continue
            keymap.keymap_items.new_from_item(kmi_src)
    del keymap_src
    del items_all_id


    kmi_unique_args = set()

    def kmi_unique_or_pass(kmi_args):
        kmi_unique_len = len(kmi_unique_args)
        kmi_unique_args.add(dict_as_tuple(kmi_args))
        return kmi_unique_len != len(kmi_unique_args)


    cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)

    if use_hack_properties:
        kmi_hack = keymap.keymap_items.new("wm.tool_set_by_id", 'NONE', 'PRESS')
        kmi_hack_properties = kmi_hack.properties
        kmi_hack.active = False

        kmi_hack_brush_select = keymap.keymap_items.new("paint.brush_select", 'NONE', 'PRESS')
        kmi_hack_brush_select_properties = kmi_hack_brush_select.properties
        kmi_hack_brush_select.active = False

    if use_release_confirm or use_tap_reset:
        kmi_toolbar = wm.keyconfigs.find_item_from_operator(
            idname="wm.toolbar",
        )[1]
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
                idname="wm.tool_set_by_id",
                context='INVOKE_REGION_WIN',
                # properties={"name": item.idname},
                properties=kmi_hack_properties,
                include={'KEYBOARD'},
            )[1]
            if kmi_found:
                use_tap_reset = False
        del kmi_found

    if use_tap_reset:
        use_tap_reset = kmi_unique_or_pass(kmi_toolbar_args)

    if use_tap_reset:
        items_all[:] = [
            item_container
            for item_container in items_all
            if item_container[0].idname != tap_reset_tool
        ]

    # -----------------------
    # Begin Keymap Generation

    # -------------------------------------------------------------------------
    # Direct Tool Assignment & Brushes

    for item_container in items_all:
        item = item_container[0]
        # Only check the first item in the tools key-map (a little arbitrary).
        if use_hack_properties:
            # First check for direct assignment.
            kmi_hack_properties.name = item.idname
            kmi_found = wm.keyconfigs.find_item_from_operator(
                idname="wm.tool_set_by_id",
                context='INVOKE_REGION_WIN',
                # properties={"name": item.idname},
                properties=kmi_hack_properties,
                include={'KEYBOARD'},
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
                        'PAINT_GPENCIL': "gpencil_tool",
                    }.get(mode, None)
                    if attr is not None:
                        setattr(kmi_hack_brush_select_properties, attr, item.data_block)
                        kmi_found = wm.keyconfigs.find_item_from_operator(
                            idname="paint.brush_select",
                            context='INVOKE_REGION_WIN',
                            properties=kmi_hack_brush_select_properties,
                            include={'KEYBOARD'},
                        )[1]
                    elif mode in {'PARTICLE_EDIT', 'SCULPT_GPENCIL'}:
                        # Doesn't use brushes
                        pass
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
                include={'KEYBOARD'},
            )[1]
        elif item.keymap is not None:
            km = keyconf_user.keymaps.get(item.keymap[0])
            if km is None:
                print("Keymap", repr(item.keymap[0]), "not found for tool", item.idname)
                kmi_found = None
            else:
                kmi_first = km.keymap_items
                kmi_first = kmi_first[0] if kmi_first else None
                if kmi_first is not None:
                    kmi_found = wm.keyconfigs.find_item_from_operator(
                        idname=kmi_first.idname,
                        # properties=kmi_first.properties,  # prevents matches, don't use.
                        context='INVOKE_REGION_WIN',
                        include={'KEYBOARD'},
                    )[1]
                    if kmi_found is None:
                        # We need non-keyboard events so keys with 'key_modifier' key is found.
                        kmi_found = wm.keyconfigs.find_item_from_operator(
                            idname=kmi_first.idname,
                            # properties=kmi_first.properties,  # prevents matches, don't use.
                            context='INVOKE_REGION_WIN',
                            exclude={'KEYBOARD'},
                        )[1]
                        if kmi_found is not None:
                            if kmi_found.key_modifier == 'NONE':
                                kmi_found = None
                else:
                    kmi_found = None
                del kmi_first
            del km
        else:
            kmi_found = None
        item_container[1] = kmi_found

    # -------------------------------------------------------------------------
    # Single Key Access

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
                (kmi_found.idname in {"wm.tool_set_by_id", "WM_OT_tool_set_by_id"})
        ):
            kmi_args = {"type": kmi_found_type, **modifier_keywords_from_item(kmi_found)}
            if kmi_unique_or_pass(kmi_args):
                kmi = keymap.keymap_items.new(idname="wm.tool_set_by_id", value='PRESS', **kmi_args)
                kmi.properties.name = item.idname
                item_container[2] = kmi

    # -------------------------------------------------------------------------
    # Single Key Modifier
    #
    #
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
                if kmi_unique_or_pass(kmi_args):
                    kmi = keymap.keymap_items.new(idname="wm.tool_set_by_id", value='PRESS', **kmi_args)
                    kmi.properties.name = item.idname
                    item_container[2] = kmi

    # -------------------------------------------------------------------------
    # Assign A-Z to Keys
    #
    # When the keys are free.

    if use_auto_keymap_alpha:
        # Map all unmapped keys to numbers,
        # while this is a bit strange it means users will not confuse regular key bindings to ordered bindings.

        # First map A-Z.
        kmi_type_alpha_char = [chr(i) for i in range(65, 91)]
        kmi_type_alpha_args = {c: {"type": c} for c in kmi_type_alpha_char}
        kmi_type_alpha_args_tuple = {c: dict_as_tuple(kmi_type_alpha_args[c]) for c in kmi_type_alpha_char}
        for item_container in items_all:
            item, kmi_found, kmi_exist = item_container
            if kmi_exist:
                continue
            kmi_type = item.label[0].upper()
            kmi_tuple = kmi_type_alpha_args_tuple.get(kmi_type)
            if kmi_tuple and kmi_tuple not in kmi_unique_args:
                kmi_unique_args.add(kmi_tuple)
                kmi = keymap.keymap_items.new(
                    idname="wm.tool_set_by_id",
                    value='PRESS',
                    **kmi_type_alpha_args[kmi_type],
                )
                kmi.properties.name = item.idname
                item_container[2] = kmi
        del kmi_type_alpha_char, kmi_type_alpha_args, kmi_type_alpha_args_tuple

    # -------------------------------------------------------------------------
    # Assign Numbers to Keys

    if use_auto_keymap_num:
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
                kmi = keymap.keymap_items.new(idname="wm.tool_set_by_id", value='PRESS', **kmi_args)
                kmi.properties.name = item.idname
                item_container[2] = kmi
                kmi_unique_args.add(kmi_tuple)

                key = kmi_type_dupe.get(kmi_args["type"])
                if key is not None:
                    kmi_args["type"] = key
                    kmi_tuple = dict_as_tuple(kmi_args)
                    if not kmi_tuple in kmi_unique_args:
                        kmi = keymap.keymap_items.new(idname="wm.tool_set_by_id", value='PRESS', **kmi_args)
                        kmi.properties.name = item.idname
                        kmi_unique_args.add(kmi_tuple)


    # ---------------------
    # End Keymap Generation

    if use_hack_properties:
        keymap.keymap_items.remove(kmi_hack)
        keymap.keymap_items.remove(kmi_hack_brush_select)

    # Keep last so we can try add a key without any modifiers
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
            "wm.tool_set_by_id",
            value='PRESS' if use_toolbar_release_hack else 'DOUBLE_CLICK',
            **kmi_toolbar_args_available,
        )
        kmi.properties.name = tap_reset_tool

    if use_release_confirm and (kmi_toolbar_type is not None):
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
                "wm.tool_set_by_id",
                type=kmi_toolbar_type,
                value='RELEASE',
                any=True,
            )

    wm.keyconfigs.update()
    return keymap
