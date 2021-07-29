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
import bmesh
from sverchok.node_tree import SverchCustomTreeNode
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata
from sverchok.data_structure import dataCorrect, Vector_generate


class SvVolumeNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Volume '''
    bl_idname = 'SvVolumeNode'
    bl_label = 'Volume'
    bl_icon = 'SNAP_VOLUME'

    def draw_buttons(self, context, layout):
        pass

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'Vers')
        self.inputs.new('StringsSocket', "Pols")
        self.outputs.new('StringsSocket', "Volume")

    def process(self):
        verts_socket, polys_socket = self.inputs
        vol_socket = self.outputs[0]

        if vol_socket.is_linked and verts_socket.is_linked:  # and polys_socket.is_linked ?

            vertices = Vector_generate(dataCorrect(verts_socket.sv_get()))
            faces = dataCorrect(polys_socket.sv_get())

            out = []
            for verts_obj, faces_obj in zip(vertices, faces):
                bm = bmesh_from_pydata(verts_obj, [], faces_obj, normal_update=True)
                out.append(bm.calc_volume())
                bm.free()
 
            vol_socket.sv_set(out)

    '''
    solution, that blow my mind, not delete.
    i have to investigate it here
    
    def Volume(self, bme):
        verts = obj_data.vertices     # array of vertices
        obj_data.calc_tessface()
        faces = obj_data.tessfaces        # array of faces
        VOLUME = 0;     # VOLUME OF THE OBJECT
        
        for f in faces:
            fverts = f.vertices      # getting face's vertices
            ab = verts[fverts[0]].co 
            ac = verts[fverts[1]].co
            ad = verts[fverts[2]].co
            
            # calculating determinator
            det = (ab[0]*ac[1]*ad[2]) - (ab[0]*ac[2]*ad[1]) - \
                (ab[1]*ac[0]*ad[2]) + (ab[1]*ac[2]*ad[0]) + \
                (ab[2]*ac[0]*ad[1]) - (ab[2]*ac[1]*ad[0])
            
            VOLUME += det/6
    '''

def register():
    bpy.utils.register_class(SvVolumeNode)


def unregister():
    bpy.utils.unregister_class(SvVolumeNode)
