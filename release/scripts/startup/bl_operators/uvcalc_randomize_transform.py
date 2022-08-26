# SPDX-License-Identifier: GPL-2.0-or-later

from bpy.types import Operator
from mathutils import Vector

import math


def get_random_transform(transform_params, entropy):
    from random import uniform
    from random import seed as random_seed

    (seed, loc, rot, scale, scale_even) = transform_params

    # First, seed the RNG.
    random_seed(seed + entropy)

    # Next, call uniform a known number of times.
    offset_u = uniform(0, 1)
    offset_v = uniform(0, 1)
    angle = uniform(0, 1)
    scale_u = uniform(0, 1)
    scale_v = uniform(0, 1)

    # Apply the transform_params.
    if loc:
        offset_u *= loc[0]
        offset_v *= loc[1]
    else:
        offset_u = 0
        offset_v = 0

    if rot:
        angle *= rot
    else:
        angle = 0

    if scale:
        scale_u = scale_u * (2 * scale[0] - 2.0) + 2.0 - scale[0]
        scale_v = scale_v * (2 * scale[1] - 2.0) + 2.0 - scale[1]
    else:
        scale_u = 1
        scale_v = 1

    if scale_even:
        scale_v = scale_u

    # Results in homogenous co-ordinates.
    return [[scale_u * math.cos(angle), -scale_v * math.sin(angle), offset_u],
            [scale_u * math.sin(angle), scale_v * math.cos(angle), offset_v]]


def randomize_uv_transform_island(bm, uv_layer, faces, transform_params):
    # Ensure consistent random values for island, regardless of selection etc.
    entropy = min(f.index for f in faces)

    transform = get_random_transform(transform_params, entropy)

    # Find bounding box.
    minmax = [1e30, 1e30, -1e30, -1e30]
    for face in faces:
        for loop in face.loops:
            u, v = loop[uv_layer].uv
            minmax[0] = min(minmax[0], u)
            minmax[1] = min(minmax[1], v)
            minmax[2] = max(minmax[2], u)
            minmax[3] = max(minmax[3], v)

    mid_u = (minmax[0] + minmax[2]) / 2
    mid_v = (minmax[1] + minmax[3]) / 2

    del_u = transform[0][2] + mid_u - transform[0][0] * mid_u - transform[0][1] * mid_v
    del_v = transform[1][2] + mid_v - transform[1][0] * mid_u - transform[1][1] * mid_v

    # Apply transform.
    for face in faces:
        for loop in face.loops:
            pre_uv = loop[uv_layer].uv
            u = transform[0][0] * pre_uv[0] + transform[0][1] * pre_uv[1] + del_u
            v = transform[1][0] * pre_uv[0] + transform[1][1] * pre_uv[1] + del_v
            loop[uv_layer].uv = (u, v)


def is_face_uv_selected(face, uv_layer):
    for loop in face.loops:
        if not loop[uv_layer].select:
            return False
    return True


def is_island_uv_selected(bm, island, uv_layer):
    for face in island:
        if is_face_uv_selected(face, uv_layer):
            return True
    return False


def randomize_uv_transform_bmesh(mesh, bm, transform_params):
    import bpy_extras.bmesh_utils
    uv_layer = bm.loops.layers.uv.verify()
    islands = bpy_extras.bmesh_utils.bmesh_linked_uv_islands(bm, uv_layer)
    for island in islands:
        if is_island_uv_selected(bm, island, uv_layer):
            randomize_uv_transform_island(bm, uv_layer, island, transform_params)


def randomize_uv_transform(context, transform_params):
    import bmesh
    ob_list = context.objects_in_mode_unique_data
    for ob in ob_list:
        bm = bmesh.from_edit_mesh(ob.data)
        if not bm.loops.layers.uv:
            continue

        # Only needed to access the minimum face index of each island.
        bm.faces.index_update()
        randomize_uv_transform_bmesh(ob.data, bm, transform_params)

    for ob in ob_list:
        bmesh.update_edit_mesh(ob.data)

    return {'FINISHED'}


from bpy.props import (
    BoolProperty,
    FloatProperty,
    FloatVectorProperty,
    IntProperty,
)


class RandomizeUVTransform(Operator):
    """Randomize uv island's location, rotation, and scale"""
    bl_idname = "uv.randomize_uv_transform"
    bl_label = "Randomize"
    bl_options = {'REGISTER', 'UNDO'}

    random_seed: IntProperty(
        name="Random Seed",
        description="Seed value for the random generator",
        min=0,
        max=10000,
        default=0,
    )
    use_loc: BoolProperty(
        name="Randomize Location",
        description="Randomize the location values",
        default=True,
    )
    loc: FloatVectorProperty(
        name="Location",
        description=("Maximum distance the objects "
                     "can spread over each axis"),
        min=-100.0,
        max=100.0,
        size=2,
        subtype='TRANSLATION',
        default=(0.0, 0.0),
    )
    use_rot: BoolProperty(
        name="Randomize Rotation",
        description="Randomize the rotation value",
        default=True,
    )
    rot: FloatProperty(
        name="Rotation",
        description="Maximum rotation",
        min=-2 * math.pi,
        max=2 * math.pi,
        subtype='ANGLE',
        default=0.0,
    )
    use_scale: BoolProperty(
        name="Randomize Scale",
        description="Randomize the scale values",
        default=True,
    )
    scale_even: BoolProperty(
        name="Scale Even",
        description="Use the same scale value for both axes",
        default=False,
    )

    scale: FloatVectorProperty(
        name="Scale",
        description="Maximum scale randomization over each axis",
        min=-100.0,
        max=100.0,
        default=(1.0, 1.0),
        size=2,
    )

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_MESH'

    def execute(self, context):
        seed = self.random_seed

        loc = [0, 0] if not self.use_loc else self.loc
        rot = 0 if not self.use_rot else self.rot
        scale = None if not self.use_scale else self.scale
        scale_even = self.scale_even

        transformParams = [seed, loc, rot, scale, scale_even]
        return randomize_uv_transform(context, transformParams)


classes = (
    RandomizeUVTransform,
)
