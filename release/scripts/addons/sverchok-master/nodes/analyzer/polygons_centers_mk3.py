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
from mathutils import Vector, Matrix, geometry
from bpy.props import BoolProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import Vector_generate, Vector_degenerate, updateNode


class CentersPolsNodeMK3(bpy.types.Node, SverchCustomTreeNode):
    ''' Centers of polygons of mesh (not including matrixes, so apply scale-rot-loc ctrl+A) '''
    bl_idname = 'CentersPolsNodeMK3'
    bl_label = 'Centers polygons 3'
    bl_icon = 'OUTLINER_OB_EMPTY'

    Separate = BoolProperty(
        name="Separate", description="separate by objects", 
        default=True, update=updateNode)

    def draw_buttons(self, context, layout):
        layout.prop(self, "Separate", text="Separate")

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices")
        self.inputs.new('StringsSocket', "Polygons")
        self.outputs.new('VerticesSocket', "Normals")
        self.outputs.new('VerticesSocket', "Norm_abs")
        self.outputs.new('VerticesSocket', "Origins")
        self.outputs.new('MatrixSocket', "Centers")

    def process(self):
        verts_socket, poly_socket = self.inputs
        norm_socket, norm_abs_socket, origins_socket, centers_socket = self.outputs

        if not any([s.is_linked for s in self.outputs]):
            return

        if not (verts_socket.is_linked and poly_socket.is_linked):
            return

        pols_ = poly_socket.sv_get()
        vers_tupls = verts_socket.sv_get()
        vers_vects = Vector_generate(vers_tupls)

        # make mesh temp утилитарно - удалить в конце
        mat_collect = []
        normals_out = []
        origins = []
        norm_abs_out = []
        for verst, versv, pols in zip(vers_tupls, vers_vects, pols_):
            normals = []
            centrs = []
            norm_abs = []
            p0_xdirs = []
            for p in pols:
                v0 = versv[p[0]]
                v1 = versv[p[1]]
                v2 = versv[p[2]]
                # save direction of 1st point in polygon
                p0_xdirs.append(v0)
                # normals
                norm = geometry.normal(v0, v1, v2)
                normals.append(norm)
                # centrs
                x,y,z = zip(*[verst[poi] for poi in p])
                x,y,z = sum(x)/len(x), sum(y)/len(y), sum(z)/len(z)
                current_center = Vector((x,y,z))
                centrs.append(current_center)
                # normal absolute !!!
                # это совершенно нормально!!! ;-)
                norm_abs.append(current_center+norm)

            if self.Separate:
                norm_abs_out.append(norm_abs)
                origins.append(centrs)
                normals_out.append(normals)
            else:
                norm_abs_out.extend(norm_abs)
                origins.extend(centrs)
                normals_out.extend(normals)
                
            mat_collect_ = []

            for cen, nor, p0 in zip(centrs, normals, p0_xdirs):
                zdir = nor
                xdir = (Vector(p0) - cen).normalized()
                ydir = zdir.cross(xdir)
                lM = [(xdir[0], ydir[0], zdir[0], cen[0]),
                      (xdir[1], ydir[1], zdir[1], cen[1]),
                      (xdir[2], ydir[2], zdir[2], cen[2]),
                      (0.0, 0.0, 0.0, 1.0)]
                mat_collect_.append(lM)
            mat_collect.extend(mat_collect_)

        if not self.Separate:
            norm_abs_out = [norm_abs_out]
            origins = [origins]
            normals_out = [normals_out]

        centers_socket.sv_set(mat_collect)
        norm_abs_socket.sv_set(Vector_degenerate(norm_abs_out))
        origins_socket.sv_set(Vector_degenerate(origins))
        norm_socket.sv_set(Vector_degenerate(normals_out))


def register():
    bpy.utils.register_class(CentersPolsNodeMK3)


def unregister():
    bpy.utils.unregister_class(CentersPolsNodeMK3)
