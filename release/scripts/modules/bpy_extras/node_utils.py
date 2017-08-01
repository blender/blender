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

# <pep8-80 compliant>

__all__ = (
    "find_node_input",
    "find_output_node",
    )


# XXX Names are not unique. Returns the first match.
def find_node_input(node, name):
    for input in node.inputs:
        if input.name == name:
            return input

    return None

# Return the output node to display in the UI. In case multiple node types are
# specified, node types earlier in the list get priority.
def find_output_node(ntree, nodetypes):
    if ntree:
        output_node = None
        for nodetype in nodetypes:
            for node in ntree.nodes:
                if getattr(node, "type", None) == nodetype:
                    if getattr(node, "is_active_output", True):
                        return node
                    if not output_node:
                        output_node = node
            if output_node:
                return output_node

    return None
