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
from bpy.props import IntProperty, FloatProperty, BoolProperty, EnumProperty
import bmesh.ops

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat, fullList
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata, pydata_from_bmesh

class SvSmoothNode(bpy.types.Node, SverchCustomTreeNode):
    '''Smooth vertices'''
    bl_idname = 'SvSmoothNode'
    bl_label = 'Smooth Vertices'
    bl_icon = 'SMOOTHCURVE'

    def update_mode(self, context):
        self.inputs['ClipDist'].hide_safe = self.laplacian
        self.inputs['BorderFactor'].hide_safe = not self.laplacian
        updateNode(self, context)

    laplacian = BoolProperty(name = "Laplacian Smooth",
            description = "Use Laplacian smoothing",
            default = False,
            update=update_mode)

    iterations = IntProperty(name = "Iterations",
            min=1, max=1000, default=1,
            update=updateNode)

    factor = FloatProperty(name = "Factor",
            description = "Smoothing factor",
            min=0.0, default=0.5,
            update=updateNode)

    border_factor = FloatProperty(name = "Border factor",
            description = "Smoothing factor in border",
            min=0.0, default=0.5,
            update=updateNode)

    mirror_clip_x = BoolProperty(name = "Clip X",
            description = "set vertices close to the X axis before the operation to 0",
            default = False,
            update=updateNode)
    mirror_clip_y = BoolProperty(name = "Clip Y",
            description = "set vertices close to the Y axis before the operation to 0",
            default = False,
            update=updateNode)
    mirror_clip_z = BoolProperty(name = "Clip Z",
            description = "set vertices close to the Z axis before the operation to 0",
            default = False,
            update=updateNode)

    clip_dist = FloatProperty(name = "Clip threshold",
            description = "Clipping threshold",
            min=0.0, default=0.0001,
            update=updateNode)

    use_x = BoolProperty(name = "X",
            description = "smooth vertices along X axis",
            default = True,
            update=updateNode)
    use_y = BoolProperty(name = "Y",
            description = "smooth vertices along Y axis",
            default = True,
            update=updateNode)
    use_z = BoolProperty(name = "Z",
            description = "smooth vertices along Z axis",
            default = True,
            update=updateNode)

    preserve_volume = BoolProperty(name="Preserve volume",
            description = "Apply volume preservation after smooth",
            default = True,
            update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices")
        self.inputs.new('StringsSocket', 'Edges')
        self.inputs.new('StringsSocket', 'Faces')
        self.inputs.new('StringsSocket', 'VertMask')

        self.inputs.new('StringsSocket', 'Iterations').prop_name = "iterations"
        self.inputs.new('StringsSocket', 'ClipDist').prop_name = "clip_dist"
        self.inputs.new('StringsSocket', 'Factor').prop_name = "factor"
        self.inputs.new('StringsSocket', 'BorderFactor').prop_name = "border_factor"

        self.outputs.new('VerticesSocket', 'Vertices')
        self.outputs.new('StringsSocket', 'Edges')
        self.outputs.new('StringsSocket', 'Faces')

        self.update_mode(context)

    def draw_buttons(self, context, layout):
        col = layout.column(align=True)
        row = col.row(align=True)
        row.prop(self, "use_x", toggle=True)
        row.prop(self, "use_y", toggle=True)
        row.prop(self, "use_z", toggle=True)

        col.prop(self, "laplacian", toggle=True)

        if self.laplacian:
            col.prop(self, "preserve_volume", toggle=True)
        else:
            row = col.row(align=True)
            row.prop(self, "mirror_clip_x", toggle=True)
            row.prop(self, "mirror_clip_y", toggle=True)
            row.prop(self, "mirror_clip_z", toggle=True)

    def process(self):
        if not any(output.is_linked for output in self.outputs):
            return

        vertices_s = self.inputs['Vertices'].sv_get()
        edges_s = self.inputs['Edges'].sv_get(default=[[]])
        faces_s = self.inputs['Faces'].sv_get(default=[[]])
        masks_s = self.inputs['VertMask'].sv_get(default=[[1]])
        iterations_s = self.inputs['Iterations'].sv_get()[0]
        if not self.laplacian:
            clip_dist_s = self.inputs['ClipDist'].sv_get()[0]
        else:
            clip_dist_s = [0.0]
        factor_s = self.inputs['Factor'].sv_get()[0]
        if self.laplacian:
            border_factor_s = self.inputs['BorderFactor'].sv_get()[0]
        else:
            border_factor_s = [0.0]

        result_vertices = []
        result_edges = []
        result_faces = []

        meshes = match_long_repeat([vertices_s, edges_s, faces_s, masks_s, clip_dist_s, factor_s, border_factor_s, iterations_s])
        for vertices, edges, faces, masks, clip_dist, factor, border_factor, iterations in zip(*meshes):
            fullList(masks,  len(vertices))

            bm = bmesh_from_pydata(vertices, edges, faces, normal_update=True)
            bm.verts.ensure_lookup_table()
            bm.edges.ensure_lookup_table()
            bm.faces.ensure_lookup_table()
            selected_verts = [vert for mask, vert in zip(masks, bm.verts) if mask]

            for i in range(iterations):
                if self.laplacian:
                    # for some reason smooth_laplacian_vert does not work properly if faces are not selected
                    for f in bm.faces:
                        f.select = True
                    bmesh.ops.smooth_laplacian_vert(bm, verts = selected_verts,
                            lambda_factor = factor,
                            lambda_border = border_factor,
                            use_x = self.use_x,
                            use_y = self.use_y,
                            use_z = self.use_z,
                            preserve_volume = self.preserve_volume)
                else:
                    bmesh.ops.smooth_vert(bm, verts = selected_verts,
                            factor = factor,
                            mirror_clip_x = self.mirror_clip_x,
                            mirror_clip_y = self.mirror_clip_y,
                            mirror_clip_z = self.mirror_clip_z,
                            clip_dist = clip_dist,
                            use_axis_x = self.use_x,
                            use_axis_y = self.use_y,
                            use_axis_z = self.use_z)

            new_vertices, new_edges, new_faces = pydata_from_bmesh(bm)
            bm.free()

            result_vertices.append(new_vertices)
            result_edges.append(new_edges)
            result_faces.append(new_faces)

        self.outputs['Vertices'].sv_set(result_vertices)
        self.outputs['Edges'].sv_set(result_edges)
        self.outputs['Faces'].sv_set(result_faces)

def register():
    bpy.utils.register_class(SvSmoothNode)


def unregister():
    bpy.utils.unregister_class(SvSmoothNode)

