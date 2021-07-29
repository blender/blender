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

def collect_links(node):
    in_links = []
    out_links = []
    for s in node.inputs:
        for link in s.links:
            in_links.append(link)
    for s in node.outputs:
        for link in s.links:
            out_links.append(link)
    return in_links, out_links

def upgrade_group(monad):
    old_node_idname = 'SvGroupNode'
    old_node_input_idname = 'SvGroupInputsNode'
    old_node_output_idname = 'SvGroupOutputsNode'
    new_node_input_idname = 'SvGroupInputsNodeExp'
    new_node_output_idname = 'SvGroupOutputsNodeExp'

    input_node = [node for node in monad.nodes if node.bl_idname == old_node_input_idname]
    output_node = [node for node in monad.nodes if node.bl_idname == old_node_output_idname]

    if len(input_node) == 1 and len(output_node) == 1:
        input_node = input_node[0]
        output_node = output_node[0]
    else:
        print("Failed to upgrade old node group {}".format(monad.name))
        return

    new_input_node = monad.nodes.new(new_node_input_idname)
    new_output_node = monad.nodes.new(new_node_output_idname)
    new_input_node.location = input_node.location
    new_output_node.location = output_node.location

    for s in input_node.outputs:
        for link in s.links:
            monad.links.new(new_input_node.outputs[-1], link.to_socket)

    for s in output_node.inputs:
        for link in s.links:
            monad.links.new(link.from_socket, new_output_node.inputs[-1])

    monad.nodes.remove(input_node)
    monad.nodes.remove(output_node)
    cls_ref = monad.update_cls()

    nodes_to_upgrade = []
    for ng in monad.sv_trees:
        for node in ng.nodes:
            if node.bl_idname == old_node_idname and node.group_name == monad.name:
                nodes_to_upgrade.append(node)

    for node in nodes_to_upgrade:
        ng = node.id_data
        in_links, out_links = collect_links(node)
        monad_instance = ng.nodes.new(cls_ref.bl_idname)
        monad_instance.location = node.location
        for link in in_links:
            to_socket = monad_instance.inputs[link.to_socket.index]
            ng.links.new(link.from_socket, to_socket)
        for link in out_links:
            from_socket = monad_instance.outputs[link.from_socket.index]
            ng.links.new(from_socket, link.to_socket)

        ng.nodes.remove(node)
        
    print("\nUpgraded node group: {} with # {} groups nodes".format(monad.name, len(nodes_to_upgrade)))
