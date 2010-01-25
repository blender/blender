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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

import bpy


def randomize_selected(seed, loc, rot, scale, scale_even, scale_min):

    import random
    from random import uniform
    from Mathutils import Vector

    random.seed(seed)

    def rand_vec(vec_range):
        return Vector([uniform(- val, val) for val in vec_range])

    for obj in bpy.context.selected_objects:

        if loc:
            obj.location += rand_vec(loc)
        else: # otherwise the values change under us
            uniform(0.0, 0.0), uniform(0.0, 0.0), uniform(0.0, 0.0)

        if rot: # TODO, non euler's
            vec = rand_vec(rot)
            obj.rotation_euler[0] += vec[0]
            obj.rotation_euler[1] += vec[1]
            obj.rotation_euler[2] += vec[2]
        else:
            uniform(0.0, 0.0), uniform(0.0, 0.0), uniform(0.0, 0.0)

        if scale:
            org_sca_x, org_sca_y, org_sca_z = obj.scale

            if scale_even:
                sca_x = sca_y = sca_z = uniform(scale[0], - scale[0])
                uniform(0.0, 0.0), uniform(0.0, 0.0)
            else:
                sca_x, sca_y, sca_z = rand_vec(scale)

            aX = sca_x + org_sca_x
            bX = org_sca_x * scale_min

            aY = sca_y + org_sca_y
            bY = org_sca_y * scale_min

            aZ = sca_z + org_sca_z
            bZ = org_sca_z * scale_min

            if aX < bX:
                aX = bX
            if aY < bY:
                aY = bY
            if aZ < bZ:
                aZ = bZ

            obj.scale = aX, aY, aZ
        else:
            uniform(0.0, 0.0), uniform(0.0, 0.0), uniform(0.0, 0.0)

from bpy.props import *


class RandomizeLocRotSize(bpy.types.Operator):
    '''Randomize objects loc/rot/scale.'''
    bl_idname = "object.randomize_locrotsize"
    bl_label = "Randomize Loc Rot Size"
    bl_register = True
    bl_undo = True

    random_seed = IntProperty(name="Random Seed",
        description="Seed value for the random generator",
        default=0, min=0, max=1000)

    use_loc = BoolProperty(name="Randomize Location",
        description="Randomize the scale values", default=True)

    loc = FloatVectorProperty(name="Location",
        description="Maximun distance the objects can spread over each axis",
        default=(0.0, 0.0, 0.0), min=-100.0, max=100.0)

    use_rot = BoolProperty(name="Randomize Rotation",
        description="Randomize the rotation values", default=True)

    rot = FloatVectorProperty(name="Rotation",
        description="Maximun rotation over each axis",
        default=(0.0, 0.0, 0.0), min=-180.0, max=180.0)

    use_scale = BoolProperty(name="Randomize Scale",
        description="Randomize the scale values", default=True)

    scale_even = BoolProperty(name="Scale Even",
        description="Use the same scale value for all axis", default=False)

    scale_min = FloatProperty(name="Minimun Scale Factor",
        description="Lowest scale percentage possible",
        default=0.15, min=-1.0, max=1.0, precision=3)

    scale = FloatVectorProperty(name="Scale",
        description="Maximum scale randomization over each axis",
        default=(0.0, 0.0, 0.0), min=-100.0, max=100.0)

    def execute(self, context):
        from math import radians
        seed = self.properties.random_seed

        loc = None if not self.properties.use_loc else self.properties.loc
        rot = None if not self.properties.use_rot else self.properties.rot * radians(1.0)
        scale = None if not self.properties.use_scale else self.properties.scale

        scale_even = self.properties.scale_even
        scale_min = self.properties.scale_min

        randomize_selected(seed, loc, rot, scale, scale_even, scale_min)

        return {'FINISHED'}


# Register the operator
bpy.types.register(RandomizeLocRotSize)


def menu_func(self, context):
    if context.mode == 'OBJECT':
        self.layout.operator(RandomizeLocRotSize.bl_idname,
        text="Randomize Loc Rot Size")

bpy.types.VIEW3D_MT_transform.append(menu_func)
