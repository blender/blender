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

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import SvSetSocketAnyType, SvGetSocketAnyType, \
                        Vector_generate, Vector_degenerate


class CentersPolsNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Centers of polygons of mesh (not including matrixes, so apply scale-rot-loc ctrl+A) '''
    bl_idname = 'CentersPolsNode'
    bl_label = 'Centers polygons'
    bl_icon = 'OUTLINER_OB_EMPTY'

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "Vertices", "Vertices")
        self.inputs.new('StringsSocket', "Polygons", "Polygons")
        self.outputs.new('VerticesSocket', "Normals", "Normals")
        self.outputs.new('VerticesSocket', "Norm_abs", "Norm_abs")
        self.outputs.new('VerticesSocket', "Origins", "Origins")
        self.outputs.new('MatrixSocket', "Centers", "Centers")

    def process(self):
        if self.outputs['Centers'].is_linked or self.outputs['Normals'].is_linked or \
                self.outputs['Origins'].is_linked or self.outputs['Norm_abs'].is_linked:
            if 'Polygons' in self.inputs and 'Vertices' in self.inputs \
                and self.inputs['Polygons'].is_linked and self.inputs['Vertices'].is_linked:

                pols_ = SvGetSocketAnyType(self, self.inputs['Polygons'])
                vers_tupls = SvGetSocketAnyType(self, self.inputs['Vertices'])
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
                        
                    norm_abs_out.append(norm_abs)    
                    origins.append(centrs)
                    normals_out.extend(normals)
                    mat_collect_ = []
                    for cen, med, nor in zip(centrs, medians, normals):
                        loc = Matrix.Translation(cen)
                        # need better solution for Z,Y vectors + may be X vector correction
                        vecz = Vector((0, 1e-6, 1))
                        q_rot0 = vecz.rotation_difference(nor).to_matrix().to_4x4()
                        q_rot2 = nor.rotation_difference(vecz).to_matrix().to_4x4()
                        vecy = Vector((1e-6, 1, 0)) * q_rot2
                        q_rot1 = vecy.rotation_difference(med).to_matrix().to_4x4()
                        # loc is matrix * rot vector * rot vector
                        M = loc*q_rot1*q_rot0
                        lM = [ j[:] for j in M ]
                        mat_collect_.append(lM)
                    mat_collect.extend(mat_collect_)
                
                SvSetSocketAnyType(self, 'Centers', mat_collect)
                SvSetSocketAnyType(self, 'Norm_abs', Vector_degenerate(norm_abs_out))
                SvSetSocketAnyType(self, 'Origins', Vector_degenerate(origins))
                SvSetSocketAnyType(self, 'Normals', Vector_degenerate([normals_out]))


def register():
    bpy.utils.register_class(CentersPolsNode)


def unregister():
    bpy.utils.unregister_class(CentersPolsNode)
    
if __name__ == '__main__':
    register()



