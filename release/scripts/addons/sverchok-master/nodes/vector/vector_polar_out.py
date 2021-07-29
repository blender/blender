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

from math import sin, cos, atan, atan2, degrees, sqrt, acos
import bpy
from bpy.props import FloatProperty, EnumProperty
from sverchok.node_tree import SverchCustomTreeNode, StringsSocket
from sverchok.data_structure import updateNode, match_long_repeat

def cylindrical(v, mode):
    x,y,z = v
    rho = sqrt(x*x + y*y)
    phi = atan2(y,x)
    if mode == "degrees":
        phi = degrees(phi)
    return rho, phi, z

def spherical(v, mode):
    x,y,z = v
    rho = sqrt(x*x + y*y + z*z)
    if rho == 0.0:
        return 0.0, 0.0, 0.0
    theta = acos(z/rho)
    phi = atan2(y,x)
    if mode == "degrees":
        phi = degrees(phi)
        theta = degrees(theta)
    return rho, phi, theta

class VectorPolarOutNode(bpy.types.Node, SverchCustomTreeNode):
    '''Get cylindrical or spherical coordinates from vectors'''
    bl_idname = 'VectorPolarOutNode'
    bl_label = 'Vector polar output'
    bl_icon = 'OUTLINER_OB_EMPTY'

    coord_modes = [
        ("z", "Cylinder", "Use cylindrical coordinates", 1),
        ("theta",  "Sphere", "Use spherical coordinates", 2),
    ]

    def coordinate_changed(self, context):
        self.outputs["z"].hide = self.coordinates == "theta"
        self.outputs["theta"].hide = self.coordinates == "z"
        updateNode(self, context)

    coordinates = EnumProperty(items=coord_modes, default='z', update=coordinate_changed)

    func_dict = {'z': cylindrical, 'theta': spherical}

    angle_modes = [
            ("radians", "Radian", "Use angles in radians", 1),
            ("degrees", "Degree", "Use angles in degrees", 2)
        ]

    angles_mode = EnumProperty(items=angle_modes, default="radians", update=updateNode)

    def sv_init(self, context):
        self.inputs.new("VerticesSocket", "Vectors")
        self.width = 100
        self.outputs.new('StringsSocket', "rho")
        self.outputs.new('StringsSocket', "phi")
        self.outputs.new('StringsSocket', "z")
        self.outputs.new('StringsSocket', "theta").hide = True

    def draw_buttons(self, context, layout):
        layout.prop(self, "coordinates", expand=True)
        layout.prop(self, "angles_mode", expand=True)
    
    def process(self):
        if not (self.outputs['rho'].is_linked or self.outputs['phi'].is_linked or self.outputs[self.coordinates].is_linked):
            return

        vss = self.inputs['Vectors'].sv_get()

        result_rhos = []
        result_phis = []
        result_zs = []
        for vs in vss:
            rs = []
            ps = []
            zs = []
            for v in vs:
                rho, phi, z = self.func_dict[self.coordinates](v, self.angles_mode)
                rs.append(rho)
                ps.append(phi)
                zs.append(z)
            result_rhos.append(rs)
            result_phis.append(ps)
            result_zs.append(zs)

        if self.outputs['rho'].is_linked:
            self.outputs['rho'].sv_set(result_rhos)
        if self.outputs['phi'].is_linked:
            self.outputs['phi'].sv_set(result_phis)
        if self.outputs[self.coordinates].is_linked:
            self.outputs[self.coordinates].sv_set(result_zs)
    
def register():
    bpy.utils.register_class(VectorPolarOutNode)


def unregister():
    bpy.utils.unregister_class(VectorPolarOutNode)


