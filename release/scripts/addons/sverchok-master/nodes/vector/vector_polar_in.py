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

from math import sin, cos, radians
import bpy
from bpy.props import FloatProperty, EnumProperty
from sverchok.node_tree import SverchCustomTreeNode, StringsSocket
from sverchok.data_structure import updateNode, match_long_repeat

def cylindrical(rho, phi, z, mode):
    if mode == "degrees":
        phi = radians(phi)
    x = rho*cos(phi)
    y = rho*sin(phi)
    return x, y, z

def spherical(rho, phi, theta, mode):
    if mode == "degrees":
        phi = radians(phi)
        theta = radians(theta)
    x = rho * sin(theta) * cos(phi)
    y = rho * sin(theta) * sin(phi)
    z = rho * cos(theta)
    return x, y, z

class VectorPolarInNode(bpy.types.Node, SverchCustomTreeNode):
    '''Generate vectors by spherical or cylindrical coordinates'''
    bl_idname = 'VectorPolarInNode'
    bl_label = 'Vector polar input'
    bl_icon = 'OUTLINER_OB_EMPTY'


    rho_ = FloatProperty(name='rho', description='Rho coordinate',
            default=0.0, precision=3,
            update=updateNode)
    phi_ = FloatProperty(name='phi', description='Phi coordinate',
            default=0.0, precision=3,
            update=updateNode)
    theta_ = FloatProperty(name='theta', description='Theta coordinate',
            default=0.0, precision=3,
            update=updateNode)
    z_ = FloatProperty(name='Z', description='Z coordinate',
            default=0.0, precision=3,
            update=updateNode)

    coord_modes = [
        ("z_", "Cylinder", "Use cylindrical coordinates", 1),
        ("theta_",  "Sphere", "Use spherical coordinates", 2),
    ]

    def coordinate_changed(self, context):
        self.inputs[2].prop_name = self.coordinates
        updateNode(self, context)

    coordinates = EnumProperty(items=coord_modes, default='z_', update=coordinate_changed)

    func_dict = {'z_': cylindrical, 'theta_': spherical}

    angle_modes = [
            ("radians", "Radian", "Use angles in radians", 1),
            ("degrees", "Degree", "Use angles in degrees", 2)
        ]

    angles_mode = EnumProperty(items=angle_modes, default="radians", update=updateNode)

    def sv_init(self, context):
        self.inputs.new('StringsSocket', "rho").prop_name = 'rho_'
        self.inputs.new('StringsSocket', "phi").prop_name = 'phi_'
        self.inputs.new('StringsSocket', "Z").prop_name = 'z_'
        self.width = 100
        self.outputs.new('VerticesSocket', "Vectors")

    def draw_buttons(self, context, layout):
        layout.prop(self, "coordinates", expand=True)
        layout.prop(self, "angles_mode", expand=True)
    
    def process(self):
        if not self.outputs['Vectors'].is_linked:
            return
        inputs = self.inputs
        rhoss = inputs['rho'].sv_get()
        phiss = inputs['phi'].sv_get()
        zss = inputs['Z'].sv_get()

        parameters = match_long_repeat([rhoss, phiss, zss])
        result = []
        for rhos, phis, zs in zip(*parameters):
            vs = []
            ps = match_long_repeat([rhos, phis, zs])
            for rho, phi, z in zip(*ps):
                v = self.func_dict[self.coordinates](rho, phi, z, self.angles_mode)
                vs.append(v)
            result.append(vs)

        self.outputs['Vectors'].sv_set(result)
    
    
def register():
    bpy.utils.register_class(VectorPolarInNode)


def unregister():
    bpy.utils.unregister_class(VectorPolarInNode)

