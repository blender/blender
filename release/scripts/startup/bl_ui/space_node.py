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
from bpy.types import Header, Menu, Panel


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

        NODE_MT_editor_menus.draw_collapsible(context, layout)

        layout.prop(snode, "tree_type", text="", expand=True)

        if snode.tree_type == 'ShaderNodeTree':
            if scene.render.use_shading_nodes:
                layout.prop(snode, "shader_type", text="", expand=True)

            ob = context.object
            if (not scene.render.use_shading_nodes or snode.shader_type == 'OBJECT') and ob:
                row = layout.row()
                # disable material slot buttons when pinned, cannot find correct slot within id_from (#36589)
                row.enabled = not snode.pin
                # Show material.new when no active ID/slot exists
                if not id_from and ob.type in {'MESH', 'CURVE', 'SURFACE', 'FONT', 'METABALL'}:
                    row.template_ID(ob, "active_material", new="material.new")
                # Material ID, but not for Lamps
                if id_from and ob.type != 'LAMP':
                    row.template_ID(id_from, "active_material", new="material.new")

                # Don't show "Use Nodes" Button when Engine is BI for Lamps
                if snode_id and not (scene.render.use_shading_nodes == 0 and ob.type == 'LAMP'):
                    layout.prop(snode_id, "use_nodes")

            if snode.shader_type == 'WORLD':
                row = layout.row()
                row.enabled = not snode.pin
                row.template_ID(scene, "world", new="world.new")
                if snode_id:
                    row.prop(snode_id, "use_nodes")

        elif snode.tree_type == 'TextureNodeTree':
            layout.prop(snode, "texture_type", text="", expand=True)

            if id_from:
                if snode.texture_type == 'BRUSH':
                    layout.template_ID(id_from, "texture", new="texture.new")
                else:
                    layout.template_ID(id_from, "active_texture", new="texture.new")
            if snode_id:
                layout.prop(snode_id, "use_nodes")

        elif snode.tree_type == 'CompositorNodeTree':
            if snode_id:
                layout.prop(snode_id, "use_nodes")
                layout.prop(snode_id.render, "use_free_unused_nodes", text="Free Unused")
            layout.prop(snode, "show_backdrop")
            if snode.show_backdrop:
                row = layout.row(align=True)
                row.prop(snode, "backdrop_channels", text="", expand=True)
            layout.prop(snode, "use_auto_render")

        else:
            # Custom node tree is edited as independent ID block
            layout.template_ID(snode, "node_tree", new="node.new_node_tree")

        layout.prop(snode, "pin", text="")
        layout.operator("node.tree_path_parent", text="", icon='FILE_PARENT')

        layout.separator()

        # Snap
        row = layout.row(align=True)
        row.prop(toolsettings, "use_snap", text="")
        row.prop(toolsettings, "snap_node_element", icon_only=True)
        if toolsettings.snap_node_element != 'GRID':
            row.prop(toolsettings, "snap_target", text="")

        row = layout.row(align=True)
        row.operator("node.clipboard_copy", text="", icon='COPYDOWN')
        row.operator("node.clipboard_paste", text="", icon='PASTEDOWN')

        layout.template_running_jobs()


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

        # actual node submenus are added by draw functions from node categories


class NODE_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        layout.operator("node.properties", icon='MENU_PANEL')
        layout.operator("node.toolbar", icon='MENU_PANEL')

        layout.separator()

        layout.operator("view2d.zoom_in")
        layout.operator("view2d.zoom_out")

        layout.separator()

        layout.operator("node.view_selected")
        layout.operator("node.view_all")

        if context.space_data.show_backdrop:
            layout.separator()

            layout.operator("node.backimage_move", text="Backdrop move")
            layout.operator("node.backimage_zoom", text="Backdrop zoom in").factor = 1.2
            layout.operator("node.backimage_zoom", text="Backdrop zoom out").factor = 0.83333
            layout.operator("node.backimage_fit", text="Fit backdrop to available space")

        layout.separator()

        layout.operator("screen.area_dupli")
        layout.operator("screen.screen_full_area")


class NODE_MT_select(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("node.select_border")
        layout.operator("node.select_circle")

        layout.separator()
        layout.operator("node.select_all").action = 'TOGGLE'
        layout.operator("node.select_all", text="Inverse").action = 'INVERT'
        layout.operator("node.select_linked_from")
        layout.operator("node.select_linked_to")

        layout.separator()

        layout.operator("node.select_same_type")
        layout.operator("node.select_same_type_step").prev = True
        layout.operator("node.select_same_type_step").prev = False

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

        layout.operator("node.join", text="Join in new Frame")
        layout.operator("node.detach", text="Remove from Frame")

        layout.separator()

        layout.operator("node.link_make")
        layout.operator("node.link_make", text="Make and Replace Links").replace = True
        layout.operator("node.links_cut")
        layout.operator("node.links_detach")

        layout.separator()

        layout.operator("node.group_edit")
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

        layout.operator("node.read_renderlayers")
        layout.operator("node.read_fullsamplelayers")


class NODE_MT_node_color_presets(Menu):
    """Predefined node color"""
    bl_label = "Color Presets"
    preset_subdir = "node_color"
    preset_operator = "script.execute_preset"
    draw = Menu.draw_preset


class NODE_MT_node_color_specials(Menu):
    bl_label = "Node Color Specials"

    def draw(self, context):
        layout = self.layout

        layout.operator("node.node_copy_color", icon='COPY_ID')


class NODE_PT_active_node_generic(Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Node"
#    bl_options = {'HIDE_HEADER'}

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

    @classmethod
    def poll(cls, context):
        return context.active_node is not None

    def draw_header(self, context):
        node = context.active_node
        self.layout.prop(node, "use_custom_color", text="")

    def draw(self, context):
        layout = self.layout
        node = context.active_node

        layout.enabled = node.use_custom_color

        row = layout.row()
        col = row.column()
        col.menu("NODE_MT_node_color_presets")
        col.prop(node, "color", text="")
        col = row.column(align=True)
        col.operator("node.node_color_preset_add", text="", icon='ZOOMIN').remove_active = False
        col.operator("node.node_color_preset_add", text="", icon='ZOOMOUT').remove_active = True
        col.menu("NODE_MT_node_color_specials", text="", icon='DOWNARROW_HLT')


class NODE_PT_active_node_properties(Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Properties"

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
                socket.draw(context, row, node, socket.name)


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

        snode = context.space_data
        layout.active = snode.show_backdrop
        layout.prop(snode, "backdrop_channels", text="")
        layout.prop(snode, "backdrop_zoom", text="Zoom")

        col = layout.column(align=True)
        col.label(text="Offset:")
        col.prop(snode, "backdrop_x", text="X")
        col.prop(snode, "backdrop_y", text="Y")
        col.operator("node.backimage_move", text="Move")

        layout.operator("node.backimage_fit", text="Fit")


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
        col.prop(snode, "show_highlight")


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

        elif self.layout_type in {'GRID'}:
            layout.alignment = 'CENTER'
            layout.template_node_socket(color)


def node_draw_tree_view(layout, context):
    pass


if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
