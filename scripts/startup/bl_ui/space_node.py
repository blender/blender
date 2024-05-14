# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import (
    Header,
    Menu,
    Panel,
    UIList,
)
from bpy.app.translations import (
    pgettext_iface as iface_,
    contexts as i18n_contexts,
)
from bl_ui.utils import PresetPanel
from bl_ui.properties_grease_pencil_common import (
    AnnotationDataPanel,
)
from bl_ui.space_toolsystem_common import (
    ToolActivePanelHelper,
)
from bl_ui.properties_material import (
    EEVEE_MATERIAL_PT_settings,
    EEVEE_NEXT_MATERIAL_PT_settings,
    EEVEE_NEXT_MATERIAL_PT_settings_surface,
    EEVEE_NEXT_MATERIAL_PT_settings_volume,
    MATERIAL_PT_viewport,
)
from bl_ui.properties_world import (
    WORLD_PT_viewport_display
)
from bl_ui.properties_data_light import (
    DATA_PT_light,
    DATA_PT_EEVEE_light,
)


class NODE_HT_header(Header):
    bl_space_type = 'NODE_EDITOR'

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        snode = context.space_data
        overlay = snode.overlay
        snode_id = snode.id
        id_from = snode.id_from
        tool_settings = context.tool_settings
        is_compositor = snode.tree_type == 'CompositorNodeTree'

        layout.template_header()

        # Now expanded via the `ui_type`.
        # layout.prop(snode, "tree_type", text="")

        display_pin = True
        if snode.tree_type == 'ShaderNodeTree':
            layout.prop(snode, "shader_type", text="")

            ob = context.object
            if snode.shader_type == 'OBJECT' and ob:
                ob_type = ob.type

                NODE_MT_editor_menus.draw_collapsible(context, layout)

                # No shader nodes for EEVEE lights.
                if snode_id and not (context.engine == 'BLENDER_EEVEE' and ob_type == 'LIGHT'):
                    row = layout.row()
                    row.prop(snode_id, "use_nodes")

                layout.separator_spacer()

                types_that_support_material = {
                    'MESH', 'CURVE', 'SURFACE', 'FONT', 'META', 'GPENCIL', 'VOLUME', 'CURVES', 'POINTCLOUD',
                }
                # disable material slot buttons when pinned, cannot find correct slot within id_from (#36589)
                # disable also when the selected object does not support materials
                has_material_slots = not snode.pin and ob_type in types_that_support_material

                if ob_type != 'LIGHT':
                    row = layout.row()
                    row.enabled = has_material_slots
                    row.ui_units_x = 4
                    row.popover(panel="NODE_PT_material_slots")

                row = layout.row()
                row.enabled = has_material_slots

                # Show material.new when no active ID/slot exists
                if not id_from and ob_type in types_that_support_material:
                    row.template_ID(ob, "active_material", new="material.new")
                # Material ID, but not for Lights
                if id_from and ob_type != 'LIGHT':
                    row.template_ID(id_from, "active_material", new="material.new")

            if snode.shader_type == 'WORLD':
                NODE_MT_editor_menus.draw_collapsible(context, layout)
                world = scene.world

                if snode_id:
                    row = layout.row()
                    row.prop(snode_id, "use_nodes")

                    if world and world.use_eevee_finite_volume:
                        row.operator("world.convert_volume_to_mesh", emboss=False, icon='WORLD', text="Convert Volume")

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

        elif snode.tree_type == 'GeometryNodeTree':
            layout.prop(snode, "geometry_nodes_type", text="")
            NODE_MT_editor_menus.draw_collapsible(context, layout)
            layout.separator_spacer()

            if snode.geometry_nodes_type == 'MODIFIER':
                ob = context.object

                row = layout.row()
                if snode.pin:
                    row.enabled = False
                    row.template_ID(snode, "node_tree", new="node.new_geometry_node_group_assign")
                elif ob:
                    active_modifier = ob.modifiers.active
                    if active_modifier and active_modifier.type == 'NODES':
                        if active_modifier.node_group:
                            row.template_ID(active_modifier, "node_group", new="object.geometry_node_tree_copy_assign")
                        else:
                            row.template_ID(active_modifier, "node_group", new="node.new_geometry_node_group_assign")
                    else:
                        row.template_ID(snode, "node_tree", new="node.new_geometry_nodes_modifier")
            else:
                layout.template_ID(snode, "geometry_nodes_tool_tree", new="node.new_geometry_node_group_tool")
                if snode.node_tree:
                    layout.popover(panel="NODE_PT_geometry_node_tool_object_types", text="Types")
                    layout.popover(panel="NODE_PT_geometry_node_tool_mode", text="Modes")
                    layout.popover(panel="NODE_PT_geometry_node_tool_options", text="Options")
                display_pin = False
        else:
            # Custom node tree is edited as independent ID block
            NODE_MT_editor_menus.draw_collapsible(context, layout)

            layout.separator_spacer()

            layout.template_ID(snode, "node_tree", new="node.new_node_tree")

        # Put pin next to ID block
        if not is_compositor and display_pin:
            layout.prop(snode, "pin", text="", emboss=False)

        layout.separator_spacer()

        # Put pin on the right for Compositing
        if is_compositor:
            layout.prop(snode, "pin", text="", emboss=False)

        layout.operator("node.tree_path_parent", text="", icon='FILE_PARENT')

        # Backdrop
        if is_compositor:
            row = layout.row(align=True)
            row.prop(snode, "show_backdrop", toggle=True)
            sub = row.row(align=True)
            sub.active = snode.show_backdrop
            sub.prop(snode, "backdrop_channels", icon_only=True, text="", expand=True)

        # Snap
        row = layout.row(align=True)
        row.prop(tool_settings, "use_snap_node", text="")
        row.prop(tool_settings, "snap_node_element", icon_only=True)
        if tool_settings.snap_node_element != 'GRID':
            row.prop(tool_settings, "snap_target", text="")

        # Overlay toggle & popover
        row = layout.row(align=True)
        row.prop(overlay, "show_overlays", icon='OVERLAY', text="")
        sub = row.row(align=True)
        sub.active = overlay.show_overlays
        sub.popover(panel="NODE_PT_overlay", text="")


class NODE_MT_editor_menus(Menu):
    bl_idname = "NODE_MT_editor_menus"
    bl_label = ""

    def draw(self, _context):
        layout = self.layout
        layout.menu("NODE_MT_view")
        layout.menu("NODE_MT_select")
        layout.menu("NODE_MT_add")
        layout.menu("NODE_MT_node")


class NODE_MT_add(bpy.types.Menu):
    bl_space_type = 'NODE_EDITOR'
    bl_label = "Add"
    bl_translation_context = i18n_contexts.operator_default
    bl_options = {'SEARCH_ON_KEY_PRESS'}

    def draw(self, context):
        import nodeitems_utils

        layout = self.layout

        if layout.operator_context == 'EXEC_REGION_WIN':
            layout.operator_context = 'INVOKE_REGION_WIN'
            layout.operator("WM_OT_search_single_menu", text="Search...", icon='VIEWZOOM').menu_idname = "NODE_MT_add"
            layout.separator()

        layout.operator_context = 'INVOKE_REGION_WIN'

        snode = context.space_data
        if snode.tree_type == 'GeometryNodeTree':
            layout.menu_contents("NODE_MT_geometry_node_add_all")
        elif snode.tree_type == 'CompositorNodeTree':
            layout.menu_contents("NODE_MT_compositor_node_add_all")
        elif snode.tree_type == 'ShaderNodeTree':
            layout.menu_contents("NODE_MT_shader_node_add_all")
        elif snode.tree_type == 'TextureNodeTree':
            layout.menu_contents("NODE_MT_texture_node_add_all")
        elif nodeitems_utils.has_node_categories(context):
            # Actual node sub-menus are defined by draw functions from node categories.
            nodeitems_utils.draw_node_categories_menu(self, context)


class NODE_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        snode = context.space_data

        layout.prop(snode, "show_region_toolbar")
        layout.prop(snode, "show_region_ui")

        layout.separator()

        sub = layout.column()
        sub.operator_context = 'EXEC_REGION_WIN'
        sub.operator("view2d.zoom_in")
        sub.operator("view2d.zoom_out")

        layout.separator()

        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("node.view_selected")
        layout.operator("node.view_all")

        if context.space_data.show_backdrop:
            layout.separator()

            layout.operator("node.backimage_move", text="Backdrop Move")
            layout.operator("node.backimage_zoom", text="Backdrop Zoom In").factor = 1.2
            layout.operator("node.backimage_zoom", text="Backdrop Zoom Out").factor = 1.0 / 1.2
            layout.operator("node.backimage_fit", text="Fit Backdrop to Available Space")

        layout.separator()

        layout.menu("INFO_MT_area")


class NODE_MT_select(Menu):
    bl_label = "Select"

    def draw(self, _context):
        layout = self.layout

        layout.operator("node.select_all", text="All").action = 'SELECT'
        layout.operator("node.select_all", text="None").action = 'DESELECT'
        layout.operator("node.select_all", text="Invert").action = 'INVERT'

        layout.separator()

        layout.operator("node.select_box").tweak = False
        layout.operator("node.select_circle")
        layout.operator_menu_enum("node.select_lasso", "mode")

        layout.separator()
        layout.operator("node.select_linked_from", text="Linked from")
        layout.operator("node.select_linked_to", text="Linked to")

        layout.separator()

        layout.operator_menu_enum("node.select_grouped", "type", text="Select Grouped")
        layout.operator("node.select_same_type_step", text="Activate Same Type Previous").prev = True
        layout.operator("node.select_same_type_step", text="Activate Same Type Next").prev = False

        layout.separator()

        layout.operator("node.find_node", text="Find Node...")


class NODE_MT_node(Menu):
    bl_label = "Node"

    def draw(self, context):
        layout = self.layout
        snode = context.space_data
        is_compositor = snode.tree_type == 'CompositorNodeTree'

        layout.operator("transform.translate").view2d_edge_pan = True
        layout.operator("transform.rotate")
        layout.operator("transform.resize")

        layout.separator()
        layout.operator("node.clipboard_copy", text="Copy", icon='COPYDOWN')
        layout.operator_context = 'EXEC_DEFAULT'
        layout.operator("node.clipboard_paste", text="Paste", icon='PASTEDOWN')
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("node.duplicate_move", icon='DUPLICATE')
        layout.operator("node.duplicate_move_linked")

        layout.separator()
        layout.operator("node.delete", icon='X')
        layout.operator("node.delete_reconnect")

        layout.separator()
        layout.operator("node.join", text="Join in New Frame")
        layout.operator("node.detach", text="Remove from Frame")

        layout.separator()
        props = layout.operator("wm.call_panel", text="Rename...")
        props.name = "TOPBAR_PT_name"
        props.keep_open = False

        layout.separator()
        layout.operator("node.link_make").replace = False
        layout.operator("node.link_make", text="Make and Replace Links").replace = True
        layout.operator("node.links_cut")
        layout.operator("node.links_detach")
        layout.operator("node.links_mute")

        layout.separator()
        layout.operator("node.group_make", icon='NODETREE')
        layout.operator("node.group_insert", text="Insert Into Group")
        layout.operator("node.group_edit").exit = False
        layout.operator("node.group_ungroup")

        layout.separator()
        layout.menu("NODE_MT_context_menu_show_hide_menu")

        if is_compositor:
            layout.separator()
            layout.operator("node.read_viewlayers", icon='RENDERLAYERS')


class NODE_MT_view_pie(Menu):
    bl_label = "View"

    def draw(self, _context):
        layout = self.layout

        pie = layout.menu_pie()
        pie.operator("node.view_all")
        pie.operator("node.view_selected", icon='ZOOM_SELECTED')


class NODE_PT_active_tool(ToolActivePanelHelper, Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Tool"


class NODE_PT_material_slots(Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Slot"
    bl_ui_units_x = 12

    def draw_header(self, context):
        ob = context.object
        self.bl_label = (
            iface_("Slot {:d}").format(ob.active_material_index + 1) if ob.material_slots else
            iface_("Slot")
        )

    # Duplicate part of `EEVEE_MATERIAL_PT_context_material`.
    def draw(self, context):
        layout = self.layout
        row = layout.row()
        col = row.column()

        ob = context.object
        col.template_list("MATERIAL_UL_matslots", "", ob, "material_slots", ob, "active_material_index")

        col = row.column(align=True)
        col.operator("object.material_slot_add", icon='ADD', text="")
        col.operator("object.material_slot_remove", icon='REMOVE', text="")

        col.separator()

        col.menu("MATERIAL_MT_context_menu", icon='DOWNARROW_HLT', text="")

        if len(ob.material_slots) > 1:
            col.separator()

            col.operator("object.material_slot_move", icon='TRIA_UP', text="").direction = 'UP'
            col.operator("object.material_slot_move", icon='TRIA_DOWN', text="").direction = 'DOWN'

        if ob.mode == 'EDIT':
            row = layout.row(align=True)
            row.operator("object.material_slot_assign", text="Assign")
            row.operator("object.material_slot_select", text="Select")
            row.operator("object.material_slot_deselect", text="Deselect")


class NODE_PT_geometry_node_tool_object_types(Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Object Types"
    bl_ui_units_x = 8

    def draw(self, context):
        layout = self.layout

        snode = context.space_data
        group = snode.node_tree

        types = [
            ("is_type_mesh", "Mesh", 'MESH_DATA'),
            ("is_type_curve", "Hair Curves", 'CURVES_DATA'),
        ]
        if context.preferences.experimental.use_new_point_cloud_type:
            types.append(("is_type_point_cloud", "Point Cloud", 'POINTCLOUD_DATA'))

        col = layout.column()
        col.active = group.is_tool
        for prop, name, icon in types:
            row = col.row(align=True)
            row.label(text=name, icon=icon)
            row.prop(group, prop, text="")


class NODE_PT_geometry_node_tool_mode(Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Modes"
    bl_ui_units_x = 8

    def draw(self, context):
        layout = self.layout

        snode = context.space_data
        group = snode.node_tree

        modes = (
            ("is_mode_object", "Object Mode", 'OBJECT_DATAMODE'),
            ("is_mode_edit", "Edit Mode", 'EDITMODE_HLT'),
            ("is_mode_sculpt", "Sculpt Mode", 'SCULPTMODE_HLT'),
        )

        col = layout.column()
        col.active = group.is_tool
        for prop, name, icon in modes:
            row = col.row(align=True)
            row.label(text=name, icon=icon)
            row.prop(group, prop, text="")


class NODE_PT_geometry_node_tool_options(Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Options"
    bl_ui_units_x = 8

    def draw(self, context):
        layout = self.layout

        snode = context.space_data
        group = snode.node_tree

        layout.prop(group, "use_wait_for_click")


class NODE_PT_node_color_presets(PresetPanel, Panel):
    """Predefined node color"""
    bl_label = "Color Presets"
    preset_subdir = "node_color"
    preset_operator = "script.execute_preset"
    preset_add_operator = "node.node_color_preset_add"


class NODE_MT_node_color_context_menu(Menu):
    bl_label = "Node Color Specials"

    def draw(self, _context):
        layout = self.layout

        layout.operator("node.node_copy_color", icon='COPY_ID')


class NODE_MT_context_menu_show_hide_menu(Menu):
    bl_label = "Show/Hide"

    def draw(self, context):
        snode = context.space_data
        is_compositor = snode.tree_type == 'CompositorNodeTree'

        layout = self.layout

        layout.operator("node.mute_toggle", text="Mute")

        # Node previews are only available in the Compositor.
        if is_compositor:
            layout.operator("node.preview_toggle", text="Node Preview")

        layout.operator("node.options_toggle", text="Node Options")

        layout.separator()

        layout.operator("node.hide_socket_toggle", text="Unconnected Sockets")
        layout.operator("node.hide_toggle", text="Collapse")
        layout.operator("node.collapse_hide_unused_toggle")


class NODE_MT_context_menu_select_menu(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("node.select_grouped", text="Select Grouped...").extend = False

        layout.separator()

        layout.operator("node.select_linked_from")
        layout.operator("node.select_linked_to")

        layout.separator()

        layout.operator("node.select_same_type_step", text="Activate Same Type Previous").prev = True
        layout.operator("node.select_same_type_step", text="Activate Same Type Next").prev = False


class NODE_MT_context_menu(Menu):
    bl_label = "Node"

    def draw(self, context):
        snode = context.space_data
        is_nested = (len(snode.path) > 1)
        is_geometrynodes = snode.tree_type == 'GeometryNodeTree'

        selected_nodes_len = len(context.selected_nodes)
        active_node = context.active_node

        layout = self.layout

        # If no nodes are selected.
        if selected_nodes_len == 0:
            layout.operator_context = 'INVOKE_DEFAULT'
            layout.menu("NODE_MT_add", icon='ADD')
            layout.operator("node.clipboard_paste", text="Paste", icon='PASTEDOWN')

            layout.separator()

            layout.operator("node.find_node", text="Find...", icon='VIEWZOOM')

            layout.separator()

            if is_geometrynodes:
                layout.operator_context = 'INVOKE_DEFAULT'
                layout.operator("node.select", text="Clear Viewer", icon='HIDE_ON').clear_viewer = True

            layout.operator("node.links_cut")
            layout.operator("node.links_mute")

            if is_nested:
                layout.separator()

                layout.operator("node.tree_path_parent", text="Exit Group", icon='FILE_PARENT')

            return

        if is_geometrynodes:
            layout.operator_context = 'INVOKE_DEFAULT'
            layout.operator("node.link_viewer", text="Link to Viewer", icon='HIDE_OFF')

            layout.separator()

        layout.operator("node.clipboard_copy", text="Copy", icon='COPYDOWN')
        layout.operator("node.clipboard_paste", text="Paste", icon='PASTEDOWN')

        layout.operator_context = 'INVOKE_DEFAULT'
        layout.operator("node.duplicate_move", icon='DUPLICATE')

        layout.separator()

        layout.operator("node.delete", icon='X')
        layout.operator_context = 'EXEC_REGION_WIN'
        layout.operator("node.delete_reconnect", text="Dissolve")

        if selected_nodes_len > 1:
            layout.separator()

            layout.operator("node.link_make").replace = False
            layout.operator("node.link_make", text="Make and Replace Links").replace = True
            layout.operator("node.links_detach")

        layout.separator()

        layout.operator("node.group_make", text="Make Group", icon='NODETREE')
        layout.operator("node.group_insert", text="Insert Into Group")

        if active_node and active_node.type == 'GROUP':
            layout.operator("node.group_edit").exit = False
            layout.operator("node.group_ungroup", text="Ungroup")

        if is_nested:
            layout.operator("node.tree_path_parent", text="Exit Group", icon='FILE_PARENT')

        layout.separator()

        layout.operator("node.join", text="Join in New Frame")
        layout.operator("node.detach", text="Remove from Frame")

        layout.separator()

        props = layout.operator("wm.call_panel", text="Rename...")
        props.name = "TOPBAR_PT_name"
        props.keep_open = False

        layout.separator()

        layout.menu("NODE_MT_context_menu_select_menu")
        layout.menu("NODE_MT_context_menu_show_hide_menu")

        if active_node:
            layout.separator()
            props = layout.operator("wm.doc_view_manual", text="Online Manual", icon='URL')
            props.doc_id = active_node.bl_idname


class NODE_PT_active_node_generic(Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Node"
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
    bl_category = "Node"
    bl_label = "Color"
    bl_options = {'DEFAULT_CLOSED'}
    bl_parent_id = "NODE_PT_active_node_generic"

    @classmethod
    def poll(cls, context):
        return context.active_node is not None

    def draw_header(self, context):
        node = context.active_node
        self.layout.prop(node, "use_custom_color", text="")

    def draw_header_preset(self, _context):
        NODE_PT_node_color_presets.draw_panel_header(self.layout)

    def draw(self, context):
        layout = self.layout
        node = context.active_node

        layout.enabled = node.use_custom_color

        row = layout.row()
        row.prop(node, "color", text="")
        row.menu("NODE_MT_node_color_context_menu", text="", icon='DOWNARROW_HLT')


class NODE_PT_active_node_properties(Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Node"
    bl_label = "Properties"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.active_node is not None

    def draw(self, context):
        layout = self.layout
        node = context.active_node
        layout.template_node_inputs(node)


class NODE_PT_texture_mapping(Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Node"
    bl_label = "Texture Mapping"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    @classmethod
    def poll(cls, context):
        node = context.active_node
        return node and hasattr(node, "texture_mapping") and (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        node = context.active_node
        mapping = node.texture_mapping

        layout.prop(mapping, "vector_type")

        layout.separator()

        col = layout.column(align=True)
        col.prop(mapping, "mapping_x", text="Projection X")
        col.prop(mapping, "mapping_y", text="Y")
        col.prop(mapping, "mapping_z", text="Z")

        layout.separator()

        layout.prop(mapping, "translation")
        layout.prop(mapping, "rotation")
        layout.prop(mapping, "scale")


# Node Backdrop options
class NODE_PT_backdrop(Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "View"
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
        layout.use_property_decorate = False

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
    bl_category = "Options"
    bl_label = "Performance"

    @classmethod
    def poll(cls, context):
        snode = context.space_data
        return snode.tree_type == 'CompositorNodeTree' and snode.node_tree is not None

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        rd = scene.render

        snode = context.space_data
        tree = snode.node_tree

        col = layout.column()
        col.prop(rd, "compositor_device", text="Device")
        col.prop(rd, "compositor_precision", text="Precision")

        col = layout.column()
        col.prop(tree, "use_viewer_border")

        col = layout.column()
        col.prop(snode, "use_auto_render")


class NODE_PT_overlay(Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Overlays"
    bl_ui_units_x = 7

    def draw(self, context):
        layout = self.layout
        layout.label(text="Node Editor Overlays")

        snode = context.space_data
        overlay = snode.overlay

        layout.active = overlay.show_overlays

        col = layout.column()
        col.prop(overlay, "show_wire_color", text="Wire Colors")

        col.separator()

        col.prop(overlay, "show_context_path", text="Context Path")
        col.prop(snode, "show_annotation", text="Annotations")

        if snode.supports_previews:
            col.separator()
            col.prop(overlay, "show_previews", text="Previews")
            if snode.tree_type == 'ShaderNodeTree':
                row = col.row()
                row.prop(overlay, "preview_shape", expand=True)
                row.active = overlay.show_previews

        if snode.tree_type == 'GeometryNodeTree':
            col.separator()
            col.prop(overlay, "show_timing", text="Timings")
            col.prop(overlay, "show_named_attributes", text="Named Attributes")

        if snode.tree_type == 'CompositorNodeTree':
            col.prop(overlay, "show_timing", text="Timings")


class NODE_MT_node_tree_interface_context_menu(Menu):
    bl_label = "Node Tree Interface Specials"

    def draw(self, _context):
        layout = self.layout

        layout.operator("node.interface_item_duplicate", icon='DUPLICATE')


class NODE_PT_node_tree_interface(Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Group"
    bl_label = "Group Sockets"

    @classmethod
    def poll(cls, context):
        snode = context.space_data
        if snode is None:
            return False
        tree = snode.edit_tree
        if tree is None:
            return False
        if tree.is_embedded_data:
            return False
        return True

    def draw(self, context):
        layout = self.layout
        snode = context.space_data
        tree = snode.edit_tree

        split = layout.row()

        split.template_node_tree_interface(tree.interface)

        ops_col = split.column(align=True)
        ops_col.operator_menu_enum("node.interface_item_new", "item_type", icon='ADD', text="")
        ops_col.operator("node.interface_item_remove", icon='REMOVE', text="")
        ops_col.separator()
        ops_col.menu("NODE_MT_node_tree_interface_context_menu", icon='DOWNARROW_HLT', text="")

        ops_col.separator()

        active_item = tree.interface.active
        if active_item is not None:
            layout.use_property_split = True
            layout.use_property_decorate = False

            if active_item.item_type == 'SOCKET':
                layout.prop(active_item, "socket_type", text="Type")
                layout.prop(active_item, "description")
                # Display descriptions only for Geometry Nodes, since it's only used in the modifier panel.
                if tree.type == 'GEOMETRY':
                    field_socket_types = {
                        "NodeSocketInt",
                        "NodeSocketColor",
                        "NodeSocketVector",
                        "NodeSocketBool",
                        "NodeSocketFloat",
                    }
                    if active_item.socket_type in field_socket_types:
                        if 'OUTPUT' in active_item.in_out:
                            layout.prop(active_item, "attribute_domain")
                        layout.prop(active_item, "default_attribute_name")
                if hasattr(active_item, "draw"):
                    active_item.draw(context, layout)

            if active_item.item_type == 'PANEL':
                layout.prop(active_item, "description")
                layout.prop(active_item, "default_closed", text="Closed by Default")

            layout.use_property_split = False


class NODE_PT_node_tree_properties(Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Group"
    bl_label = "Group"

    @classmethod
    def poll(cls, context):
        snode = context.space_data
        if snode is None:
            return False
        group = snode.edit_tree
        if group is None:
            return False
        if group.is_embedded_data:
            return False
        return True

    def draw(self, context):
        layout = self.layout
        snode = context.space_data
        group = snode.edit_tree
        layout.use_property_split = True
        layout.use_property_decorate = False

        layout.prop(group, "name", text="Name")

        if group.asset_data:
            layout.prop(group.asset_data, "description", text="Description")
        else:
            layout.prop(group, "description", text="Description")

        layout.prop(group, "color_tag")

        if group.bl_idname == "GeometryNodeTree":
            header, body = layout.panel("group_usage")
            header.label(text="Usage")
            if body:
                col = body.column(align=True)
                col.prop(group, "is_modifier")
                col.prop(group, "is_tool")


# Grease Pencil properties
class NODE_PT_annotation(AnnotationDataPanel, Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "View"
    bl_options = {'DEFAULT_CLOSED'}

    # NOTE: this is just a wrapper around the generic GP Panel

    @classmethod
    def poll(cls, context):
        snode = context.space_data
        return snode is not None and snode.node_tree is not None


def node_draw_tree_view(_layout, _context):
    pass


# Adapt properties editor panel to display in node editor. We have to
# copy the class rather than inherit due to the way bpy registration works.
def node_panel(cls):
    node_cls_dict = cls.__dict__.copy()

    # Needed for re-registration.
    node_cls_dict.pop("bl_rna", None)

    node_cls = type('NODE_' + cls.__name__, cls.__bases__, node_cls_dict)

    node_cls.bl_space_type = 'NODE_EDITOR'
    node_cls.bl_region_type = 'UI'
    node_cls.bl_category = "Options"
    if hasattr(node_cls, "bl_parent_id"):
        node_cls.bl_parent_id = "NODE_" + node_cls.bl_parent_id

    return node_cls


classes = (
    NODE_HT_header,
    NODE_MT_editor_menus,
    NODE_MT_add,
    NODE_MT_view,
    NODE_MT_select,
    NODE_MT_node,
    NODE_MT_node_color_context_menu,
    NODE_MT_context_menu_show_hide_menu,
    NODE_MT_context_menu_select_menu,
    NODE_MT_context_menu,
    NODE_MT_view_pie,
    NODE_PT_material_slots,
    NODE_PT_geometry_node_tool_object_types,
    NODE_PT_geometry_node_tool_mode,
    NODE_PT_geometry_node_tool_options,
    NODE_PT_node_color_presets,
    NODE_PT_node_tree_properties,
    NODE_MT_node_tree_interface_context_menu,
    NODE_PT_node_tree_interface,
    NODE_PT_active_node_generic,
    NODE_PT_active_node_color,
    NODE_PT_texture_mapping,
    NODE_PT_active_tool,
    NODE_PT_backdrop,
    NODE_PT_quality,
    NODE_PT_annotation,
    NODE_PT_overlay,
    NODE_PT_active_node_properties,

    node_panel(EEVEE_MATERIAL_PT_settings),
    node_panel(EEVEE_NEXT_MATERIAL_PT_settings),
    node_panel(EEVEE_NEXT_MATERIAL_PT_settings_surface),
    node_panel(EEVEE_NEXT_MATERIAL_PT_settings_volume),
    node_panel(MATERIAL_PT_viewport),
    node_panel(WORLD_PT_viewport_display),
    node_panel(DATA_PT_light),
    node_panel(DATA_PT_EEVEE_light),
)


if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
