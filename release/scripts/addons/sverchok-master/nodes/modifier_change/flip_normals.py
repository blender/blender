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
# from mathutils import Vector
from bpy.props import EnumProperty, BoolProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, fullList
from sverchok.data_structure import match_long_repeat as mlrepeat
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata



def flip_from_mask(mask, geom, reverse):
    """
    this mode expects a mask list with an element corresponding to each polygon
    """
    verts, edges, faces = geom
    fullList(mask, len(faces))
    b_faces = []
    for m, face in zip(mask, faces):
        mask_val = bool(m) if not reverse else not bool(m)
        b_faces.append(face if mask_val else face[::-1])

    return verts, edges, b_faces


def flip_to_match_1st(geom, reverse):
    """
    this mode expects all faces to be coplanar, else you need to manually generate a flip mask.
    """
    verts, edges, faces = geom
    b_faces = []
    bm = bmesh_from_pydata(verts, faces=faces, normal_update=True)
    bm.faces.ensure_lookup_table()
    Direction = bm.faces[0].normal
    for face in bm.faces:
        close = (face.normal - Direction).length < 0.004
        flip = close if not reverse else not close
        poly = [i.index for i in face.verts]
        b_faces.append(poly if flip else poly[::-1])

    bm.free()
    return verts, edges, b_faces



class SvFlipNormalsNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Flip face normals '''
    bl_idname = 'SvFlipNormalsNode'
    bl_label = 'Flip normals'
    bl_icon = 'OUTLINER_OB_EMPTY'

    mode_options = [(mode, mode, '', idx) for idx, mode in enumerate(['mask', 'match'])]
        
    selected_mode = EnumProperty(
        items=mode_options, description="offers flip options", default="match", update=updateNode
    )

    reverse = BoolProperty(update=updateNode)


    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices")
        self.inputs.new('StringsSocket', 'Edges')
        self.inputs.new('StringsSocket', 'Polygons')
        self.inputs.new('StringsSocket', 'Mask')

        self.outputs.new('VerticesSocket', 'Vertices')
        self.outputs.new('StringsSocket', 'Edges')
        self.outputs.new('StringsSocket', 'Polygons')

    def draw_buttons(self, context, layout):
        r = layout.row(align=True)
        r1 = r.split(0.35)        
        r1.prop(self, 'reverse', text='reverse', toggle=True)
        r2 = r1.split().row()
        r2.prop(self, "selected_mode", expand=True)

    def process(self):

        if not any(self.outputs[idx].is_linked for idx in range(3)):
            return

        vertices_s = self.inputs['Vertices'].sv_get(default=[[]])
        edges_s = self.inputs['Edges'].sv_get(default=[[]])
        faces_s = self.inputs['Polygons'].sv_get(default=[[]])

        # if vertices_s is [[]] and faces_s is [[]]:
        #    return

        geom = [[], [], []]

        if self.selected_mode == 'mask':
            mask_s = self.inputs['Mask'].sv_get(default=[[True]])
            for *single_geom, mask in zip(*mlrepeat([vertices_s, edges_s, faces_s, mask_s])):
                for idx, d in enumerate(flip_from_mask(mask, single_geom, self.reverse)):
                    geom[idx].append(d)

        elif self.selected_mode == 'match':
            for single_geom in zip(*mlrepeat([vertices_s, edges_s, faces_s])):
                for idx, d in enumerate(flip_to_match_1st(single_geom, self.reverse)):
                    geom[idx].append(d)

        self.set_output(geom)


    def set_output(self, geom):
        _ = [self.outputs[idx].sv_set(data) for idx, data in enumerate(geom)]


def register():
    bpy.utils.register_class(SvFlipNormalsNode)


def unregister():
    bpy.utils.unregister_class(SvFlipNormalsNode)
