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
import bpy
from bpy.types import Menu


class USERPREF_MT_keyconfigs(Menu):
    bl_label = "KeyPresets"
    preset_subdir = "keyconfig"
    preset_operator = "wm.keyconfig_activate"

    def draw(self, context):
        props = self.layout.operator("wm.context_set_value", text="Blender (default)")
        props.data_path = "window_manager.keyconfigs.active"
        props.value = "context.window_manager.keyconfigs.default"

        # now draw the presets
        Menu.draw_preset(self, context)


class InputKeyMapPanel:
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "Input"
    bl_region_type = 'WINDOW'
    bl_options = {'HIDE_HEADER'}

    def draw_entry(self, display_keymaps, entry, col, level=0):
        idname, spaceid, regionid, children = entry

        for km, kc in display_keymaps:
            if km.name == idname and km.space_type == spaceid and km.region_type == regionid:
                self.draw_km(display_keymaps, kc, km, children, col, level)

        '''
        km = kc.keymaps.find(idname, space_type=spaceid, region_type=regionid)
        if not km:
            kc = defkc
            km = kc.keymaps.find(idname, space_type=spaceid, region_type=regionid)

        if km:
            self.draw_km(kc, km, children, col, level)
        '''

    def indented_layout(self, layout, level):
        indentpx = 16
        if level == 0:
            level = 0.0001   # Tweak so that a percentage of 0 won't split by half
        indent = level * indentpx / bpy.context.region.width

        split = layout.split(percentage=indent)
        col = split.column()
        col = split.column()
        return col

    def draw_km(self, display_keymaps, kc, km, children, layout, level):
        km = km.active()

        layout.context_pointer_set("keymap", km)

        col = self.indented_layout(layout, level)

        row = col.row()
        row.prop(km, "show_expanded_children", text="", emboss=False)
        row.label(text=km.name)

        row.label()
        row.label()

        if km.is_modal:
            row.label(text="", icon='LINKED')
        if km.is_user_modified:
            row.operator("wm.keymap_restore", text="Restore")
        else:
            row.label()

        if km.show_expanded_children:
            if children:
                # Put the Parent key map's entries in a 'global' sub-category
                # equal in hierarchy to the other children categories
                subcol = self.indented_layout(col, level + 1)
                subrow = subcol.row()
                subrow.prop(km, "show_expanded_items", text="", emboss=False)
                subrow.label(text="%s " % km.name + "(Global)")
            else:
                km.show_expanded_items = True

            # Key Map items
            if km.show_expanded_items:
                for kmi in km.keymap_items:
                    self.draw_kmi(display_keymaps, kc, km, kmi, col, level + 1)

                # "Add New" at end of keymap item list
                col = self.indented_layout(col, level + 1)
                subcol = col.split(percentage=0.2).column()
                subcol.operator("wm.keyitem_add", text="Add New", icon='ZOOMIN')

            col.separator()

            # Child key maps
            if children:
                subcol = col.column()
                row = subcol.row()

                for entry in children:
                    self.draw_entry(display_keymaps, entry, col, level + 1)

    def draw_kmi(self, display_keymaps, kc, km, kmi, layout, level):
        map_type = kmi.map_type

        col = self.indented_layout(layout, level)

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

            if map_type not in {'TEXTINPUT', 'TIMER'}:
                split = box.split(percentage=0.4)
                sub = split.row()

                if km.is_modal:
                    sub.prop(kmi, "propvalue", text="")
                else:
                    # One day...
                    #~ sub.prop_search(kmi, "idname", bpy.context.window_manager, "operators_all", text="")
                    sub.prop(kmi, "idname", text="")

                sub = split.column()
                subrow = sub.row(align=True)

                if map_type in {'KEYBOARD', 'NDOF'}:
                    subrow.prop(kmi, "type", text="", event=True)
                    subrow.prop(kmi, "value", text="")
                elif map_type == 'MOUSE':
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
                    self.draw_km(display_keymaps, kc, kmm, None, layout, level + 1)
                    layout.context_pointer_set("keymap", km)

    def draw_filtered(self, display_keymaps, filter_text, layout):
        for km, kc in display_keymaps:
            km = km.active()
            layout.context_pointer_set("keymap", km)

            filtered_items = [kmi for kmi in km.keymap_items
                              if filter_text in kmi.idname.lower() or
                                 filter_text in kmi.name.lower()]

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
                    self.draw_kmi(display_keymaps, kc, km, kmi, col, 1)

                # "Add New" at end of keymap item list
                col = self.indented_layout(layout, 1)
                subcol = col.split(percentage=0.2).column()
                subcol.operator("wm.keyitem_add", text="Add New", icon='ZOOMIN')

    def draw_hierarchy(self, display_keymaps, layout):
        from bpy_extras import keyconfig_utils
        for entry in keyconfig_utils.KM_HIERARCHY:
            self.draw_entry(display_keymaps, entry, layout)

    def draw_keymaps(self, context, layout):
        from bpy_extras import keyconfig_utils

        wm = context.window_manager
        kc = wm.keyconfigs.user

        col = layout.column()
        sub = col.column()

        subsplit = sub.split()
        subcol = subsplit.column()

        row = subcol.row(align=True)

        #~ row.prop_search(wm.keyconfigs, "active", wm, "keyconfigs", text="Key Config:")
        text = bpy.path.display_name(context.window_manager.keyconfigs.active.name)
        if not text:
            text = "Blender (default)"
        row.menu("USERPREF_MT_keyconfigs", text=text)
        row.operator("wm.keyconfig_preset_add", text="", icon='ZOOMIN')
        row.operator("wm.keyconfig_preset_add", text="", icon='ZOOMOUT').remove_active = True

        #~ layout.context_pointer_set("keyconfig", wm.keyconfigs.active)
        #~ row.operator("wm.keyconfig_remove", text="", icon='X')

        row.prop(context.space_data, "filter_text", icon='VIEWZOOM')

        col.separator()

        display_keymaps = keyconfig_utils.keyconfig_merge(kc, kc)
        if context.space_data.filter_text != "":
            filter_text = context.space_data.filter_text.lower()
            self.draw_filtered(display_keymaps, filter_text, col)
        else:
            self.draw_hierarchy(display_keymaps, col)


if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
