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
# dict for nodes be upgraded to
# compact node layout. Format
# bl_idname : [[socket_name0, prop_name0],
#              [socket_name1, prop_name1]],

from sverchok import old_nodes

upgrade_dict = {
    'SphereNode':
        [['Radius', 'rad_'],
         ['U', 'U_'],
         ['V', 'V_']],
    'CylinderNode':
        [['RadTop', 'radTop_'],
         ['RadBot', 'radBot_'],
         ['Vertices', 'vert_'],
         ['Height', 'height_'],
         ['Subdivisions', 'subd_']],
    'RandomNode':
        [['Count', 'count_inner'],
         ['Seed', 'seed']],
    'PlaneNode':
        [["Nº Vertices X", 'int_X'],
         ["Nº Vertices Y", 'int_Y'],
         ["Step X", "step_X"],
         ["Step Y", "step_Y"]],
    'ListSliceNode':
        [['Start', 'start'],
         ['Stop', 'stop']],
    'LineNode':
        [["Nº Vertices", 'int_'],
         ["Step", 'step_']],
    'FloatNode':
        [['Float', 'float_']],
    'IntegerNode':
        [['Integer', 'int_']],
    'HilbertNode':
        [["Level", 'level_'],
         ["Size", 'size_']],
    'HilbertImageNode':
        [["Level", 'level_'],
         ["Size", 'size_'],
         ["Sensitivity", 'sensitivity_']],
    'VectorMoveNode':
        [["multiplier", 'mult_']],
    'EvaluateLine':
        [["Factor", 'factor_']],
    'SvBoxNode':
        [["Size", 'Size'],
         ["Divx", 'Divx'],
         ["Divy", 'Divy'],
         ["Divz", 'Divz']],
    'ImageNode':
        [["vecs X", 'Xvecs'],
         ["vecs Y", 'Yvecs'],
         ["Step X", 'Xstep'],
         ["Step Y", 'Ystep']],
    'GenVectorsNode':
        [['X', 'x_'],
         ['Y', 'y_'],
         ['Z', 'z_']],
    'MatrixInterpolationNode':
        [['Factor', 'factor_']],
    'MatrixShearNode':
        [['Factor1', 'factor1_'],
         ['Factor2', 'factor2_']],
    'RandomVectorNode':
        [["Count", 'count_inner'],
         ["Seed", "seed"]],
    'ListRepeaterNode':
        [["Number", "number"]],
    'ListItem2Node':
        [["Item", "item"]],
    'SvWireframeNode':
        [['thickness', 'thickness'],
         ['Offset', 'offset']],
    'SvSolidifyNode':
        [['thickness', 'thickness']],
    'SvRemoveDoublesNode':
        [['Distance', 'distance']],
    'ScalarMathNode':
        [['X', 'x'],
         ['Y', 'y']]
    }


#  for new style vertices socket ~ 0.5
#  bl_idname:
#  socket_name, prop_name, boolean use_prop , default 3 tuple
#  two distinct scenarios
#  use prop_name of node
#  or use_prop and set default

vertices_socket_upgrade = {
    'MatrixGenNode' :
        [['Location', '', True, (0,0,0)],
         ['Scale', '', True, (1,1,1)],
         ['Rotation', '', True, (0,0,1)]],
}

# new sockets
# format
# bl_idname : [[new_socket0],[newsocket1]],
# new_socket [inputs/outputs,type,name,position]

new_socket_dict = {
    'SvWireframeNode':
        [['inputs', 'StringsSocket', 'thickness', 0],
         ['inputs', 'StringsSocket', 'Offset', 1]],
    'SvSolidifyNode':
        [['inputs', 'StringsSocket', 'thickness', 0]],
    'SvRemoveDoublesNode':
        [['inputs', 'StringsSocket', 'Distance', 0]],
    'MaskListNode':
        [['outputs', 'StringsSocket', 'mask', 0],
         ['outputs', 'StringsSocket', 'ind_true', 1],
         ['outputs', 'StringsSocket', 'ind_false', 2]],
    'ListFLNode':
        [['outputs', 'StringsSocket', 'Middl', 0]],
    'CentersPolsNode':
        [['outputs', 'VerticesSocket', "Norm_abs", 1],
         ['outputs', 'VerticesSocket', "Origins", 2]],
    'SvSolidifyNode':
        [['outputs', 'StringsSocket', 'newpols', 3]],
    'IndexViewerNode':
        [['inputs', 'StringsSocket', 'text', 4]],
    'SvIterateNode':
        [['outputs', 'MatrixSocket', 'Matrices', 3]],
    'SvBmeshViewerNodeMK2':
        [['outputs', 'SvObjectSocket', 'Objects', 0]],
    }


def upgrade_nodes(ng):
    ''' Apply prop_name for nodes in the node group ng for
        upgrade to compact ui and create nodes that we add to
        '''
    old_nodes.load_old(ng)

    for node in [n for n in ng.nodes if n.bl_idname in new_socket_dict]:
        print(node.name)

        for in_out, s_type, name, pos in new_socket_dict[node.bl_idname]:
            s_list = getattr(node, in_out)
            if name not in s_list:
                s_list.new(s_type, name)
                s_list.move(len(s_list)-1, pos)

    for node in [node for node in ng.nodes if node.bl_idname in upgrade_dict]:
        for s_name, p_name in upgrade_dict[node.bl_idname]:
            socket = node.inputs.get(s_name)
            if socket and not socket.prop_name:
                socket.prop_name = p_name

    for n in [n for n in ng.nodes if n.bl_idname in vertices_socket_upgrade]:
        for s_name, p_name, use_prop, default in vertices_socket_upgrade[n.bl_idname]:
            socket = n.inputs.get(s_name)
            if socket:
                if p_name and not socket.prop_name:
                    socket.prop_name = p_name
                elif use_prop and not socket.use_prop:
                    socket.prop = default
                    socket.use_prop = True
                    socket.prop_name = ""
                else:
                    pass
