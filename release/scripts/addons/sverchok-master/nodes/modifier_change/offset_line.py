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

from math import radians, pi, sin 

import bpy
from bpy.props import FloatProperty
from mathutils import Vector, Matrix
from mathutils.geometry import normal

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat
from sverchok.core.sv_custom_exceptions import SvNotFullyConnected

TWO_PI = 2 * pi

def calc_angle(p1,p2):
    #Convert to from 0 to 2pi angle
    angle = p1.angle_signed(p2)
    return angle + TWO_PI if angle < 0 else angle

def offset_edges(verts_in, edges_in, shift_in):
    # take an input mesh (verts + edges ) and an offset property and generate the resulting geometry

    verts_out = []
    faces_out = []
    
    verts_z = verts_in[:]
    verts_in = [Vector(v).to_2d() for v in verts_in]

    diff_shift = len(verts_in) - len(shift_in)
    if diff_shift >= 0:
        shift_in.extend([shift_in[-1] for _ in range(diff_shift)])
    else:
        shift_in = shift_in[:diff_shift]
    
    #Searching neighbours for each point
    neighbours = [[] for _ in verts_in]
    for edg in edges_in:
        neighbours[edg[0]].append(edg)
        neighbours[edg[1]].append(edg)

    #Sorting neighbours by hour hand
    for i, p_neighbs in enumerate(neighbours):
        if len(p_neighbs) != 1:
            angles = [0]
            first_item = list(set(p_neighbs[0]) - set([i]))[0]
            for another_neighb in p_neighbs[1:]:            
                second_item = list(set(another_neighb) - set([i]))[0]
                vector1 = verts_in[first_item] - verts_in[i]
                vector2 = verts_in[second_item] - verts_in[i]
                angles.append(calc_angle(vector1, vector2))
            sorted_ = list(zip(angles, p_neighbs))
            sorted_.sort()
            neighbours[i] = [e[1] for e in sorted_]
            
    def get_end_points(item_point, shift):
        second_item = list(set(neighbours[item_point][0]) - set([item_point]))[0]
        vert_edg = verts_in[item_point] - verts_in[second_item]
        mat_r1 = Matrix.Rotation(radians(-45),2,'X')
        mat_r2 = Matrix.Rotation(radians(45),2,'X')
        shift_end_points = shift/sin(radians(45))
        vert_new1 = (vert_edg * mat_r1).normalized() * shift_end_points + verts_in[item_point]
        vert_new2 = (vert_edg * mat_r2).normalized() * shift_end_points + verts_in[item_point]
        return [vert_new1,vert_new2]

    def get_middle_points(item_point, shift):
        points = []
        for i in range(len(neighbours[item_point])):
            current_edges = (neighbours[item_point][i:] + neighbours[item_point][:i])[:2]
            second_item1 = list(set(current_edges[0]) - set([item_point]))[0]
            second_item2 = list(set(current_edges[1]) - set([item_point]))[0]
            vert_edg1 = verts_in[second_item1] - verts_in[item_point]
            vert_edg2 = verts_in[second_item2] - verts_in[item_point]
            angle = calc_angle(vert_edg1, vert_edg2) / 2
            mat_r = Matrix.Rotation(angle, 2, 'X')        
            points.append((vert_edg1 * mat_r).normalized() * shift/sin(angle) + verts_in[item_point])
        return points

    #Seting points
    findex_new_points = [0]
    for i,sh in enumerate(shift_in):
        #avoid zero offset
        if not sh:
            sh = 0.001
        
        if len(neighbours[i]) == 1:
            verts_out.extend(get_end_points(i,sh))
            findex_new_points.append(findex_new_points[-1] + 2)
        else:
            p = get_middle_points(i,sh)
            verts_out.extend(p)
            findex_new_points.append(findex_new_points[-1] + len(p))
        
    # Preparing Z coordinate
    z_co = []
    for c,(i1,i2) in enumerate(zip(findex_new_points[:-1], findex_new_points[1:])):
        z_co.extend([verts_z[c][2] for _ in range(i2-i1)])
        
    #Creating faces and mark outer edges and central points
    outer_edges = []
    current_index = len(verts_out) - 1
    vers_mask = [0 for _ in verts_out]
    nomber_outer_points = current_index
    position_old_points = [0 for _ in verts_out]
    for edg in edges_in:
        need_points = []
        for i in edg:
            if len(neighbours[i]) <= 2:
                need_points.extend([findex_new_points[i], findex_new_points[i] + 1])
            else:
                position = neighbours[i].index(edg)
                nomber_points = len(neighbours[i])
                variants_positions = list(range(findex_new_points[i], findex_new_points[i] + nomber_points))
                need_points.extend((variants_positions[position - 1:] + variants_positions[:position - 1])[:2])
    
        vec_edg = verts_in[edg[0]] - verts_in[edg[1]]
        vec_1 = verts_out[need_points[0]] - verts_in[edg[0]]
        vec_2 = verts_out[need_points[1]] - verts_in[edg[0]]
        vec_3 = verts_out[need_points[2]] - verts_in[edg[1]]
        vec_4 = verts_out[need_points[3]] - verts_in[edg[1]]
        new_vecs = [vec_1, vec_2, vec_3, vec_4]

        angles = [vec_edg.angle_signed(vec) for vec in new_vecs]

        if position_old_points[edg[0]] == 0:
            verts_out.append(verts_in[edg[0]])
            z_co.append(verts_z[edg[0]][2]) 
            vers_mask.append(1)
            current_index += 1
            position_old_points[edg[0]] = current_index
    
        if position_old_points[edg[1]] == 0:
            verts_out.append(verts_in[edg[1]])
            z_co.append(verts_z[edg[1]][2])
            vers_mask.append(1)
            current_index += 1
            position_old_points[edg[1]] = current_index
            
        n_p = need_points
        if angles[0] < 0 and angles[2] < 0 or angles[0] >= 0 and angles[2] >= 0:
            new_edges = [[n_p[1], position_old_points[edg[0]], position_old_points[edg[1]], n_p[3]],
                         [n_p[0], position_old_points[edg[0]], position_old_points[edg[1]], n_p[2]]]
                             
            outer_edges.extend([[n_p[1],n_p[3]],[n_p[0],n_p[2]]])
        else:
            new_edges = [[n_p[0], position_old_points[edg[0]], position_old_points[edg[1]], n_p[3]],
                         [n_p[1], position_old_points[edg[0]], position_old_points[edg[1]], n_p[2]]]
                         
            outer_edges.extend([[n_p[0],n_p[3]],[n_p[1],n_p[2]]])

        if len(neighbours[edg[0]]) == 1:
            new_edges.append([need_points[1], position_old_points[edg[0]], need_points[0]])
            outer_edges.append([need_points[1],need_points[0]])
        if len(neighbours[edg[1]]) == 1:
            new_edges.append([need_points[2], position_old_points[edg[1]], need_points[3]])
            outer_edges.append([need_points[2],need_points[3]])
    
        for c,face in enumerate(new_edges):
            if normal(*[verts_out[i].to_3d() for i in face])[2] < 0:
                new_edges[c] = new_edges[c][::-1]
        
        faces_out.extend(new_edges)

    verts_out = [(v.x, v.y, z) for v, z in zip(verts_out, z_co)]
    return verts_out, faces_out, outer_edges, vers_mask

class SvOffsetLineNode(bpy.types.Node, SverchCustomTreeNode):
    """
    Triggers: Offset Line 2D
    Tooltip: Offsetting a Line into 2D space

    Only X and Y dimensions of input points will be taken for work.
    """
    bl_idname = 'SvOffsetLineNode'
    bl_label = 'Offset line'
    bl_icon = 'OUTLINER_OB_EMPTY'

    offset = FloatProperty(
        name='offset', description='distance of offset',
        default=0.1, update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'Vers')
        self.inputs.new('StringsSocket', "Edgs")
        self.inputs.new('StringsSocket', "Offset").prop_name = 'offset'
        self.outputs.new('VerticesSocket', 'Vers')
        self.outputs.new('StringsSocket', "Faces")
        self.outputs.new('StringsSocket', "OuterEdges")
        self.outputs.new('StringsSocket', "VersMask")

    def process(self):

        if not all(socket.is_linked for socket in self.inputs[:2]):
            raise SvNotFullyConnected(self, sockets=["Vers", "Edgs"])
        
        if not any(socket.is_linked for socket in self.outputs):
            return

        verts_in = self.inputs['Vers'].sv_get()
        edges_in = self.inputs['Edgs'].sv_get()
        shifter = self.inputs['Offset'].sv_get()
        
        # verts_out, faces_out, outer_edges, vers_mask
        out_geometry = [[], [], [], []]
        
        shifter = match_long_repeat([verts_in, shifter])[1]
        
        for verts,edges,shift in zip(verts_in, edges_in, shifter):
            geometry = offset_edges(verts, edges, shift)
            _ = [out_geometry[idx].append(data) for idx, data in enumerate(geometry)]
        
        # nothing done
        if not len(out_geometry[0]):
            return

        self.outputs['Vers'].sv_set(out_geometry[0])
        self.outputs['Faces'].sv_set(out_geometry[1])
        self.outputs['OuterEdges'].sv_set(out_geometry[2])
        self.outputs['VersMask'].sv_set(out_geometry[3])

def register():
    bpy.utils.register_class(SvOffsetLineNode)


def unregister():
    bpy.utils.unregister_class(SvOffsetLineNode)
