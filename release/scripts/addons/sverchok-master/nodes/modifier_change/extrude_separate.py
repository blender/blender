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

from math import pi

import bpy
import bmesh.ops
from bpy.props import IntProperty, FloatProperty
from mathutils import Matrix, Vector

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat, fullList
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata, pydata_from_bmesh

vsock, toposock = 'VerticesSocket', 'StringsSocket'


class SvExtrudeSeparateNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Inset like behaviour '''
    bl_idname = 'SvExtrudeSeparateNode'
    bl_label = 'Extrude Separate Faces'
    bl_icon = 'OUTLINER_OB_EMPTY'

    height_ = FloatProperty(
        name="Height", description="Extrusion amount",
        default=0.0, update=updateNode)

    scale_ = FloatProperty(
        name="Scale", description="Extruded faces scale",
        default=1.0, min=0.0, update=updateNode)

    replacement_nodes = [
            ('SvInsetSpecial',
                dict(Vertices='vertices', Polygons='polygons'),
                dict(Vertices='vertices', Polygons='polygons'))]

    def sv_init(self, context):
        inew = self.inputs.new
        onew = self.outputs.new
        
        inew(vsock, "Vertices")
        inew(toposock, 'Edges')
        inew(toposock, 'Polygons')
        inew(toposock, 'Mask')
        inew(toposock, "Height").prop_name = "height_"
        inew(toposock, "Scale").prop_name = "scale_"
        
        onew(vsock, 'Vertices')
        onew(toposock, 'Edges')
        onew(toposock, 'Polygons')
        onew(toposock, 'ExtrudedPolys')
        onew(toposock, 'OtherPolys')

    @property
    def scale_socket_type(self):
        socket = self.inputs['Scale']
        if socket.is_linked:
            other = socket.other
            if other.bl_idname == 'VerticesSocket':
                print('connected a Vector Socket')
                return True
        return False
  
    def process(self):

        inputs = self.inputs
        outputs = self.outputs

        if not (inputs['Vertices'].is_linked and inputs['Polygons'].is_linked):
            return
        if not any(socket.is_linked for socket in outputs):
            return

        vector_in = self.scale_socket_type

        vertices_s = inputs['Vertices'].sv_get()
        edges_s = inputs['Edges'].sv_get(default=[[]])
        faces_s = inputs['Polygons'].sv_get(default=[[]])
        masks_s = inputs['Mask'].sv_get(default=[[1]])
        heights_s = inputs['Height'].sv_get()
        scales_s  = inputs['Scale'].sv_get()

        linked_extruded_polygons = outputs['ExtrudedPolys'].is_linked
        linked_other_polygons = outputs['OtherPolys'].is_linked

        result_vertices = []
        result_edges = []
        result_faces = []
        result_extruded_faces = []
        result_other_faces = []

        meshes = match_long_repeat([vertices_s, edges_s, faces_s, masks_s, heights_s, scales_s])

        for vertices, edges, faces, masks, heights, scales in zip(*meshes):

            new_extruded_faces = []
            new_extruded_faces_append = new_extruded_faces.append
            fullList(heights, len(faces))
            fullList(scales, len(faces))
            fullList(masks, len(faces))

            bm = bmesh_from_pydata(vertices, edges, faces)
            extruded_faces = bmesh.ops.extrude_discrete_faces(bm, faces=bm.faces)['faces']

            for face, mask, height, scale in zip(extruded_faces, masks, heights, scales):

                if not mask:
                    continue

                vec = scale if vector_in else (scale, scale, scale)

                # preparing matrix
                normal = face.normal    
                if normal[0] == 0 and normal[1] == 0:
                    m_r = Matrix() if normal[2] >= 0 else Matrix.Rotation(pi, 4, 'X')
                else:    
                    z_axis = normal
                    x_axis = (Vector((z_axis[1] * -1, z_axis[0], 0))).normalized()
                    y_axis = (z_axis.cross(x_axis)).normalized()
                    m_r = Matrix(list([*zip(x_axis[:], y_axis[:], z_axis[:])])).to_4x4()

                dr = face.normal * height
                center = face.calc_center_median()
                translation = Matrix.Translation(center)
                m = (translation * m_r).inverted()
                
                # inset, scale and push operations
                bmesh.ops.scale(bm, vec=vec, space=m, verts=face.verts)
                bmesh.ops.translate(bm, verts=face.verts, vec=dr)

                if linked_extruded_polygons or linked_other_polygons:
                    new_extruded_faces_append([v.index for v in face.verts])

            new_vertices, new_edges, new_faces = pydata_from_bmesh(bm)
            bm.free()

            new_other_faces = [f for f in new_faces if f not in new_extruded_faces] if linked_other_polygons else []

            result_vertices.append(new_vertices)
            result_edges.append(new_edges)
            result_faces.append(new_faces)
            result_extruded_faces.append(new_extruded_faces)
            result_other_faces.append(new_other_faces)

        outputs['Vertices'].sv_set(result_vertices)
        outputs['Edges'].sv_set(result_edges)
        outputs['Polygons'].sv_set(result_faces)
        outputs['ExtrudedPolys'].sv_set(result_extruded_faces)
        outputs['OtherPolys'].sv_set(result_other_faces)


def register():
    bpy.utils.register_class(SvExtrudeSeparateNode)


def unregister():
    bpy.utils.unregister_class(SvExtrudeSeparateNode)
