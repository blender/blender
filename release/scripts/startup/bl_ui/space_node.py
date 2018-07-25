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
import nodeitems_utils
from bpy.types import Header, Menu, Panel
from bpy.app.translations import pgettext_iface as iface_
from bl_operators.presets import PresetMenu
from .properties_grease_pencil_common import (
    GreasePencilDrawingToolsPanel,
    GreasePencilStrokeEditPanel,
    GreasePencilStrokeSculptPanel,
    GreasePencilBrushPanel,
    GreasePencilBrushCurvesPanel,
    GreasePencilDataPanel,
    GreasePencilPaletteColorPanel,
    GreasePencilToolsPanel
)


class NODE_HT_header(Header):
    bl_space_type = 'NODE_EDITOR'

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        snode = context.space_data
        snode_id = snode.id
        id_from = snode.id_from
        toolsettings = context.tool_settings

        row = layout.row(align=True)
        row.template_header()

        # Now expanded via the 'ui_type'
        # layout.prop(snode, "tree_type", text="")

        if snode.tree_type == 'ShaderNodeTree':
            layout.prop(snode, "shader_type", text="")

            ob = context.object
            if snode.shader_type == 'OBJECT' and ob:

                NODE_MT_editor_menus.draw_collapsible(context, layout)

                # No shader nodes for Eevee lamps
                if snode_id and not (context.engine == 'BLENDER_EEVEE' and ob.type == 'LIGHT'):
                    row = layout.row()
                    row.prop(snode_id, "use_nodes")

                layout.separator_spacer()

                row = layout.row()
                # disable material slot buttons when pinned, cannot find correct slot within id_from (#36589)
                row.enabled = not snode.pin
                # Show material.new when no active ID/slot exists
                if not id_from and ob.type in {'MESH', 'CURVE', 'SURFACE', 'FONT', 'METABALL'}:
                    row.template_ID(ob, "active_material", new="material.new")
                # Material ID, but not for Lights
                if id_from and ob.type != 'LIGHT':
                    row.template_ID(id_from, "active_material", new="material.new")

            if snode.shader_type == 'WORLD':
                NODE_MT_editor_menus.draw_collapsible(context, layout)

                if snode_id:
                    row = layout.row()
                    row.prop(snode_id, "use_nodes")

                layout.separator_spacer()

                row = layout.row()
                row.enabled = not snode.pin
                row.template_ID(scene, "world", new="world.new")

            if snode.shader_type == 'LINESTYLE':
                view_layer = context.view_layer
                lineset = view_layer.freestyle_settings.linesets.active

                if lineset is not None:
                    NODE_MT_editor_menus.draw_collapsible(context, layout)

                    if snode_id:
                        row = layout.row()
                        row.prop(snode_id, "use_nodes")

                    layout.separator_spacer()

                    row = layout.row()
                    row.enabled = not snode.pin
                    row.template_ID(lineset, "linestyle", new="scene.freestyle_linestyle_new")

        elif snode.tree_type == 'TextureNodeTree':
            layout.prop(snode, "texture_type", text="")

            NODE_MT_editor_menus.draw_collapsible(context, layout)

            if snode_id:
                layout.prop(snode_id, "use_nodes")

            layout.separator_spacer()

            if id_from:
                if snode.texture_type == 'BRUSH':
                    layout.template_ID(id_from, "texture", new="texture.new")
                else:
                    layout.template_ID(id_from, "active_texture", new="texture.new")

        elif snode.tree_type == 'CompositorNodeTree':

            NODE_MT_editor_menus.draw_collapsible(context, layout)

            if snode_id:
                layout.prop(snode_id, "use_nodes")

            layout.prop(snode, "use_auto_render")
            layout.prop(snode, "show_backdrop")
            if snode.show_backdrop:
                row = layout.row(align=True)
                row.prop(snode, "backdrop_channels", text="", expand=True)

        else:
            # Custom node tree is edited as independent ID block
            NODE_MT_editor_menus.draw_collapsible(context, layout)

            layout.separator_spacer()

            layout.template_ID(snode, "node_tree", new="node.new_node_tree")

        layout.separator_spacer()

        layout.template_running_jobs()

        layout.prop(snode, "pin", text="")
        layout.operator("node.tree_path_parent", text="", icon='FILE_PARENT')

        # Snap
        row = layout.row(align=True)
        row.prop(toolsettings, "use_snap", text="")
        row.prop(toolsettings, "snap_node_element", icon_only=True)
        if toolsettings.snap_node_element != 'GRID':
            row.prop(toolsettings, "snap_target", text="")

        row = layout.row(align=True)
        row.operator("node.clipboard_copy", text="", icon='COPYDOWN')
        row.operator("node.clipboard_paste", text="", icon='PASTEDOWN')


class NODE_MT_editor_menus(Menu):
    bl_idname = "NODE_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        self.draw_menus(self.layout, context)

    @staticmethod
    def draw_menus(layout, context):
        layout.menu("NODE_MT_view")
        layout.menu("NODE_MT_select")
        layout.menu("NODE_MT_add")
        layout.menu("NODE_MT_node")


class NODE_MT_add(bpy.types.Menu):
    bl_space_type = 'NODE_EDITOR'
    bl_label = "Add"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_DEFAULT'
        props = layout.operator("node.add_search", text="Search ...")
        props.use_transform = True

        # actual node submenus are defined by draw functions from node categories
        nodeitems_utils.draw_node_categories_menu(self, context)


class NODE_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        snode = context.space_data

        layout.operator("node.properties", icon='MENU_PANEL')
        layout.operator("node.toolbar", icon='MENU_PANEL')

        layout.separator()

        # Auto-offset nodes (called "insert_offset" in code)
        layout.prop(snode, "use_insert_offset")

        layout.separator()

        layout.operator("view2d.zoom_in")
        layout.operator("view2d.zoom_out")

        layout.separator()

        layout.operator("node.view_selected")
        layout.operator("node.view_all")

        if context.space_data.show_backdrop:
            layout.separator()

            layout.operator("node.backimage_move", text="Backdrop Move")
            layout.operator("node.backimage_zoom", text="Backdrop Zoom In").factor = 1.2
            layout.operator("node.backimage_zoom", text="Backdrop Zoom Out").factor = 0.83333
            layout.operator("node.backimage_fit", text="Fit Backdrop to Available Space")

        layout.separator()

        layout.menu("INFO_MT_area")


class NODE_MT_select(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("node.select_border").tweak = False
        layout.operator("node.select_circle")

        layout.separator()
        layout.operator("node.select_all").action = 'TOGGLE'
        layout.operator("node.select_all", text="Inverse").action = 'INVERT'
        layout.operator("node.select_linked_from")
        layout.operator("node.select_linked_to")

        layout.separator()

        layout.operator("node.select_grouped").extend = False
        layout.operator("node.select_same_type_step", text="Activate Same Type Previous").prev = True
        layout.operator("node.select_same_type_step", text="Activate Same Type Next").prev = False

        layout.separator()

        layout.operator("node.find_node")


class NODE_MT_node(Menu):
    bl_label = "Node"

    def draw(self, context):
        layout = self.layout

        layout.operator("transform.translate")
        layout.operator("transform.rotate")
        layout.operator("transform.resize")

        layout.separator()

        layout.operator("node.duplicate_move")
        layout.operator("node.delete")
        layout.operator("node.delete_reconnect")

        layout.separator()

        layout.operator("node.join", text="Join in New Frame")
        layout.operator("node.detach", text="Remove from Frame")

        layout.separator()

        layout.operator("node.link_make").replace = False
        layout.operator("node.link_make", text="Make and Replace Links").replace = True
        layout.operator("node.links_cut")
        layout.operator("node.links_detach")

        layout.separator()

        layout.operator("node.group_edit").exit = False
        layout.operator("node.group_ungroup")
        layout.operator("node.group_make")
        layout.operator("node.group_insert")

        layout.separator()

        layout.operator("node.hide_toggle")
        layout.operator("node.mute_toggle")
        layout.operator("node.preview_toggle")
        layout.operator("node.hide_socket_toggle")
        layout.operator("node.options_toggle")
        layout.operator("node.collapse_hide_unused_toggle")

        layout.separator()

        layout.operator("node.read_viewlayers")
        layout.operator("node.read_fullsamplelayers")


class NODE_PT_node_color_presets(PresetMenu):
    """Predefined node color"""
    bl_label = "Color Presets"
    preset_subdir = "node_color"
    preset_operator = "script.execute_preset"
    preset_add_operator = "node.node_color_preset_add"


class NODE_MT_node_color_specials(Menu):
    bl_label = "Node Color Specials"

    def draw(self, context):
        layout = self.layout

        layout.operator("node.node_copy_color", icon='COPY_ID')


class NODE_MT_specials(Menu):
    bl_label = "Node Context Menu"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_DEFAULT'
        layout.operator("node.duplicate_move")
        layout.operator("node.delete")
        layout.operator_context = 'EXEC_DEFAULT'

        layout.operator("node.delete_reconnect")

        layout.separator()

        layout.operator("node.link_make").replace = False
        layout.operator("node.link_make", text="Make and Replace Links").replace = True
        layout.operator("node.links_detach")

        layout.separator()

        layout.operator("node.group_make", text="Group")
        layout.operator("node.group_ungroup", text="Ungroup")
        layout.operator("node.group_edit").exit = False

        layout.separator()

        layout.operator("node.hide_toggle")
        layout.operator("node.mute_toggle")
        layout.operator("node.preview_toggle")
        layout.operator("node.hide_socket_toggle")
        layout.operator("node.options_toggle")
        layout.operator("node.collapse_hide_unused_toggle")


class NODE_PT_active_node_generic(Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Node"

    @classmethod
    def poll(cls, context):
        return context.active_node is not None

    def draw(self, context):
        layout = self.layout
        node = context.active_node

        layout.prop(node, "name", icon='NODE')
        layout.prop(node, "label", icon='NODE')


class NODE_PT_active_node_color(Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Color"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = 'NODE_PT_active_node_generic'

    @classmethod
    def poll(cls, context):
        return context.active_node is not None

    def draw_header(self, context):
        node = context.active_node
        self.layout.prop(node, "use_custom_color", text="")

    def draw_header_preset(self, context):
        NODE_PT_node_color_presets.draw_panel_header(self.layout)

    def draw(self, context):
        layout = self.layout
        node = context.active_node

        layout.enabled = node.use_custom_color

        row = layout.row()
        row.prop(node, "color", text="")
        row.menu("NODE_MT_node_color_specials", text="", icon='DOWNARROW_HLT')


class NODE_PT_active_node_properties(Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Properties"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = 'NODE_PT_active_node_generic'

    @classmethod
    def poll(cls, context):
        return context.active_node is not None

    def draw(self, context):
        layout = self.layout
        node = context.active_node
        # set "node" context pointer for the panel layout
        layout.context_pointer_set("node", node)

        if hasattr(node, "draw_buttons_ext"):
            node.draw_buttons_ext(context, layout)
        elif hasattr(node, "draw_buttons"):
            node.draw_buttons(context, layout)

        # XXX this could be filtered further to exclude socket types which don't have meaningful input values (e.g. cycles shader)
        value_inputs = [socket for socket in node.inputs if socket.enabled and not socket.is_linked]
        if value_inputs:
            layout.separator()
            layout.label("Inputs:")
            for socket in value_inputs:
                row = layout.row()
                socket.draw(context, row, node, iface_(socket.name, socket.bl_rna.translation_context))


# Node Backdrop options
class NODE_PT_backdrop(Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Backdrop"

    @classmethod
    def poll(cls, context):
        snode = context.space_data
        return snode.tree_type == 'CompositorNodeTree'

    def draw_header(self, context):
        snode = context.space_data
        self.layout.prop(snode, "show_backdrop", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        snode = context.space_data
        layout.active = snode.show_backdrop

        col = layout.column()

        col.prop(snode, "backdrop_channels", text="Channels")
        col.prop(snode, "backdrop_zoom", text="Zoom")

        col.prop(snode, "backdrop_offset", text="Offset")

        col.separator()

        col.operator("node.backimage_move", text="Move")
        col.operator("node.backimage_fit", text="Fit")


class NODE_PT_quality(bpy.types.Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Performance"

    @classmethod
    def poll(cls, context):
        snode = context.space_data
        return snode.tree_type == 'CompositorNodeTree' and snode.node_tree is not None

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        snode = context.space_data
        tree = snode.node_tree

        col = layout.column()
        col.prop(tree, "render_quality", text="Render")
        col.prop(tree, "edit_quality", text="Edit")
        col.prop(tree, "chunk_size")

        col = layout.column()
        col.prop(tree, "use_opencl")
        col.prop(tree, "use_groupnode_buffer")
        col.prop(tree, "use_two_pass")
        col.prop(tree, "use_viewer_border")


class NODE_UL_interface_sockets(bpy.types.UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        socket = item
        color = socket.draw_color(context)

        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            row = layout.row(align=True)

            # inputs get icon on the left
            if not socket.is_output:
                row.template_node_socket(color)

            row.prop(socket, "name", text="", emboss=False, icon_value=icon)

            # outputs get icon on the right
            if socket.is_output:
                row.template_node_socket(color)

        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.template_node_socket(color)


# Grease Pencil properties
class NODE_PT_grease_pencil(GreasePencilDataPanel, Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_options = {'DEFAULT_CLOSED'}

    # NOTE: this is just a wrapper around the generic GP Panel

    @classmethod
    def poll(cls, context):
        snode = context.space_data
        return snode is not None and snode.node_tree is not None


# Grease Pencil palette colors
class NODE_PT_grease_pencil_palettecolor(GreasePencilPaletteColorPanel, Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'

    # NOTE: this is just a wrapper around the generic GP Panel

    @classmethod
    def poll(cls, context):
        snode = context.space_data
        return snode is not None and snode.node_tree is not None


class NODE_PT_grease_pencil_tools(GreasePencilToolsPanel, Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_options = {'DEFAULT_CLOSED'}

    # NOTE: this is just a wrapper around the generic GP tools panel
    # It contains access to some essential tools usually found only in
    # toolbar, but which may not necessarily be open


# Tool Shelf ------------------


# Grease Pencil drawing tools
class NODE_PT_tools_grease_pencil_draw(GreasePencilDrawingToolsPanel, Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'TOOLS'


# Grease Pencil stroke editing tools
class NODE_PT_tools_grease_pencil_edit(GreasePencilStrokeEditPanel, Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'TOOLS'


# Grease Pencil stroke sculpting tools
class NODE_PT_tools_grease_pencil_sculpt(GreasePencilStrokeSculptPanel, Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'TOOLS'

# Grease Pencil drawing brushes


class NODE_PT_tools_grease_pencil_brush(GreasePencilBrushPanel, Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'TOOLS'

# Grease Pencil drawing curves


class NODE_PT_tools_grease_pencil_brushcurves(GreasePencilBrushCurvesPanel, Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'TOOLS'

# -----------------------------


def node_draw_tree_view(layout, context):
    pass


classes = (
    NODE_HT_header,
    NODE_MT_editor_menus,
    NODE_MT_add,
    NODE_MT_view,
    NODE_MT_select,
    NODE_MT_node,
    NODE_PT_node_color_presets,
    NODE_MT_node_color_specials,
    NODE_MT_specials,
    NODE_PT_active_node_generic,
    NODE_PT_active_node_color,
    NODE_PT_active_node_properties,
    NODE_PT_backdrop,
    NODE_PT_quality,
    NODE_UL_interface_sockets,
    NODE_PT_grease_pencil,
    NODE_PT_grease_pencil_palettecolor,
    NODE_PT_grease_pencil_tools,
    NODE_PT_tools_grease_pencil_draw,
    NODE_PT_tools_grease_pencil_edit,
    NODE_PT_tools_grease_pencil_sculpt,
    NODE_PT_tools_grease_pencil_brush,
    NODE_PT_tools_grease_pencil_brushcurves,
)


if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
