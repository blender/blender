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

__all__ = (
    "draw_entry",
    "draw_km",
    "draw_kmi",
    "draw_filtered",
    "draw_hierarchy",
    "draw_keymaps",
    )


import bpy
from bpy.app.translations import pgettext_iface as iface_
from bpy.app.translations import contexts as i18n_contexts


def _indented_layout(layout, level):
    indentpx = 16
    if level == 0:
        level = 0.0001   # Tweak so that a percentage of 0 won't split by half
    indent = level * indentpx / bpy.context.region.width

    split = layout.split(percentage=indent)
    col = split.column()
    col = split.column()
    return col


def draw_entry(display_keymaps, entry, col, level=0):
    idname, spaceid, regionid, children = entry

    for km, kc in display_keymaps:
        if km.name == idname and km.space_type == spaceid and km.region_type == regionid:
            draw_km(display_keymaps, kc, km, children, col, level)

    '''
    km = kc.keymaps.find(idname, space_type=spaceid, region_type=regionid)
    if not km:
        kc = defkc
        km = kc.keymaps.find(idname, space_type=spaceid, region_type=regionid)

    if km:
        draw_km(kc, km, children, col, level)
    '''


def draw_km(display_keymaps, kc, km, children, layout, level):
    km = km.active()

    layout.context_pointer_set("keymap", km)

    col = _indented_layout(layout, level)

    row = col.row()
    row.prop(km, "show_expanded_children", text="", emboss=False)
    row.label(text=km.name, text_ctxt=i18n_contexts.id_windowmanager)

    if km.is_user_modified or km.is_modal:
        subrow = row.row()
        subrow.alignment = 'RIGHT'

        if km.is_user_modified:
            subrow.operator("wm.keymap_restore", text="Restore")
        if km.is_modal:
            subrow.label(text="", icon='LINKED')
        del subrow

    if km.show_expanded_children:
        if children:
            # Put the Parent key map's entries in a 'global' sub-category
            # equal in hierarchy to the other children categories
            subcol = _indented_layout(col, level + 1)
            subrow = subcol.row()
            subrow.prop(km, "show_expanded_items", text="", emboss=False)
            subrow.label(text=iface_("%s (Global)") % km.name, translate=False)
        else:
            km.show_expanded_items = True

        # Key Map items
        if km.show_expanded_items:
            for kmi in km.keymap_items:
                draw_kmi(display_keymaps, kc, km, kmi, col, level + 1)

            # "Add New" at end of keymap item list
            col = _indented_layout(col, level + 1)
            subcol = col.split(percentage=0.2).column()
            subcol.operator("wm.keyitem_add", text="Add New", text_ctxt=i18n_contexts.id_windowmanager,
                            icon='ZOOMIN')

        col.separator()

        # Child key maps
        if children:
            subcol = col.column()
            row = subcol.row()

            for entry in children:
                draw_entry(display_keymaps, entry, col, level + 1)


def draw_kmi(display_keymaps, kc, km, kmi, layout, level):
    map_type = kmi.map_type

    col = _indented_layout(layout, level)

    if kmi.show_expanded:
        col = col.column(align=True)
        box = col.box()
    else:
        box = col.column()

    split = box.split(percentage=0.05)

    # header bar
    row = split.row()
    row.prop(kmi, "show_expanded", text="", emboss=False)

    row = split.row()
    row.prop(kmi, "active", text="", emboss=False)

    if km.is_modal:
        row.prop(kmi, "propvalue", text="")
    else:
        row.label(text=kmi.name)

    row = split.row()
    row.prop(kmi, "map_type", text="")
    if map_type == 'KEYBOARD':
        row.prop(kmi, "type", text="", full_event=True)
    elif map_type == 'MOUSE':
        row.prop(kmi, "type", text="", full_event=True)
    elif map_type == 'NDOF':
        row.prop(kmi, "type", text="", full_event=True)
    elif map_type == 'TWEAK':
        subrow = row.row()
        subrow.prop(kmi, "type", text="")
        subrow.prop(kmi, "value", text="")
    elif map_type == 'TIMER':
        row.prop(kmi, "type", text="")
    else:
        row.label()

    if (not kmi.is_user_defined) and kmi.is_user_modified:
        row.operator("wm.keyitem_restore", text="", icon='BACK').item_id = kmi.id
    else:
        row.operator("wm.keyitem_remove", text="", icon='X').item_id = kmi.id

    # Expanded, additional event settings
    if kmi.show_expanded:
        box = col.box()

        split = box.split(percentage=0.4)
        sub = split.row()

        if km.is_modal:
            sub.prop(kmi, "propvalue", text="")
        else:
            # One day...
            #~ sub.prop_search(kmi, "idname", bpy.context.window_manager, "operators_all", text="")
            sub.prop(kmi, "idname", text="")

        if map_type not in {'TEXTINPUT', 'TIMER'}:
            sub = split.column()
            subrow = sub.row(align=True)

            if map_type == 'KEYBOARD':
                subrow.prop(kmi, "type", text="", event=True)
                subrow.prop(kmi, "value", text="")
            elif map_type in {'MOUSE', 'NDOF'}:
                subrow.prop(kmi, "type", text="")
                subrow.prop(kmi, "value", text="")

            subrow = sub.row()
            subrow.scale_x = 0.75
            subrow.prop(kmi, "any")
            subrow.prop(kmi, "shift")
            subrow.prop(kmi, "ctrl")
            subrow.prop(kmi, "alt")
            subrow.prop(kmi, "oskey", text="Cmd")
            subrow.prop(kmi, "key_modifier", text="", event=True)

        # Operator properties
        box.template_keymap_item_properties(kmi)

        # Modal key maps attached to this operator
        if not km.is_modal:
            kmm = kc.keymaps.find_modal(kmi.idname)
            if kmm:
                draw_km(display_keymaps, kc, kmm, None, layout, level + 1)
                layout.context_pointer_set("keymap", km)

_EVENT_TYPES = set()
_EVENT_TYPE_MAP = {}
_EVENT_TYPE_MAP_EXTRA = {}


def draw_filtered(display_keymaps, filter_type, filter_text, layout):

    if filter_type == 'NAME':
        def filter_func(kmi):
            return (filter_text in kmi.idname.lower() or
                    filter_text in kmi.name.lower())
    else:
        if not _EVENT_TYPES:
            enum = bpy.types.Event.bl_rna.properties["type"].enum_items
            _EVENT_TYPES.update(enum.keys())
            _EVENT_TYPE_MAP.update({item.name.replace(" ", "_").upper(): key
                                    for key, item in enum.items()})

            del enum
            _EVENT_TYPE_MAP_EXTRA.update({
                "`": 'ACCENT_GRAVE',
                "*": 'NUMPAD_ASTERIX',
                "/": 'NUMPAD_SLASH',
                "RMB": 'RIGHTMOUSE',
                "LMB": 'LEFTMOUSE',
                "MMB": 'MIDDLEMOUSE',
                })
            _EVENT_TYPE_MAP_EXTRA.update({
                "%d" % i: "NUMPAD_%d" % i for i in range(9)
                })
        # done with once off init

        filter_text_split = filter_text.strip()
        filter_text_split = filter_text.split()

        # Modifier {kmi.attribute: name} mapping
        key_mod = {
            "ctrl": "ctrl",
            "alt": "alt",
            "shift": "shift",
            "cmd": "oskey",
            "oskey": "oskey",
            "any": "any",
            }
        # KeyMapItem like dict, use for comparing against
        # attr: {states, ...}
        kmi_test_dict = {}

        # initialize? - so if a if a kmi has a MOD assigned it wont show up.
        #~ for kv in key_mod.values():
        #~    kmi_test_dict[kv] = {False}

        # altname: attr
        for kk, kv in key_mod.items():
            if kk in filter_text_split:
                filter_text_split.remove(kk)
                kmi_test_dict[kv] = {True}
        # whats left should be the event type
        if len(filter_text_split) > 1:
            return False
        elif filter_text_split:
            kmi_type = filter_text_split[0].upper()
            kmi_type_set = set()

            if kmi_type in _EVENT_TYPES:
                kmi_type_set.add(kmi_type)
            else:
                # replacement table
                for event_type_map in (_EVENT_TYPE_MAP, _EVENT_TYPE_MAP_EXTRA):
                    kmi_type_test = event_type_map.get(kmi_type)
                    if kmi_type_test is not None:
                        kmi_type_set.add(kmi_type_test)
                    else:
                        # print("Unknown Type:", kmi_type)

                        # Partial match
                        for k, v in event_type_map.items():
                            if (kmi_type in k) or (kmi_type in v):
                                kmi_type_set.add(v)

                        if not kmi_type_set:
                            return False

            kmi_test_dict["type"] = kmi_type_set

        # main filter func, runs many times
        def filter_func(kmi):
            for kk, ki in kmi_test_dict.items():
                if getattr(kmi, kk) not in ki:
                    return False
            return True

    for km, kc in display_keymaps:
        km = km.active()
        layout.context_pointer_set("keymap", km)

        filtered_items = [kmi for kmi in km.keymap_items if filter_func(kmi)]

        if filtered_items:
            col = layout.column()

            row = col.row()
            row.label(text=km.name, icon='DOT')

            row.label()
            row.label()

            if km.is_user_modified:
                row.operator("wm.keymap_restore", text="Restore")
            else:
                row.label()

            for kmi in filtered_items:
                draw_kmi(display_keymaps, kc, km, kmi, col, 1)

            # "Add New" at end of keymap item list
            col = _indented_layout(layout, 1)
            subcol = col.split(percentage=0.2).column()
            subcol.operator("wm.keyitem_add", text="Add New", icon='ZOOMIN')
    return True


def draw_hierarchy(display_keymaps, layout):
    from bpy_extras import keyconfig_utils
    for entry in keyconfig_utils.KM_HIERARCHY:
        draw_entry(display_keymaps, entry, layout)


def draw_keymaps(context, layout):
    from bpy_extras import keyconfig_utils

    wm = context.window_manager
    kc = wm.keyconfigs.user
    spref = context.space_data

    col = layout.column()
    sub = col.column()

    subsplit = sub.split()
    subcol = subsplit.column()

    row = subcol.row(align=True)

    #~ row.prop_search(wm.keyconfigs, "active", wm, "keyconfigs", text="Key Config:")
    text = bpy.path.display_name(wm.keyconfigs.active.name)
    if not text:
        text = "Blender (default)"
    row.menu("USERPREF_MT_keyconfigs", text=text)
    row.operator("wm.keyconfig_preset_add", text="", icon='ZOOMIN')
    row.operator("wm.keyconfig_preset_add", text="", icon='ZOOMOUT').remove_active = True

    #~ layout.context_pointer_set("keyconfig", wm.keyconfigs.active)
    #~ row.operator("wm.keyconfig_remove", text="", icon='X')
    row.separator()
    rowsub = row.split(align=True, percentage=0.33)
    # postpone drawing into rowsub, so we can set alert!

    col.separator()
    display_keymaps = keyconfig_utils.keyconfig_merge(kc, kc)
    filter_type = spref.filter_type
    filter_text = spref.filter_text.strip()
    if filter_text:
        filter_text = filter_text.lower()
        ok = draw_filtered(display_keymaps, filter_type, filter_text, col)
    else:
        draw_hierarchy(display_keymaps, col)
        ok = True

    # go back and fill in rowsub
    rowsub.prop(spref, "filter_type", text="")
    rowsubsub = rowsub.row(align=True)
    if not ok:
        rowsubsub.alert = True
    rowsubsub.prop(spref, "filter_text", text="", icon='VIEWZOOM')
