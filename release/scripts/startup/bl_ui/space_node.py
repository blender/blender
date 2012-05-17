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
        ob = context.object
        snode = context.space_data
        snode_id = snode.id
        id_from = snode.id_from

        row = layout.row(align=True)
        row.template_header()

        if context.area.show_menus:
            row.menu("NODE_MT_view")
            row.menu("NODE_MT_select")
            row.menu("NODE_MT_add")
            row.menu("NODE_MT_node")

        layout.prop(snode, "tree_type", text="", expand=True)

        if snode.tree_type == 'SHADER':
            if scene.render.use_shading_nodes:
                layout.prop(snode, "shader_type", text="", expand=True)

            if (not scene.render.use_shading_nodes or snode.shader_type == 'OBJECT') and ob:
                # Show material.new when no active ID/slot exists
                if not id_from and ob.type in {'MESH', 'CURVE', 'SURFACE', 'FONT', 'METABALL'}:
                    layout.template_ID(ob, "active_material", new="material.new")
                # Material ID, but not for Lamps
                if id_from and ob.type != 'LAMP':
                    layout.template_ID(id_from, "active_material", new="material.new")
                # Don't show "Use Nodes" Button when Engine is BI for Lamps
                if snode_id and not (scene.render.use_shading_nodes == 0 and ob.type == 'LAMP'):
                    layout.prop(snode_id, "use_nodes")

            if snode.shader_type == 'WORLD':
                layout.template_ID(scene, "world", new="world.new")
                if snode_id:
                    layout.prop(snode_id, "use_nodes")

        elif snode.tree_type == 'TEXTURE':
            layout.prop(snode, "texture_type", text="", expand=True)

            if id_from:
                if snode.texture_type == 'BRUSH':
                    layout.template_ID(id_from, "texture", new="texture.new")
                else:
                    layout.template_ID(id_from, "active_texture", new="texture.new")
            if snode_id:
                layout.prop(snode_id, "use_nodes")

        elif snode.tree_type == 'COMPOSITING':
            layout.prop(snode_id, "use_nodes")
            layout.prop(snode_id.render, "use_free_unused_nodes", text="Free Unused")
            layout.prop(snode, "show_backdrop")
            if snode.show_backdrop:
                row = layout.row(align=True)
                row.prop(snode, "backdrop_channels", text="", expand=True)
            layout.prop(snode, "use_auto_render")

        layout.separator()

        layout.template_running_jobs()


class NODE_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        layout.operator("node.properties", icon='MENU_PANEL')
        layout.separator()

        layout.operator("view2d.zoom_in")
        layout.operator("view2d.zoom_out")

        layout.separator()

        layout.operator("node.view_all")

        if context.space_data.show_backdrop:
            layout.separator()

            layout.operator("node.backimage_move", text="Backdrop move")
            layout.operator("node.backimage_zoom", text="Backdrop zoom in").factor = 1.2
            layout.operator("node.backimage_zoom", text="Backdrop zoom out").factor = 0.833

        layout.separator()

        layout.operator("screen.area_dupli")
        layout.operator("screen.screen_full_area")


class NODE_MT_select(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("node.select_border")

        layout.separator()
        layout.operator("node.select_all")
        layout.operator("node.select_linked_from")
        layout.operator("node.select_linked_to")
        layout.operator("node.select_same_type")
        layout.operator("node.select_same_type_next")
        layout.operator("node.select_same_type_prev")


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
        layout.operator("node.link_make")
        layout.operator("node.link_make", text="Make and Replace Links").replace = True
        layout.operator("node.links_cut")

        layout.separator()
        layout.operator("node.group_edit")
        layout.operator("node.group_ungroup")
        layout.operator("node.group_make")

        layout.separator()

        layout.operator("node.hide_toggle")
        layout.operator("node.mute_toggle")
        layout.operator("node.preview_toggle")
        layout.operator("node.hide_socket_toggle")
        layout.operator("node.options_toggle")

        layout.separator()

        layout.operator("node.show_cyclic_dependencies")
        layout.operator("node.read_renderlayers")
        layout.operator("node.read_fullsamplelayers")


# Node Backdrop options
class NODE_PT_properties(Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Backdrop"

    @classmethod
    def poll(cls, context):
        snode = context.space_data
        return snode.tree_type == 'COMPOSITING'

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

class NODE_PT_quality(bpy.types.Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Quality"

    @classmethod
    def poll(cls, context):
        snode = context.space_data
        return snode.tree_type == 'COMPOSITING' and snode.node_tree is not None

    def draw(self, context):
        layout = self.layout
        snode = context.space_data
        tree = snode.node_tree

        layout.prop(tree, "render_quality", text="Render")
        layout.prop(tree, "edit_quality", text="Edit")
        layout.prop(tree, "chunksize")
        layout.prop(tree, "use_opencl")

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
