# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Panel, Menu
from bpy.props import StringProperty
from bpy.app.translations import contexts as i18n_contexts

from . import operators

from .utils.constants import blend_types, geo_combine_operations, operations
from .utils.nodes import get_nodes_links, NWBaseMenu


def socket_to_icon(socket):
    socket_type = socket.type

    if socket_type == "CUSTOM":
        return "RADIOBUT_OFF"

    if socket_type == "VALUE":
        socket_type = "FLOAT"

    return "NODE_SOCKET_" + socket_type


def drawlayout(context, layout, mode='non-panel'):
    tree_type = context.space_data.tree_type

    col = layout.column(align=True)
    col.menu(NWMergeNodesMenu.bl_idname)
    col.separator()

    if tree_type == 'ShaderNodeTree':
        col = layout.column(align=True)
        col.operator(operators.NWAddTextureSetup.bl_idname, text="Add Texture Setup", icon='NODE_SEL')
        col.operator(operators.NWAddPrincipledSetup.bl_idname, text="Add Principled Setup", icon='NODE_SEL')
        col.separator()

    col = layout.column(align=True)
    col.operator(operators.NWDetachOutputs.bl_idname, icon='UNLINKED')
    col.operator(operators.NWSwapLinks.bl_idname)
    col.menu(NWAddReroutesMenu.bl_idname, icon='LAYER_USED')
    col.separator()

    col = layout.column(align=True)
    col.menu(NWLinkActiveToSelectedMenu.bl_idname, icon='LINKED')
    if tree_type != 'GeometryNodeTree':
        col.operator(operators.NWLinkToOutputNode.bl_idname, icon='DRIVER')
    col.separator()

    col = layout.column(align=True)
    if mode == 'panel':
        row = col.row(align=True)
        row.operator(operators.NWClearLabel.bl_idname).option = True
        row.operator(operators.NWModifyLabels.bl_idname)
    else:
        col.operator(operators.NWClearLabel.bl_idname).option = True
        col.operator(operators.NWModifyLabels.bl_idname)
    col.menu(NWBatchChangeNodesMenu.bl_idname, text="Batch Change", text_ctxt=i18n_contexts.operator_default)
    col.separator()
    col.menu(NWCopyToSelectedMenu.bl_idname)
    col.separator()

    col = layout.column(align=True)
    if tree_type == 'CompositorNodeTree':
        col.operator(operators.NWResetBG.bl_idname, icon='ZOOM_PREVIOUS')
    if tree_type != 'GeometryNodeTree':
        col.operator(operators.NWReloadImages.bl_idname, icon='FILE_REFRESH')
    col.separator()

    col = layout.column(align=True)
    col.operator('node.join', icon='STICKY_UVS_LOC')
    col.separator()

    col = layout.column(align=True)
    col.operator(operators.NWAlignNodes.bl_idname, icon='CENTER_ONLY')
    col.operator(operators.NWCenterNodes.bl_idname, icon='SNAP_FACE_CENTER')
    col.separator()

    col = layout.column(align=True)
    col.operator(operators.NWDeleteUnused.bl_idname, icon='CANCEL')


class NodeWranglerPanel(Panel, NWBaseMenu):
    bl_idname = "NODE_PT_nw_node_wrangler"
    bl_space_type = 'NODE_EDITOR'
    bl_label = "Node Wrangler"
    bl_region_type = "UI"
    bl_category = "Node Wrangler"

    prepend: StringProperty(
        name='prepend',
    )
    append: StringProperty()
    remove: StringProperty()

    def draw(self, context):
        self.layout.label(text="(Quick Access: Shift+W)")
        drawlayout(context, self.layout, mode='panel')


#
#  M E N U S
#
class NodeWranglerMenu(Menu, NWBaseMenu):
    bl_idname = "NODE_MT_nw_node_wrangler_menu"
    bl_label = "Node Wrangler"

    def draw(self, context):
        self.layout.operator_context = 'INVOKE_DEFAULT'
        drawlayout(context, self.layout)


class NWMergeNodesMenu(Menu, NWBaseMenu):
    bl_idname = "NODE_MT_nw_merge_nodes_menu"
    bl_label = "Merge Selected Nodes"

    def draw(self, context):
        type = context.space_data.tree_type
        layout = self.layout
        if type == 'ShaderNodeTree':
            layout.menu(NWMergeShadersMenu.bl_idname, text="Use Shaders")
        if type == 'GeometryNodeTree':
            layout.menu(NWMergeGeometryMenu.bl_idname, text="Use Geometry Nodes")
            layout.menu(NWMergeMathMenu.bl_idname, text="Use Math Nodes")
        else:
            layout.menu(NWMergeMixMenu.bl_idname, text="Use Mix Nodes")
            layout.menu(NWMergeMathMenu.bl_idname, text="Use Math Nodes")
            props = layout.operator(operators.NWMergeNodes.bl_idname, text="Use Z-Combine Nodes")
            props.mode = 'MIX'
            props.merge_type = 'ZCOMBINE'
            props = layout.operator(operators.NWMergeNodes.bl_idname, text="Use Alpha Over Nodes")
            props.mode = 'MIX'
            props.merge_type = 'ALPHAOVER'


class NWMergeGeometryMenu(Menu, NWBaseMenu):
    bl_idname = "NODE_MT_nw_merge_geometry_menu"
    bl_label = "Merge Selected Nodes using Geometry Nodes"

    def draw(self, context):
        layout = self.layout
        # The boolean node + Join Geometry node
        for type, name, description in geo_combine_operations:
            props = layout.operator(operators.NWMergeNodes.bl_idname, text=name, text_ctxt=i18n_contexts.id_nodetree)
            props.mode = type
            props.merge_type = 'GEOMETRY'


class NWMergeShadersMenu(Menu, NWBaseMenu):
    bl_idname = "NODE_MT_nw_merge_shaders_menu"
    bl_label = "Merge Selected Nodes using Shaders"

    def draw(self, context):
        layout = self.layout
        for type in ('MIX', 'ADD'):
            name = f'{type.capitalize()} Shader'
            props = layout.operator(operators.NWMergeNodes.bl_idname, text=name, text_ctxt=i18n_contexts.default)
            props.mode = type
            props.merge_type = 'SHADER'


class NWMergeMixMenu(Menu, NWBaseMenu):
    bl_idname = "NODE_MT_nw_merge_mix_menu"
    bl_label = "Merge Selected Nodes using Mix"

    def draw(self, context):
        layout = self.layout
        for type, name, description in blend_types:
            props = layout.operator(operators.NWMergeNodes.bl_idname, text=name, text_ctxt=i18n_contexts.id_nodetree)
            props.mode = type
            props.merge_type = 'MIX'


class NWConnectionListOutputs(Menu, NWBaseMenu):
    bl_idname = "NODE_MT_nw_connection_list_out"
    bl_label = ""

    def draw(self, context):
        layout = self.layout
        nodes, links = get_nodes_links(context)

        layout.label(text="From Socket", icon='RADIOBUT_OFF')
        layout.separator()

        n1 = nodes[context.scene.NWLazySource]
        for index, output in enumerate(n1.outputs):
            # Only show sockets that are exposed.
            if output.enabled:
                layout.operator(
                    operators.NWCallInputsMenu.bl_idname,
                    text=output.name,
                    text_ctxt=i18n_contexts.default,
                    icon=socket_to_icon(output),
                ).from_socket = index


class NWConnectionListInputs(Menu, NWBaseMenu):
    bl_idname = "NODE_MT_nw_connection_list_in"
    bl_label = ""

    def draw(self, context):
        layout = self.layout
        nodes, links = get_nodes_links(context)

        layout.label(text="To Socket", icon='FORWARD')
        layout.separator()

        n2 = nodes[context.scene.NWLazyTarget]

        for index, input in enumerate(n2.inputs):
            # Only show sockets that are exposed.
            # This prevents, for example, the scale value socket
            # of the vector math node being added to the list when
            # the mode is not 'SCALE'.
            if input.enabled:
                op = layout.operator(
                    operators.NWMakeLink.bl_idname, text=input.name,
                    text_ctxt=i18n_contexts.default,
                    icon=socket_to_icon(input),
                )
                op.from_socket = context.scene.NWSourceSocket
                op.to_socket = index


class NWMergeMathMenu(Menu, NWBaseMenu):
    bl_idname = "NODE_MT_nw_merge_math_menu"
    bl_label = "Merge Selected Nodes using Math"

    def draw(self, context):
        layout = self.layout
        for type, name, description in operations:
            props = layout.operator(operators.NWMergeNodes.bl_idname, text=name, text_ctxt=i18n_contexts.id_nodetree)
            props.mode = type
            props.merge_type = 'MATH'


class NWBatchChangeNodesMenu(Menu, NWBaseMenu):
    bl_idname = "NODE_MT_nw_batch_change_nodes_menu"
    bl_label = "Batch Change Selected Nodes"

    def draw(self, context):
        layout = self.layout
        layout.menu(NWBatchChangeBlendTypeMenu.bl_idname)
        layout.menu(NWBatchChangeOperationMenu.bl_idname)


class NWBatchChangeBlendTypeMenu(Menu, NWBaseMenu):
    bl_idname = "NODE_MT_nw_batch_change_blend_type_menu"
    bl_label = "Batch Change Blend Type"

    def draw(self, context):
        layout = self.layout
        for type, name, description in blend_types:
            props = layout.operator(
                operators.NWBatchChangeNodes.bl_idname,
                text=name,
                text_ctxt=i18n_contexts.id_nodetree,
            )
            props.blend_type = type
            props.operation = 'CURRENT'


class NWBatchChangeOperationMenu(Menu, NWBaseMenu):
    bl_idname = "NODE_MT_nw_batch_change_operation_menu"
    bl_label = "Batch Change Math Operation"

    def draw(self, context):
        layout = self.layout
        for type, name, description in operations:
            props = layout.operator(operators.NWBatchChangeNodes.bl_idname, text=name, text_ctxt=i18n_contexts.id_nodetree)
            props.blend_type = 'CURRENT'
            props.operation = type


class NWCopyToSelectedMenu(Menu, NWBaseMenu):
    bl_idname = "NODE_MT_nw_copy_node_properties_menu"
    bl_label = "Copy to Selected"

    def draw(self, context):
        layout = self.layout
        layout.operator(operators.NWCopySettings.bl_idname, text="Settings from Active")
        layout.menu(NWCopyLabelMenu.bl_idname)


class NWCopyLabelMenu(Menu, NWBaseMenu):
    bl_idname = "NODE_MT_nw_copy_label_menu"
    bl_label = "Copy Label"

    def draw(self, context):
        layout = self.layout
        layout.operator(operators.NWCopyLabel.bl_idname, text="From Active Node's Label").option = 'FROM_ACTIVE'
        layout.operator(operators.NWCopyLabel.bl_idname, text="From Linked Node's Label").option = 'FROM_NODE'
        layout.operator(operators.NWCopyLabel.bl_idname, text="From Linked Output's Name").option = 'FROM_SOCKET'


class NWAddReroutesMenu(Menu, NWBaseMenu):
    bl_idname = "NODE_MT_nw_add_reroutes_menu"
    bl_label = "Add Reroutes"
    bl_description = "Add reroute nodes to selected nodes' outputs"

    def draw(self, context):
        layout = self.layout
        layout.operator(operators.NWAddReroutes.bl_idname, text="To All Outputs").option = 'ALL'
        layout.operator(operators.NWAddReroutes.bl_idname, text="To Loose Outputs").option = 'LOOSE'
        layout.operator(operators.NWAddReroutes.bl_idname, text="To Linked Outputs").option = 'LINKED'


class NWLinkActiveToSelectedMenu(Menu, NWBaseMenu):
    bl_idname = "NODE_MT_nw_link_active_to_selected_menu"
    bl_label = "Link Active to Selected"

    def draw(self, context):
        layout = self.layout
        layout.menu(NWLinkStandardMenu.bl_idname)
        layout.menu(NWLinkUseNodeNameMenu.bl_idname)
        layout.menu(NWLinkUseOutputsNamesMenu.bl_idname)


class NWLinkStandardMenu(Menu, NWBaseMenu):
    bl_idname = "NODE_MT_nw_link_standard_menu"
    bl_label = "To All Selected"

    def draw(self, context):
        layout = self.layout
        props = layout.operator(operators.NWLinkActiveToSelected.bl_idname, text="Do Not Replace Links")
        props.replace = False
        props.use_node_name = False
        props.use_outputs_names = False
        props = layout.operator(operators.NWLinkActiveToSelected.bl_idname, text="Replace Links")
        props.replace = True
        props.use_node_name = False
        props.use_outputs_names = False


class NWLinkUseNodeNameMenu(Menu, NWBaseMenu):
    bl_idname = "NODE_MT_nw_link_use_node_name_menu"
    bl_label = "Use Node Name/Label"

    def draw(self, context):
        layout = self.layout
        props = layout.operator(operators.NWLinkActiveToSelected.bl_idname, text="Do Not Replace Links")
        props.replace = False
        props.use_node_name = True
        props.use_outputs_names = False
        props = layout.operator(operators.NWLinkActiveToSelected.bl_idname, text="Replace Links")
        props.replace = True
        props.use_node_name = True
        props.use_outputs_names = False


class NWLinkUseOutputsNamesMenu(Menu, NWBaseMenu):
    bl_idname = "NODE_MT_nw_link_use_outputs_names_menu"
    bl_label = "Use Output Names"

    def draw(self, context):
        layout = self.layout
        props = layout.operator(operators.NWLinkActiveToSelected.bl_idname, text="Do Not Replace Links")
        props.replace = False
        props.use_node_name = False
        props.use_outputs_names = True
        props = layout.operator(operators.NWLinkActiveToSelected.bl_idname, text="Replace Links")
        props.replace = True
        props.use_node_name = False
        props.use_outputs_names = True


class NWAttributeMenu(bpy.types.Menu):
    bl_idname = "NODE_MT_nw_node_attribute_menu"
    bl_label = "Attributes"

    @classmethod
    def poll(cls, context):
        space = context.space_data
        return (space.type == 'NODE_EDITOR'
                and space.node_tree is not None
                and space.node_tree.library is None
                and space.tree_type == 'ShaderNodeTree'
                and space.shader_type == 'OBJECT')

    def draw(self, context):
        l = self.layout
        nodes, links = get_nodes_links(context)
        mat = context.object.active_material

        objs = []
        for obj in bpy.data.objects:
            for slot in obj.material_slots:
                if slot.material == mat:
                    objs.append(obj)
        attrs = []
        for obj in objs:
            if obj.data.attributes:
                for attr in obj.data.attributes:
                    if not attr.is_internal:
                        attrs.append(attr.name)
        attrs = list(set(attrs))  # get a unique list

        if attrs:
            for attr in attrs:
                l.operator(
                    operators.NWAddAttrNode.bl_idname,
                    text=attr,
                    translate=False,
                ).attr_name = attr
        else:
            l.label(text="No attributes on objects with this material")


#
#  APPENDAGES TO EXISTING UI
#


def select_parent_children_buttons(self, context):
    layout = self.layout
    layout.operator(operators.NWSelectParentChildren.bl_idname, text="Select Frame Children").option = 'CHILD'
    layout.operator(operators.NWSelectParentChildren.bl_idname, text="Select Parent Frame").option = 'PARENT'


def attr_nodes_menu_func(self, context):
    col = self.layout.column(align=True)
    col.menu("NODE_MT_nw_node_attribute_menu")
    col.separator()


def multipleimages_menu_func(self, context):
    col = self.layout.column(align=True)
    col.operator("node.add_image", text="Multiple Images")
    col.operator(operators.NWAddSequence.bl_idname, text="Image Sequence")
    col.separator()


def bgreset_menu_func(self, context):
    self.layout.operator(operators.NWResetBG.bl_idname)


def save_viewer_menu_func(self, context):
    space = context.space_data
    if (space.type == 'NODE_EDITOR'
            and space.tree_type == 'CompositorNodeTree'
            and space.node_tree_sub_type == 'SCENE'
            and space.node_tree is not None
            and space.node_tree.library is None
            and space.edit_tree.nodes.active
            and space.edit_tree.nodes.active.type == "VIEWER"):
        self.layout.operator(operators.NWSaveViewer.bl_idname, icon='FILE_IMAGE')


def reset_nodes_button(self, context):
    node_active = context.active_node
    node_selected = context.selected_nodes

    # Check if active node is in the selection, ignore some node types
    if (len(node_selected) != 1
            or node_active is None
            or not node_active.select
            or node_active.type in {"REROUTE", "GROUP"}):
        return

    row = self.layout.row()

    if node_active.type == "FRAME":
        row.operator(operators.NWResetNodes.bl_idname, text="Reset Nodes in Frame", icon="FILE_REFRESH")
    else:
        row.operator(operators.NWResetNodes.bl_idname, text="Reset Node", icon="FILE_REFRESH")

    self.layout.separator()


classes = (
    NodeWranglerPanel,
    NodeWranglerMenu,
    NWMergeNodesMenu,
    NWMergeGeometryMenu,
    NWMergeShadersMenu,
    NWMergeMixMenu,
    NWConnectionListOutputs,
    NWConnectionListInputs,
    NWMergeMathMenu,
    NWBatchChangeNodesMenu,
    NWBatchChangeBlendTypeMenu,
    NWBatchChangeOperationMenu,
    NWCopyToSelectedMenu,
    NWCopyLabelMenu,
    NWAddReroutesMenu,
    NWLinkActiveToSelectedMenu,
    NWLinkStandardMenu,
    NWLinkUseNodeNameMenu,
    NWLinkUseOutputsNamesMenu,
    NWAttributeMenu,
)


def register():
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)

    # menu items
    bpy.types.NODE_MT_select.append(select_parent_children_buttons)
    bpy.types.NODE_MT_category_shader_input.prepend(attr_nodes_menu_func)
    bpy.types.NODE_PT_backdrop.append(bgreset_menu_func)
    bpy.types.NODE_PT_active_node_generic.append(save_viewer_menu_func)
    bpy.types.NODE_MT_category_shader_texture.prepend(multipleimages_menu_func)
    bpy.types.NODE_MT_category_compositor_input.prepend(multipleimages_menu_func)
    bpy.types.NODE_PT_active_node_generic.prepend(reset_nodes_button)
    bpy.types.NODE_MT_node.prepend(reset_nodes_button)


def unregister():
    # menu items
    bpy.types.NODE_MT_select.remove(select_parent_children_buttons)
    bpy.types.NODE_MT_category_shader_input.remove(attr_nodes_menu_func)
    bpy.types.NODE_PT_backdrop.remove(bgreset_menu_func)
    bpy.types.NODE_PT_active_node_generic.remove(save_viewer_menu_func)
    bpy.types.NODE_MT_category_shader_texture.remove(multipleimages_menu_func)
    bpy.types.NODE_MT_category_compositor_input.remove(multipleimages_menu_func)
    bpy.types.NODE_PT_active_node_generic.remove(reset_nodes_button)
    bpy.types.NODE_MT_node.remove(reset_nodes_button)

    from bpy.utils import unregister_class
    for cls in classes:
        unregister_class(cls)
