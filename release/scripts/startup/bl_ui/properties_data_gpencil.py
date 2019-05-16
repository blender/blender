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

###############################
# Base-Classes (for shared stuff - e.g. poll, attributes, etc.)


class DataButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        return context.gpencil


class ObjectButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        ob = context.object
        return ob and ob.type == 'GPENCIL'


class LayerDataButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        gpencil = context.gpencil
        return gpencil and gpencil.layers.active


###############################
# GP Object Properties Panels and Helper Classes

class DATA_PT_context_gpencil(DataButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        ob = context.object
        space = context.space_data

        if ob:
            layout.template_ID(ob, "data")
        else:
            layout.template_ID(space, "pin_id")


class GPENCIL_MT_layer_context_menu(Menu):
    bl_label = "Layer"

    def draw(self, context):
        layout = self.layout
        gpd = context.gpencil

        layout.operator("gpencil.layer_duplicate", icon='ADD')  # XXX: needs a dedicated icon

        layout.separator()

        layout.operator("gpencil.reveal", icon='RESTRICT_VIEW_OFF', text="Show All")
        layout.operator("gpencil.hide", icon='RESTRICT_VIEW_ON', text="Hide Others").unselected = True

        layout.separator()

        layout.operator("gpencil.lock_all", icon='LOCKED', text="Lock All")
        layout.operator("gpencil.unlock_all", icon='UNLOCKED', text="UnLock All")
        layout.prop(gpd, "use_autolock_layers", text="Autolock Inactive Layers")

        layout.separator()

        layout.operator("gpencil.layer_merge", icon='SORT_ASC', text="Merge Down")

        layout.separator()
        layout.menu("VIEW3D_MT_gpencil_copy_layer")


class DATA_PT_gpencil_layers(DataButtonsPanel, Panel):
    bl_label = "Layers"

    def draw(self, context):
        layout = self.layout
        #layout.use_property_split = True
        layout.use_property_decorate = False

        gpd = context.gpencil

        # Grease Pencil data...
        if (gpd is None) or (not gpd.layers):
            layout.operator("gpencil.layer_add", text="New Layer")
        else:
            self.draw_layers(context, layout, gpd)

    def draw_layers(self, _context, layout, gpd):

        row = layout.row()

        col = row.column()
        layer_rows = 7
        col.template_list("GPENCIL_UL_layer", "", gpd, "layers", gpd.layers, "active_index",
                          rows=layer_rows, sort_reverse=True, sort_lock=True)

        gpl = gpd.layers.active

        if gpl:
            srow = col.row(align=True)
            srow.prop(gpl, "blend_mode", text="Blend")

            srow = col.row(align=True)
            srow.prop(gpl, "opacity", text="Opacity", slider=True)
            srow.prop(gpl, "clamp_layer", text="",
                      icon='MOD_MASK' if gpl.clamp_layer else 'LAYER_ACTIVE')

            srow = col.row(align=True)
            srow.prop(gpl, "use_solo_mode", text="Show Only On Keyframed")

        col = row.column()

        sub = col.column(align=True)
        sub.operator("gpencil.layer_add", icon='ADD', text="")
        sub.operator("gpencil.layer_remove", icon='REMOVE', text="")

        if gpl:
            sub.menu("GPENCIL_MT_layer_context_menu", icon='DOWNARROW_HLT', text="")

            if len(gpd.layers) > 1:
                col.separator()

                sub = col.column(align=True)
                sub.operator("gpencil.layer_move", icon='TRIA_UP', text="").type = 'UP'
                sub.operator("gpencil.layer_move", icon='TRIA_DOWN', text="").type = 'DOWN'

                col.separator()

                sub = col.column(align=True)
                sub.operator("gpencil.layer_isolate", icon='LOCKED', text="").affect_visibility = False
                sub.operator("gpencil.layer_isolate", icon='RESTRICT_VIEW_ON', text="").affect_visibility = True


class DATA_PT_gpencil_layer_adjustments(LayerDataButtonsPanel, Panel):
    bl_label = "Adjustments"
    bl_parent_id = 'DATA_PT_gpencil_layers'
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        scene = context.scene

        gpd = context.gpencil
        gpl = gpd.layers.active
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
        col.prop(gpl, "pass_index")

        col = layout.row(align=True)
        col.prop_search(gpl, "viewlayer_render", scene, "view_layers", text="View Layer")

        col = layout.row(align=True)
        col.prop(gpl, "lock_material")


class DATA_PT_gpencil_layer_relations(LayerDataButtonsPanel, Panel):
    bl_label = "Relations"
    bl_parent_id = 'DATA_PT_gpencil_layers'
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        gpd = context.gpencil
        gpl = gpd.layers.active

        col = layout.column()
        col.active = not gpl.lock
        col.prop(gpl, "parent")
        col.prop(gpl, "parent_type", text="Type")
        parent = gpl.parent

        if parent and gpl.parent_type == 'BONE' and parent.type == 'ARMATURE':
            col.prop_search(gpl, "parent_bone", parent.data, "bones", text="Bone")


class DATA_PT_gpencil_layer_display(LayerDataButtonsPanel, Panel):
    bl_label = "Display"
    bl_parent_id = 'DATA_PT_gpencil_layers'
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        gpd = context.gpencil
        gpl = gpd.layers.active

        col = layout.row(align=True)
        col.prop(gpl, "channel_color")


class DATA_PT_gpencil_onion_skinning(DataButtonsPanel, Panel):
    bl_label = "Onion Skinning"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        gpd = context.gpencil

        layout = self.layout
        layout.use_property_split = True
        layout.enabled = gpd.users <= 1

        if gpd.users > 1:
            layout.label(text="Multiuser datablock not supported", icon='ERROR')

        col = layout.column()
        col.prop(gpd, "onion_mode")
        col.prop(gpd, "onion_factor", text="Opacity", slider=True)
        col.prop(gpd, "onion_keyframe_type")

        if gpd.onion_mode == 'ABSOLUTE':
            col = layout.column(align=True)
            col.prop(gpd, "ghost_before_range", text="Frames Before")
            col.prop(gpd, "ghost_after_range", text="Frames After")
        elif gpd.onion_mode == 'RELATIVE':
            col = layout.column(align=True)
            col.prop(gpd, "ghost_before_range", text="Keyframes Before")
            col.prop(gpd, "ghost_after_range", text="Keyframes After")


class DATA_PT_gpencil_onion_skinning_custom_colors(DataButtonsPanel, Panel):
    bl_parent_id = "DATA_PT_gpencil_onion_skinning"
    bl_label = "Custom Colors"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        gpd = context.gpencil

        self.layout.prop(gpd, "use_ghost_custom_colors", text="")

    def draw(self, context):
        gpd = context.gpencil

        layout = self.layout
        layout.use_property_split = True
        layout.enabled = gpd.users <= 1 and gpd.use_ghost_custom_colors

        layout.prop(gpd, "before_color", text="Before")
        layout.prop(gpd, "after_color", text="After")


class DATA_PT_gpencil_onion_skinning_display(DataButtonsPanel, Panel):
    bl_parent_id = "DATA_PT_gpencil_onion_skinning"
    bl_label = "Display"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        gpd = context.gpencil

        layout = self.layout
        layout.use_property_split = True
        layout.enabled = gpd.users <= 1

        layout.prop(gpd, "use_ghosts_always", text="View In Render")

        col = layout.column(align=True)
        col.prop(gpd, "use_onion_fade", text="Fade")
        sub = layout.column()
        sub.active = gpd.onion_mode in {'RELATIVE', 'SELECTED'}
        sub.prop(gpd, "use_onion_loop", text="Loop")


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
    def draw_item(self, _context, layout, _data, item, icon, _active_data, _active_propname, _index):
        vgroup = item
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            layout.prop(vgroup, "name", text="", emboss=False, icon_value=icon)
            icon = 'LOCKED' if vgroup.lock_weight else 'UNLOCKED'
            layout.prop(vgroup, "lock_weight", text="", icon=icon, emboss=False)
        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label(text="", icon_value=icon)


class DATA_PT_gpencil_vertex_groups(ObjectButtonsPanel, Panel):
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


class DATA_PT_gpencil_strokes(DataButtonsPanel, Panel):
    bl_label = "Strokes"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        ob = context.object
        gpd = context.gpencil

        col = layout.column(align=True)
        col.prop(gpd, "stroke_depth_order")

        if ob:
            col.enabled = not ob.show_in_front

        col = layout.column(align=True)
        col.prop(gpd, "stroke_thickness_space")
        sub = col.column()
        sub.active = gpd.stroke_thickness_space == 'WORLDSPACE'
        sub.prop(gpd, "pixel_factor", text="Thickness Scale")

        layout.prop(gpd, "use_force_fill_recalc", text="Force Fill Update")
        layout.prop(gpd, "use_adaptive_uv", text="Adaptive UVs")


class DATA_PT_gpencil_display(DataButtonsPanel, Panel):
    bl_label = "Viewport Display"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        gpd = context.gpencil
        gpl = gpd.layers.active

        layout.prop(gpd, "edit_line_color", text="Edit Line Color")
        if gpl:
            layout.prop(gpd, "show_stroke_direction", text="Show Stroke Directions")


class DATA_PT_gpencil_canvas(DataButtonsPanel, Panel):
    bl_label = "Canvas"
    bl_parent_id = 'DATA_PT_gpencil_display'
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        gpd = context.gpencil
        grid = gpd.grid

        row = layout.row(align=True)
        col = row.column()
        col.prop(grid, "color", text="Color")
        col.prop(grid, "scale", text="Scale")
        col.prop(grid, "offset")
        row = layout.row(align=True)
        col = row.column()
        col.prop(grid, "lines", text="Subdivisions")


class DATA_PT_custom_props_gpencil(DataButtonsPanel, PropertyPanel, Panel):
    _context_path = "object.data"
    _property_type = bpy.types.GreasePencil

###############################


classes = (
    DATA_PT_context_gpencil,
    DATA_PT_gpencil_layers,
    DATA_PT_gpencil_onion_skinning,
    DATA_PT_gpencil_onion_skinning_custom_colors,
    DATA_PT_gpencil_onion_skinning_display,
    DATA_PT_gpencil_layer_adjustments,
    DATA_PT_gpencil_layer_relations,
    DATA_PT_gpencil_layer_display,
    DATA_PT_gpencil_vertex_groups,
    DATA_PT_gpencil_strokes,
    DATA_PT_gpencil_display,
    DATA_PT_gpencil_canvas,
    DATA_PT_custom_props_gpencil,

    GPENCIL_UL_vgroups,

    GPENCIL_MT_layer_context_menu,
    GPENCIL_MT_gpencil_vertex_group,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
