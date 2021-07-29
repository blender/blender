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
from bpy.props import FloatProperty
import bmesh

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, Vector_generate,
                                     Vector_degenerate, match_long_repeat)

from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata
# "coauthor": "Alessandro Zomparelli (sketchesofcode)"


class AdaptivePolsNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Make spread one object on another adaptively polygons of mesh (not including matrixes, so apply scale-rot-loc ctrl+A) '''
    bl_idname = 'AdaptivePolsNode'
    bl_label = 'Adaptive Polygons'
    bl_icon = 'OUTLINER_OB_EMPTY'

    width_coef = FloatProperty(name='width_coef',
                               description='with coefficient for sverchok adaptivepols donors size',
                               default=1.0, max=3.0, min=0.5,
                               update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', "VersR", "VersR")
        self.inputs.new('StringsSocket', "PolsR", "PolsR")
        self.inputs.new('VerticesSocket', "VersD", "VersD")
        self.inputs.new('StringsSocket', "PolsD", "PolsD")
        self.inputs.new('StringsSocket', "Z_Coef", "Z_Coef")
        self.outputs.new('VerticesSocket', "Vertices", "Vertices")
        self.outputs.new('StringsSocket', "Poligons", "Poligons")

    def draw_buttons(self, context, layout):
        layout.prop(self, "width_coef", text="donor width")

    def lerp(self, v1, v2, v3, v4, v):
        v12 = v1 + (v2-v1)*v[0] + ((v2-v1)/2)
        v43 = v4 + (v3-v4)*v[0] + ((v3-v4)/2)
        return v12 + (v43-v12)*v[1] + ((v43-v12)/2)

    def lerp2(self, v1, v2, v3, v4, v, x, y):
        v12 = v1 + (v2-v1)*v[0]*x + ((v2-v1)/2)
        v43 = v4 + (v3-v4)*v[0]*x + ((v3-v4)/2)
        return v12 + (v43-v12)*v[1]*y + ((v43-v12)/2)

    def lerp3(self, v1, v2, v3, v4, v, x, y, z):
        loc = self.lerp2(v1.co, v2.co, v3.co, v4.co, v, x, y)
        nor = self.lerp(v1.normal, v2.normal, v3.normal, v4.normal, v)
        nor.normalize()
        #print (loc, nor, v[2], z)
        return loc + nor*v[2]*z

    def process(self):
        # достаём два слота - вершины и полики
        if all(s.is_linked for s in self.inputs[:-1]):
            if self.inputs['Z_Coef'].is_linked:
                z_coef = self.inputs['Z_Coef'].sv_get()[0]
            else:
                z_coef = []

            polsR = self.inputs['PolsR'].sv_get()[0]  # recipient one object [0]
            versR = self.inputs['VersR'].sv_get()[0]  # recipient
            polsD = self.inputs['PolsD'].sv_get()  # donor many objects [:]
            versD_ = self.inputs['VersD'].sv_get()  # donor
            versD = Vector_generate(versD_)
            polsR, polsD, versD = match_long_repeat([polsR, polsD, versD])

            bm = bmesh_from_pydata(versR, [], polsR, normal_update=True)
            bm.verts.ensure_lookup_table()
            new_ve = bm.verts

            vers_out = []
            pols_out = []
            i = 0
            for vD, pR in zip(versD, polsR):
                # part of donor to make limits
                j = i
                pD = polsD[i]

                xx = [x[0] for x in vD]
                x0 = (self.width_coef) / (max(xx)-min(xx))
                yy = [y[1] for y in vD]
                y0 = (self.width_coef) / (max(yy)-min(yy))
                zz = [z[2] for z in vD]
                zzz = (max(zz)-min(zz))
                if zzz:
                    z0 = 1 / zzz
                else:
                    z0 = 0

                # part of recipient polygons to reciev donor
                last = len(pR)-1
                vs = [new_ve[v] for v in pR]  # new_ve  - temporery data
                if z_coef:
                    if j < len(z_coef):
                        z1 = z0 * z_coef[j]
                else:
                    z1 = z0

                new_vers = []
                new_pols = []
                for v in vD:
                    new_vers.append(self.lerp3(vs[0], vs[1], vs[2], vs[last], v, x0, y0, z1))
                for p in pD:
                    new_pols.append([id for id in p])
                pols_out.append(new_pols)
                vers_out.append(new_vers)
                i += 1

            bm.free()
            output = Vector_degenerate(vers_out)

            self.outputs['Vertices'].sv_set(output)
            self.outputs['Poligons'].sv_set(pols_out)


def register():
    bpy.utils.register_class(AdaptivePolsNode)


def unregister():
    bpy.utils.unregister_class(AdaptivePolsNode)

#if __name__ == '__main__':
#    register()
