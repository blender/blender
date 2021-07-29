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


import bpy

from sverchok.data_structure import match_long_repeat


def get_valid_node(mat_name, node_name, bl_idname):

    materials = bpy.data.materials

    # make sure the Material is present
    m = materials.get(mat_name)
    if not m:
        m = materials.new(mat_name)

    # if it doesn't use nodes force that (and cycles ?! )
    m.use_nodes = True
    m.use_fake_user = True

    # make sure the CurveNode we want to use is present too
    node = m.node_tree.nodes.get(node_name)
    if not node:
        node = m.node_tree.nodes.new(bl_idname)
        node.name = node_name

    return node


def get_valid_evaluate_function(mat_name, node_name):
    ''' 
    Takes a material name (cycles) and a Node name it expects to find.
    The node will be of type ShaderNodeRGBCurve and this function
    will force its existence, then return the evaluate function for the last
    component of RGBA - allowing us to use this as a float modifier.
    '''

    node = get_valid_node(mat_name, node_name, 'ShaderNodeRGBCurve')

    curve = node.mapping.curves[3]
    try: curve.evaluate(0.0)
    except: node.mapping.initialize()

    return curve.evaluate


def vectorize(all_data):

    def listify(data):
        if isinstance(data, (int, float)):
            data = [data]
        return data

    for idx, d in enumerate(all_data):
        all_data[idx] = listify(d)

    return match_long_repeat(all_data)


def ddir(content, filter_str=None):
    vals = []
    if not filter_str:
        vals = [n for n in dir(content) if not n.startswith('__')]
    else:
        vals = [n for n in dir(content) if not n.startswith('__') and filter_str in n]
    return vals
