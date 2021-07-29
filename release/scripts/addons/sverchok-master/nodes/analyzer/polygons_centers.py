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
from sverchok.data_structure import (Vector_generate, Vector_degenerate, updateNode)


class CentersPolsNodeMK2(bpy.types.Node, SverchCustomTreeNode):
    ''' Centers of polygons of mesh (not including matrixes, so apply scale-rot-loc ctrl+A) '''
    bl_idname = 'CentersPolsNodeMK2'
    bl_label = 'Centers polygons 2'
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
            # medians в векторах
            medians = []
            normals = []
            centrs = []
            norm_abs = []
            for p in pols:
                # medians
                # it calcs middle point of opposite edges, 
                # than finds length vector between this two points
                v0 = versv[p[0]]
                v1 = versv[p[1]]
                v2 = versv[p[2]]
                lp=len(p)
                if lp >= 4:
                    l = ((lp-2)//2) + 2
                    v3 = versv[p[l]]
                    poi_2 = (v2+v3)/2
                    # normals
                    norm = geometry.normal(v0, v1, v2, v3)
                    normals.append(norm)
                else:
                    poi_2 = v2
                    # normals
                    norm = geometry.normal(v0, v1, v2)
                    normals.append(norm)
                poi_1 = (v0+v1)/2
                vm = poi_2 - poi_1
                medians.append(vm)
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
            for cen, med, nor in zip(centrs, medians, normals):
                loc = Matrix.Translation(cen)
                # need better solution for Z,Y vectors + may be X vector correction
                vecz = Vector((0, 1e-6, 1))
                q_rot0 = vecz.rotation_difference(nor).to_matrix().to_4x4()
                q_rot2 = nor.rotation_difference(vecz).to_matrix().to_4x4()
                if med[1]>med[0]:
                    vecy = Vector((1e-6, 1, 0)) * q_rot2
                else:
                    vecy = Vector((1, 1e-6, 0)) * q_rot2
                q_rot1 = vecy.rotation_difference(med).to_matrix().to_4x4()
                # loc is matrix * rot vector * rot vector
                M = loc*q_rot1*q_rot0
                lM = [ j[:] for j in M ]
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
    bpy.utils.register_class(CentersPolsNodeMK2)


def unregister():
    bpy.utils.unregister_class(CentersPolsNodeMK2)
