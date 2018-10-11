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
from bpy.types import Menu, Panel, UIList
from rna_prop_ui import PropertyPanel
from .properties_grease_pencil_common import (
    GreasePencilOnionPanel,
    GPENCIL_UL_layer,
)

###############################
# Base-Classes (for shared stuff - e.g. poll, attributes, etc.)


class DataButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        return context.object and context.object.type == 'GPENCIL'


class LayerDataButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        return (context.object and
                context.object.type == 'GPENCIL' and
                context.active_gpencil_layer)


###############################
# GP Object Properties Panels and Helper Classes

class DATA_PT_gpencil(DataButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        # Grease Pencil data selector
        gpd_owner = context.gpencil_data_owner

        layout.template_ID(gpd_owner, "data")


class GPENCIL_MT_layer_specials(Menu):
    bl_label = "Layer"

    def draw(self, context):
        layout = self.layout

        layout.operator("gpencil.layer_duplicate", icon='ADD')  # XXX: needs a dedicated icon

        layout.separator()

        layout.operator("gpencil.reveal", icon='RESTRICT_VIEW_OFF', text="Show All")
        layout.operator("gpencil.hide", icon='RESTRICT_VIEW_ON', text="Hide Others").unselected = True

        layout.separator()

        layout.operator("gpencil.lock_all", icon='LOCKED', text="Lock All")
        layout.operator("gpencil.unlock_all", icon='UNLOCKED', text="UnLock All")

        layout.separator()

        layout.operator("gpencil.layer_merge", icon='SORT_ASC', text="Merge Down")

        layout.separator()
        layout.menu("VIEW3D_MT_gpencil_copy_layer")


class DATA_PT_gpencil_datapanel(Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"
    bl_label = "Layers"

    @classmethod
    def poll(cls, context):
        if context.gpencil_data is None:
            return False

        ob = context.object
        if ob is not None and ob.type == 'GPENCIL':
            return True

        return False

    @staticmethod
    def draw(self, context):
        layout = self.layout
        #layout.use_property_split = True
        layout.use_property_decorate = False

        gpd = context.gpencil_data

        # Grease Pencil data...
        if (gpd is None) or (not gpd.layers):
            layout.operator("gpencil.layer_add", text="New Layer")
        else:
            self.draw_layers(context, layout, gpd)

    def draw_layers(self, context, layout, gpd):
        userpref = context.user_preferences
        edit = userpref.edit
        reverse = edit.use_grease_pencil_reverse_layers

        row = layout.row()

        col = row.column()
        layer_rows = 7
        col.template_list("GPENCIL_UL_layer", "", gpd, "layers", gpd.layers, "active_index",
                          rows=layer_rows, reverse=reverse)

        col = row.column()

        sub = col.column(align=True)
        sub.operator("gpencil.layer_add", icon='ADD', text="")
        sub.operator("gpencil.layer_remove", icon='REMOVE', text="")

        gpl = context.active_gpencil_layer
        if gpl:
            sub.menu("GPENCIL_MT_layer_specials", icon='DOWNARROW_HLT', text="")

            if len(gpd.layers) > 1:
                col.separator()

                sub = col.column(align=True)
                if reverse is False:
                    sub.operator("gpencil.layer_move", icon='TRIA_UP', text="").type = 'UP'
                    sub.operator("gpencil.layer_move", icon='TRIA_DOWN', text="").type = 'DOWN'
                else:
                    sub.operator("gpencil.layer_move", icon='TRIA_UP', text="").type = 'DOWN'
                    sub.operator("gpencil.layer_move", icon='TRIA_DOWN', text="").type = 'UP'

                col.separator()

                sub = col.column(align=True)
                sub.operator("gpencil.layer_isolate", icon='LOCKED', text="").affect_visibility = False
                sub.operator("gpencil.layer_isolate", icon='RESTRICT_VIEW_ON', text="").affect_visibility = True

        row = layout.row(align=True)
        if gpl:
            row.prop(gpl, "opacity", text="Opacity", slider=True)


class DATA_PT_gpencil_layer_optionpanel(LayerDataButtonsPanel, Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"
    bl_label = "Adjustments"
    bl_parent_id = 'DATA_PT_gpencil_datapanel'
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        gpl = context.active_gpencil_layer
        layout.active = not gpl.lock

        # Layer options
        # Offsets - Color Tint
        layout.enabled = not gpl.lock
        col = layout.column(align=True)
        col.prop(gpl, "tint_color")
        col.prop(gpl, "tint_factor", text="Factor", slider=True)

        # Offsets - Thickness
        col = layout.row(align=True)
        col.prop(gpl, "line_change", text="Stroke Thickness")

        col = layout.row(align=True)
        col.prop(gpl, "lock_material")


class DATA_PT_gpencil_parentpanel(LayerDataButtonsPanel, Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"
    bl_label = "Relations"
    bl_parent_id = 'DATA_PT_gpencil_datapanel'
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        gpl = context.active_gpencil_layer
        col = layout.column()
        col.active = not gpl.lock
        col.prop(gpl, "parent")
        col.prop(gpl, "parent_type", text="Type")
        parent = gpl.parent

        if parent and gpl.parent_type == 'BONE' and parent.type == 'ARMATURE':
            col.prop_search(gpl, "parent_bone", parent.data, "bones", text="Bone")


class DATA_PT_gpencil_onionpanel(Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"
    bl_label = "Onion Skinning"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return bool(context.active_gpencil_layer)

    @staticmethod
    def draw_header(self, context):
        self.layout.prop(context.gpencil_data, "use_onion_skinning", text="")

    def draw(self, context):
        gpd = context.gpencil_data

        layout = self.layout
        layout.use_property_split = True
        layout.enabled = gpd.use_onion_skinning and gpd.users <= 1

        if gpd.use_onion_skinning and gpd.users > 1:
            layout.label(text="Multiuser datablock not supported", icon='ERROR')

        GreasePencilOnionPanel.draw_settings(layout, gpd)


class GPENCIL_MT_gpencil_vertex_group(Menu):
    bl_label = "GP Vertex Groups"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'EXEC_AREA'
        layout.operator("object.vertex_group_add")

        ob = context.active_object
        if ob.vertex_groups.active:
            layout.separator()

            layout.operator("gpencil.vertex_group_assign", text="Assign to Active Group")
            layout.operator("gpencil.vertex_group_remove_from", text="Remove from Active Group")

            layout.separator()
            layout.operator_menu_enum("object.vertex_group_set_active", "group", text="Set Active Group")
            layout.operator("object.vertex_group_remove", text="Remove Active Group").all = False
            layout.operator("object.vertex_group_remove", text="Remove All Groups").all = True

            layout.separator()
            layout.operator("gpencil.vertex_group_select", text="Select Points")
            layout.operator("gpencil.vertex_group_deselect", text="Deselect Points")


class GPENCIL_UL_vgroups(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        vgroup = item
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            layout.prop(vgroup, "name", text="", emboss=False, icon_value=icon)
            # icon = 'LOCKED' if vgroup.lock_weight else 'UNLOCKED'
            # layout.prop(vgroup, "lock_weight", text="", icon=icon, emboss=False)
        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label(text="", icon_value=icon)


class DATA_PT_gpencil_vertexpanel(DataButtonsPanel, Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"
    bl_label = "Vertex Groups"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        ob = context.object
        group = ob.vertex_groups.active

        rows = 2
        if group:
            rows = 4

        row = layout.row()
        row.template_list("GPENCIL_UL_vgroups", "", ob, "vertex_groups", ob.vertex_groups, "active_index", rows=rows)

        col = row.column(align=True)
        col.operator("object.vertex_group_add", icon='ADD', text="")
        col.operator("object.vertex_group_remove", icon='REMOVE', text="").all = False

        if ob.vertex_groups:
            row = layout.row()

            sub = row.row(align=True)
            sub.operator("gpencil.vertex_group_assign", text="Assign")
            sub.operator("gpencil.vertex_group_remove_from", text="Remove")

            sub = row.row(align=True)
            sub.operator("gpencil.vertex_group_select", text="Select")
            sub.operator("gpencil.vertex_group_deselect", text="Deselect")

            layout.prop(context.tool_settings, "vertex_group_weight", text="Weight")


class DATA_PT_gpencil_display(DataButtonsPanel, Panel):
    bl_label = "Viewport Display"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        ob = context.object

        gpd = context.gpencil_data
        gpl = context.active_gpencil_layer

        layout.prop(gpd, "xray_mode", text="Depth Ordering")
        layout.prop(ob, "empty_display_size", text="Marker Size")

        col = layout.column(align=True)
        col.prop(gpd, "show_constant_thickness")
        sub = col.column()
        sub.active = not gpd.show_constant_thickness
        sub.prop(gpd, "pixel_factor", text="Thickness Scale")

        layout.prop(gpd, "edit_line_color", text="Edit Line Color")
        if gpl:
            layout.prop(gpd, "show_stroke_direction", text="Show Stroke Directions")

        layout.prop(gpd, "use_force_fill_recalc", text="Force Fill Update")
        layout.prop(gpd, "use_adaptative_uv", text="Adaptative UVs")
        layout.prop(gpd, "zdepth_offset", text="Surface Offset")


class DATA_PT_gpencil_canvas(DataButtonsPanel, Panel):
    bl_label = "Canvas"
    bl_parent_id = 'DATA_PT_gpencil_display'
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        gpd = context.gpencil_data
        grid = gpd.grid

        row = layout.row(align=True)
        col = row.column()
        col.prop(grid, "color", text="Color")
        col.prop(grid, "scale", text="Scale")
        col.prop(grid, "offset")
        row = layout.row(align=True)
        col = row.column()
        col.prop(grid, "lines", text="Subdivisions")
        col.prop(grid, "axis", text="Plane")


class DATA_PT_custom_props_gpencil(DataButtonsPanel, PropertyPanel, Panel):
    _context_path = "object.data"
    _property_type = bpy.types.GreasePencil

###############################


classes = (
    DATA_PT_gpencil,
    DATA_PT_gpencil_datapanel,
    DATA_PT_gpencil_onionpanel,
    DATA_PT_gpencil_layer_optionpanel,
    DATA_PT_gpencil_parentpanel,
    DATA_PT_gpencil_vertexpanel,
    DATA_PT_gpencil_display,
    DATA_PT_gpencil_canvas,
    DATA_PT_custom_props_gpencil,

    GPENCIL_UL_vgroups,

    GPENCIL_MT_layer_specials,
    GPENCIL_MT_gpencil_vertex_group,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
