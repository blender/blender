# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import math

from bpy.types import Operator
from mathutils import Matrix, Vector

from bpy.props import (
    BoolProperty,
    EnumProperty,
    FloatProperty,
    FloatVectorProperty,
    IntProperty,
)


# ------------------------------------------------------------------------------
# Local Utility Functions

def is_face_uv_selected(face, uv_layer, any_edge):
    # Returns True if the face is UV selected.
    #
    # :arg face: the face to query.
    # :type face: :class:`BMFace`
    # :arg uv_layer: the UV layer to source UVs from.
    # :type bmesh: :class:`BMLayerItem`
    # :arg any_edge: use edge selection instead of vertex selection.
    # :type any_edge: bool
    # :return: True if the face is UV selected.
    # :rtype: bool

    if not face.select:  # Geometry selection
        return False

    import bpy
    if bpy.context.tool_settings.use_uv_select_sync:
        # In sync selection mode, UV selection comes solely from geometry selection.
        return True

    if any_edge:
        for loop in face.loops:
            if loop[uv_layer].select_edge:
                return True
        return False

    for loop in face.loops:
        if not loop[uv_layer].select:
            return False
    return True


def is_island_uv_selected(island, uv_layer, any_edge):
    # Returns True if the island is UV selected.
    #
    # :arg island: list of faces to query.
    # :type island: Sequence[:class:`BMFace`]
    # :arg uv_layer: the UV layer to source UVs from.
    # :type bmesh: :class:`BMLayerItem`
    # :arg any_edge: use edge selection instead of vertex selection.
    # :type any_edge: bool
    # :return: list of lists containing polygon indices.
    # :rtype: bool
    for face in island:
        if is_face_uv_selected(face, uv_layer, any_edge):
            return True
    return False


def island_uv_bounds(island, uv_layer):
    # The UV bounds of UV island.
    #
    # :arg island: list of faces to query.
    # :type island: Sequence[:class:`BMFace`]
    # :arg uv_layer: the UV layer to source UVs from.
    # :return: U-min, V-min, U-max, V-max.
    # :rtype: list[float]
    minmax = [1e30, 1e30, -1e30, -1e30]
    for face in island:
        for loop in face.loops:
            u, v = loop[uv_layer].uv
            minmax[0] = min(minmax[0], u)
            minmax[1] = min(minmax[1], v)
            minmax[2] = max(minmax[2], u)
            minmax[3] = max(minmax[3], v)
    return minmax


def island_uv_bounds_center(island, uv_layer):
    # The UV bounds center of UV island.
    #
    # :arg island: list of faces to query.
    # :type island: Sequence[:class:`BMFace`]
    # :arg uv_layer: the UV layer to source UVs from.
    # :return: U, V center.
    # :rtype: tuple[float, float]
    minmax = island_uv_bounds(island, uv_layer)
    return (
        (minmax[0] + minmax[2]) / 2.0,
        (minmax[1] + minmax[3]) / 2.0,
    )


# ------------------------------------------------------------------------------
# Align UV Rotation Operator

def find_rotation_auto(bm, uv_layer, faces, aspect_y):
    sum_u = 0.0
    sum_v = 0.0
    for face in faces:
        prev_uv = face.loops[-1][uv_layer].uv
        for loop in face.loops:
            uv = loop[uv_layer].uv
            du = uv[0] - prev_uv[0]
            dv = uv[1] - prev_uv[1]
            edge_angle = math.atan2(dv, du * aspect_y)
            edge_angle *= 4.0  # Wrap 4 times around the circle
            sum_u += math.cos(edge_angle)
            sum_v += math.sin(edge_angle)
            prev_uv = uv

    # Compute angle.
    return -math.atan2(sum_v, sum_u) / 4.0


def find_rotation_edge(bm, uv_layer, faces, aspect_y):
    sum_u = 0.0
    sum_v = 0.0
    for face in faces:
        prev_uv = face.loops[-1][uv_layer].uv
        prev_select = face.loops[-1][uv_layer].select_edge
        for loop in face.loops:
            uv = loop[uv_layer].uv
            if prev_select:
                du = uv[0] - prev_uv[0]
                dv = uv[1] - prev_uv[1]
                edge_angle = math.atan2(dv, du * aspect_y)
                edge_angle *= 2.0  # Wrap 2 times around the circle
                sum_u += math.cos(edge_angle)
                sum_v += math.sin(edge_angle)

            prev_uv = uv
            prev_select = loop[uv_layer].select_edge

    # Add 90 degrees to align along V co-ordinate.
    # Twice, because we divide by two.
    sum_u, sum_v = -sum_u, -sum_v

    # Compute angle.
    return -math.atan2(sum_v, sum_u) / 2.0


def find_rotation_geometry(bm, uv_layer, faces, method, axis, aspect_y):
    sum_u_co = Vector((0.0, 0.0, 0.0))
    sum_v_co = Vector((0.0, 0.0, 0.0))
    for face in faces:
        # Triangulate.
        for fan in range(2, len(face.loops)):
            delta_uv0 = face.loops[fan - 1][uv_layer].uv - face.loops[0][uv_layer].uv
            delta_uv1 = face.loops[fan][uv_layer].uv - face.loops[0][uv_layer].uv

            delta_uv0[0] *= aspect_y
            delta_uv1[0] *= aspect_y

            mat = Matrix((delta_uv0, delta_uv1))
            mat.invert_safe()

            delta_co0 = face.loops[fan - 1].vert.co - face.loops[0].vert.co
            delta_co1 = face.loops[fan].vert.co - face.loops[0].vert.co
            w = delta_co0.cross(delta_co1).length
            # U direction in geometry co-ordinates.
            sum_u_co += (delta_co0 * mat[0][0] + delta_co1 * mat[0][1]) * w
            # V direction in geometry co-ordinates.
            sum_v_co += (delta_co0 * mat[1][0] + delta_co1 * mat[1][1]) * w

    if axis == 'X':
        axis_index = 0
    elif axis == 'Y':
        axis_index = 1
    elif axis == 'Z':
        axis_index = 2

    # Compute angle.
    return math.atan2(sum_u_co[axis_index], sum_v_co[axis_index])


def align_uv_rotation_island(bm, uv_layer, faces, method, axis, aspect_y):
    angle = 0.0
    if method == 'AUTO':
        angle = find_rotation_auto(bm, uv_layer, faces, aspect_y)
    elif method == 'EDGE':
        angle = find_rotation_edge(bm, uv_layer, faces, aspect_y)
    elif method == 'GEOMETRY':
        angle = find_rotation_geometry(bm, uv_layer, faces, method, axis, aspect_y)

    if angle == 0.0:
        return False  # No change.

    # Find bounding box center.
    mid_u, mid_v = island_uv_bounds_center(faces, uv_layer)

    cos_angle = math.cos(angle)
    sin_angle = math.sin(angle)

    delta_u = mid_u - cos_angle * mid_u + sin_angle / aspect_y * mid_v
    delta_v = mid_v - sin_angle * aspect_y * mid_u - cos_angle * mid_v

    # Apply transform.
    for face in faces:
        for loop in face.loops:
            pre_uv = loop[uv_layer].uv
            u = cos_angle * pre_uv[0] - sin_angle / aspect_y * pre_uv[1] + delta_u
            v = sin_angle * aspect_y * pre_uv[0] + cos_angle * pre_uv[1] + delta_v
            loop[uv_layer].uv = u, v

    return True


def align_uv_rotation_bmesh(bm, method, axis, aspect_y):
    import bpy_extras.bmesh_utils

    uv_layer = bm.loops.layers.uv.active
    if not uv_layer:
        return False

    islands = bpy_extras.bmesh_utils.bmesh_linked_uv_islands(bm, uv_layer)
    changed = False
    for island in islands:
        if is_island_uv_selected(island, uv_layer, method == 'EDGE'):
            if align_uv_rotation_island(bm, uv_layer, island, method, axis, aspect_y):
                changed = True
    return changed


def get_aspect_y(context):
    area = context.area
    if not area:
        return 1.0
    space_data = context.area.spaces.active
    if not space_data:
        return 1.0
    if not space_data.image:
        return 1.0
    image_width = space_data.image.size[0]
    image_height = space_data.image.size[1]
    if image_height:
        return image_width / image_height
    return 1.0


def align_uv_rotation(context, method, axis, correct_aspect):
    import bmesh

    aspect_y = 1.0
    if correct_aspect:
        aspect_y = get_aspect_y(context)

    ob_list = context.objects_in_mode_unique_data
    for ob in ob_list:
        bm = bmesh.from_edit_mesh(ob.data)
        if bm.loops.layers.uv:
            if align_uv_rotation_bmesh(bm, method, axis, aspect_y):
                bmesh.update_edit_mesh(ob.data)

    return {'FINISHED'}


class AlignUVRotation(Operator):
    """Align the UV island's rotation"""
    bl_idname = "uv.align_rotation"
    bl_label = "Align Rotation"
    bl_options = {'REGISTER', 'UNDO'}

    method: EnumProperty(
        name="Method", description="Method to calculate rotation angle",
        items=(
            ('AUTO', "Auto", "Align from all edges"),
            ('EDGE', "Edge", "Only selected edges"),
            ('GEOMETRY', "Geometry", "Align to Geometry axis"),
        ),
    )

    axis: EnumProperty(
        name="Axis", description="Axis to align to",
        items=(
            ('X', "X", "X axis"),
            ('Y', "Y", "Y axis"),
            ('Z', "Z", "Z axis"),
        ),
    )

    correct_aspect: BoolProperty(
        name="Correct Aspect",
        description="Take image aspect ratio into account",
        default=False,
    )

    def execute(self, context):
        return align_uv_rotation(context, self.method, self.axis, self.correct_aspect)

    def draw(self, _context):
        layout = self.layout
        layout.prop(self, "method")
        if self.method == 'GEOMETRY':
            layout.prop(self, "axis")
        layout.prop(self, "correct_aspect")

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_MESH'


# ------------------------------------------------------------------------------
# Randomize UV Operator

def get_random_transform(transform_params, entropy):
    from random import uniform
    from random import seed as random_seed

    (seed, loc, rot, scale, scale_even) = transform_params

    # First, seed the RNG.
    random_seed(seed + entropy)

    # Next, call uniform a known number of times.
    offset_u = uniform(0.0, 1.0)
    offset_v = uniform(0.0, 1.0)
    angle = uniform(0.0, 1.0)
    scale_u = uniform(0.0, 1.0)
    scale_v = uniform(0.0, 1.0)

    # Apply the transform_params.
    if loc:
        offset_u *= loc[0]
        offset_v *= loc[1]
    else:
        offset_u = 0.0
        offset_v = 0.0

    if rot:
        angle *= rot
    else:
        angle = 0.0

    if scale:
        scale_u = scale_u * (2.0 * scale[0] - 2.0) + 2.0 - scale[0]
        scale_v = scale_v * (2.0 * scale[1] - 2.0) + 2.0 - scale[1]
    else:
        scale_u = 1.0
        scale_v = 1.0

    if scale_even:
        scale_v = scale_u

    # Results in homogenous co-ordinates.
    return [[scale_u * math.cos(angle), -scale_v * math.sin(angle), offset_u],
            [scale_u * math.sin(angle), scale_v * math.cos(angle), offset_v]]


def randomize_uv_transform_island(bm, uv_layer, faces, transform_params):
    # Ensure consistent random values for island, regardless of selection etc.
    entropy = min(f.index for f in faces)

    transform = get_random_transform(transform_params, entropy)

    # Find bounding box center.
    mid_u, mid_v = island_uv_bounds_center(faces, uv_layer)

    del_u = transform[0][2] + mid_u - transform[0][0] * mid_u - transform[0][1] * mid_v
    del_v = transform[1][2] + mid_v - transform[1][0] * mid_u - transform[1][1] * mid_v

    # Apply transform.
    for face in faces:
        for loop in face.loops:
            pre_uv = loop[uv_layer].uv
            u = transform[0][0] * pre_uv[0] + transform[0][1] * pre_uv[1] + del_u
            v = transform[1][0] * pre_uv[0] + transform[1][1] * pre_uv[1] + del_v
            loop[uv_layer].uv = (u, v)


def randomize_uv_transform_bmesh(mesh, bm, transform_params):
    import bpy_extras.bmesh_utils
    uv_layer = bm.loops.layers.uv.verify()
    islands = bpy_extras.bmesh_utils.bmesh_linked_uv_islands(bm, uv_layer)
    for island in islands:
        if is_island_uv_selected(island, uv_layer, False):
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


class RandomizeUVTransform(Operator):
    """Randomize the UV island's location, rotation, and scale"""
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
        description="Maximum distance the objects can spread over each axis",
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
        min=-2.0 * math.pi,
        max=2.0 * math.pi,
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

        loc = [0.0, 0.0] if not self.use_loc else self.loc
        rot = 0.0 if not self.use_rot else self.rot
        scale = None if not self.use_scale else self.scale
        scale_even = self.scale_even

        transformParams = [seed, loc, rot, scale, scale_even]
        return randomize_uv_transform(context, transformParams)


classes = (
    AlignUVRotation,
    RandomizeUVTransform,
)
