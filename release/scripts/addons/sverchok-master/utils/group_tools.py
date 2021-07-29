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

"""
old file, see monad file instead
can be removed, kept for a while 2016-08-17
"""

import json
import os
import re
import zipfile
import traceback

from itertools import chain

import bpy
from bpy.types import EnumProperty
from bpy.props import StringProperty
from bpy.props import BoolProperty


from .sv_IO_panel_tools import create_dict_of_tree, import_tree
from sverchok.data_structure import get_other_socket


class SvNodeGroupCreator(bpy.types.Operator):

    '''Create a Node group from selected'''

    bl_idname = "node.sv_group_creator"
    bl_label = "Create node group from selected"

    def execute(self, context):

        ng = context.space_data.node_tree
        ng.freeze(hard=True)
        is_process = ng.sv_process
        ng.sv_process = False
        # collect data
        nodes = {n for n in ng.nodes if n.select}
        if not nodes:
            self.report({"CANCELLED"}, "No nodes selected")
            return {'CANCELLED'}
        # collect links outside of of selected nodes
        test_in = lambda l: bool(l.to_node in nodes) and bool(l.from_node not in nodes)
        test_out = lambda l: bool(l.from_node in nodes) and bool(l.to_node not in nodes)
        out_links = [l for l in ng.links if test_out(l)]
        in_links = [l for l in ng.links if test_in(l)]
        locx = [n.location.x for n in nodes]
        locy = sum(n.location.y for n in nodes)/len(nodes)

        # crete node_group nodes

        group_in = ng.nodes.new("SvGroupInputsNode")
        group_in.location = (min(locx)-300, locy)
        group_out = ng.nodes.new("SvGroupOutputsNode")
        group_out.location = (max(locx)+300, locy)
        group_node = ng.nodes.new("SvGroupNode")
        group_node.location = (sum(locx)/len(nodes), locy)

        # create node group links and replace with a node group instead
        for i,l in enumerate(in_links):
            out_socket = l.from_socket
            in_socket = l.to_socket
            s_name = "{}:{}".format(i,in_socket.name)
            other = get_other_socket(in_socket)
            gn_socket = group_node.inputs.new(other.bl_idname, s_name )
            gi_socket = group_in.outputs.new(other.bl_idname, s_name)

            ng.links.remove(l)
            ng.links.new(in_socket, gi_socket)
            ng.links.new(gn_socket, out_socket)

        out_links_sockets = set(l.from_socket for l in out_links)

        for i, from_socket in enumerate(out_links_sockets):
            to_sockets = [l.to_socket for l in from_socket.links]
            s_name = "{}:{}".format(i, from_socket.name)
            # to account for reroutes
            other = get_other_socket(to_sockets[0])
            gn_socket = group_node.outputs.new(other.bl_idname, s_name)
            go_socket = group_out.inputs.new(other.bl_idname, s_name)
            for to_socket in to_sockets:
                l = to_socket.links[0]
                ng.links.remove(l)
                ng.links.new(go_socket, from_socket)
                ng.links.new(to_socket, gn_socket)

        # collect sockets for node group in out
        group_in.collect()
        group_out.collect()
        # deselect all
        for n in ng.nodes:
            n.select = False
        nodes.add(group_in)
        nodes.add(group_out)

        # select nodes to move
        for n in nodes:
            n.select = True

        nodes_json = create_dict_of_tree(ng, {}, selected=True)
        print(nodes_json)

        for n in nodes:
            ng.nodes.remove(n)

        group_ng = bpy.data.node_groups.new("SvGroup", 'SverchGroupTreeType')

        group_node.group_name = group_ng.name
        group_node.select = True
        group_ng.use_fake_user = True
        import_tree(group_ng, "", nodes_json)

        ng.unfreeze(hard=True)
        ng.sv_process = is_process
        ng.update()
        self.report({"INFO"}, "Node group created")
        return {'FINISHED'}

class SvNodeGroupEdit(bpy.types.Operator):
    bl_idname = "node.sv_node_group_edit"
    bl_label = "Edit group"

    group_name = StringProperty()

    def execute(self, context):
        ng = context.space_data.node_tree
        node = context.node
        group_ng = bpy.data.node_groups.get(self.group_name)
        print(group_ng.name)
        ng.freeze()
        frame = ng.nodes.new("NodeFrame")
        frame.label = group_ng.name
        for n in ng.nodes:
            n.select = False
        nodes_json = create_dict_of_tree(group_ng)
        print(nodes_json)
        import_tree(ng, "", nodes_json, create_texts=False)
        nodes = [n for n in ng.nodes if n.select]
        locs = [n.location for n in nodes]
        for n in nodes:
            if not n.parent:
                n.parent = frame
        ng[frame.name] = self.group_name
        ng["Group Node"] = node.name
        return {'FINISHED'}

class SvNodeGroupEditDone(bpy.types.Operator):
    bl_idname = "node.sv_node_group_done"
    bl_label = "Save group"

    frame_name = StringProperty()

    def execute(self, context):
        print("Saving node group")
        ng = context.space_data.node_tree
        frame = ng.nodes.get(self.frame_name)
        if not frame:
            return {'CANCELLED'}
        nodes = [n for n in ng.nodes if n.parent == frame]

        g_node = ng.nodes[ng["Group Node"]]

        for n in ng.nodes:
            n.select = False
        for n in nodes:
            n.select = True
        in_out = [n for n in nodes if n.bl_idname in {'SvGroupInputsNode', 'SvGroupOutputsNode'}]

        in_out.sort(key=lambda n:n.bl_idname)
        for n in in_out:
            n.collect()
        g_node.adjust_sockets(in_out)

        frame.select = True
        group_ng = bpy.data.node_groups[ng[frame.name]]
        del ng[frame.name]
        group_ng.name = frame.label

        ng.freeze(hard=True)
        ng.nodes.remove(frame)
        nodes_json = create_dict_of_tree(ng, {}, selected=True)
        for n in nodes:
            ng.nodes.remove(n)
        g_node.group_name = group_ng.name
        ng.unfreeze(hard=True)
        group_ng.nodes.clear()
        import_tree(group_ng, "", nodes_json)

        self.report({"INFO"}, "Node group save")
        return {'FINISHED'}


class SverchokGroupLayoutsMenu(bpy.types.Panel):
    bl_idname = "Sverchok_groups_menu"
    bl_label = "SV Groups Beta"
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_category = 'Sverchok'
    bl_options = {'DEFAULT_CLOSED'}
    use_pin = True

    @classmethod
    def poll(cls, context):
        try:
            return context.space_data.node_tree.bl_idname == 'SverchCustomTreeType'
        except:
            return False

    def draw(self, context):
        layout = self.layout
        layout.operator("node.sv_group_creator")

        #for ng in bpy.data.node_groups:
        #    if ng.bl_idname == 'SverchGroupTreeType':
        #        layout.label(ng.name)
        #        op = layout.operator("node.sv_node_group_edit", text="Edit")
        #        op.group_name = ng.name

classes = [
    SverchokGroupLayoutsMenu,
    SvNodeGroupCreator,
    SvNodeGroupEdit,
    SvNodeGroupEditDone
]
"""
def register():
    for class_name in classes:
        bpy.utils.register_class(class_name)

def unregister():
    for class_name in reversed(classes):
        bpy.utils.unregister_class(class_name)
"""
