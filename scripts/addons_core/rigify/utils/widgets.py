# SPDX-FileCopyrightText: 2019-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
import math
import inspect
import functools

from typing import Optional, Callable, TYPE_CHECKING
from bpy.types import Mesh, Object, UILayout, WindowManager
from mathutils import Matrix, Vector, Euler
from itertools import count

from .errors import MetarigError
from .collections import ensure_collection
from .misc import ArmatureObject, MeshObject, AnyVector, verify_mesh_obj, IdPropSequence
from .naming import change_name_side, get_name_side, Side

if TYPE_CHECKING:
    from .. import RigifyName


WGT_PREFIX = "WGT-"  # Prefix for widget objects
WGT_GROUP_PREFIX = "WGTS_"  # noqa; Prefix for the widget collection


##############################################
# Widget creation
##############################################

def obj_to_bone(obj: Object, rig: ArmatureObject, bone_name: str,
                bone_transform_name: Optional[str] = None):
    """ Places an object at the location/rotation/scale of the given bone.
    """
    if bpy.context.mode == 'EDIT_ARMATURE':
        raise MetarigError("obj_to_bone(): does not work while in edit mode")

    bone = rig.pose.bones[bone_name]

    loc = bone.custom_shape_translation
    rot = bone.custom_shape_rotation_euler
    scale = Vector(bone.custom_shape_scale_xyz)

    if bone.use_custom_shape_bone_size:
        scale *= bone.length

    if bone_transform_name is not None:
        bone = rig.pose.bones[bone_transform_name]
    elif bone.custom_shape_transform:
        bone = bone.custom_shape_transform

    shape_mat = Matrix.LocRotScale(loc, Euler(rot), scale)

    obj.rotation_mode = 'XYZ'
    obj.matrix_basis = rig.matrix_world @ bone.bone.matrix_local @ shape_mat


def create_widget(rig: ArmatureObject, bone_name: str,
                  bone_transform_name: Optional[str] = None, *,
                  widget_name: Optional[str] = None,
                  widget_force_new=False, subsurf=0) -> Optional[MeshObject]:
    """
    Creates an empty widget object for a bone, and returns the object.
    If the object already existed, returns None.
    """
    assert rig.mode != 'EDIT'

    from ..base_generate import BaseGenerator

    scene = bpy.context.scene
    bone = rig.pose.bones[bone_name]

    # Access the current generator instance when generating (ugh, globals)
    generator = BaseGenerator.instance

    if generator:
        collection = generator.widget_collection
    else:
        collection = ensure_collection(bpy.context, WGT_GROUP_PREFIX + rig.name, hidden=True)

    use_mirror = generator and generator.use_mirror_widgets
    bone_mid_name = change_name_side(bone_name, Side.MIDDLE) if use_mirror else bone_name

    obj_name = widget_name or WGT_PREFIX + rig.name + '_' + bone_name
    reuse_mesh = None

    obj: Optional[MeshObject]

    # Check if it already exists in the scene
    if not widget_force_new:
        obj = None

        if generator:
            # Check if the widget was already generated
            if bone_name in generator.new_widget_table:
                return None

            # If re-generating, check widgets used by the previous rig
            obj = generator.old_widget_table.get(bone_name)

        if not obj:
            # Search the scene by name
            obj = scene.objects.get(obj_name)
            if obj and obj.library:
                # Second brute force try if the first result is linked
                local_objs = [obj for obj in scene.objects
                              if obj.name == obj_name and not obj.library]
                obj = local_objs[0] if local_objs else None

        if obj:
            # Record the generated widget
            if generator:
                generator.new_widget_table[bone_name] = obj

            # Re-add to the collection if not there for some reason
            if obj.name not in collection.objects:
                collection.objects.link(obj)

            # Flip scale for originally mirrored widgets
            if obj.scale.x < 0 < bone.custom_shape_scale_xyz.x:
                bone.custom_shape_scale_xyz.x *= -1

            # Move object to bone position, in case it changed
            obj_to_bone(obj, rig, bone_name, bone_transform_name)

            return None

        # Create a linked duplicate of the widget assigned in the metarig
        reuse_widget = rig.pose.bones[bone_name].custom_shape
        if reuse_widget:
            subsurf = 0
            reuse_mesh = reuse_widget.data

        # Create a linked duplicate with the mirror widget
        if not reuse_mesh and use_mirror and bone_mid_name != bone_name:
            reuse_mesh = generator.widget_mirror_mesh.get(bone_mid_name)

    # Create an empty mesh datablock if not linking
    if reuse_mesh:
        mesh = reuse_mesh

    elif use_mirror and bone_mid_name != bone_name:
        # When mirroring, untag side from mesh name, and remember it
        mesh = bpy.data.meshes.new(change_name_side(obj_name, Side.MIDDLE))

        generator.widget_mirror_mesh[bone_mid_name] = mesh

    else:
        mesh = bpy.data.meshes.new(obj_name)

    # Create the object
    obj = verify_mesh_obj(bpy.data.objects.new(obj_name, mesh))
    collection.objects.link(obj)

    # Add the subdivision surface modifier
    if subsurf > 0:
        mod = obj.modifiers.new("subsurf", 'SUBSURF')
        mod.levels = subsurf

    # Record the generated widget
    if generator:
        generator.new_widget_table[bone_name] = obj

    # Flip scale for right side if mirroring widgets
    if use_mirror and get_name_side(bone_name) == Side.RIGHT:
        if bone.custom_shape_scale_xyz.x > 0:
            bone.custom_shape_scale_xyz.x *= -1

    # Move object to bone position and set layers
    obj_to_bone(obj, rig, bone_name, bone_transform_name)

    if reuse_mesh:
        return None

    return obj


##############################################
# Widget choice dropdown
##############################################

_registered_widgets = {}


def _get_valid_args(callback, skip):
    spec = inspect.getfullargspec(callback)
    return set(spec.args[skip:] + spec.kwonlyargs)


def register_widget(name: str, callback, **default_args):
    unwrapped = inspect.unwrap(callback)
    if unwrapped != callback:
        valid_args = _get_valid_args(unwrapped, 1)
    else:
        valid_args = _get_valid_args(callback, 2)

    _registered_widgets[name] = (callback, valid_args, default_args)


def get_rigify_widgets(id_store: WindowManager) -> IdPropSequence['RigifyName']:
    return id_store.rigify_widgets  # noqa


def layout_widget_dropdown(layout: UILayout, props, prop_name: str, **kwargs):
    """Create a UI dropdown to select a widget from the known list."""

    id_store = bpy.context.window_manager
    rigify_widgets = get_rigify_widgets(id_store)

    rigify_widgets.clear()

    for name in sorted(_registered_widgets):
        item = rigify_widgets.add()
        item.name = name

    layout.prop_search(props, prop_name, id_store, "rigify_widgets", **kwargs)


def create_registered_widget(obj: ArmatureObject, bone_name: str, widget_id: str, **kwargs):
    try:
        callback, valid_args, default_args = _registered_widgets[widget_id]
    except KeyError:
        raise MetarigError("Unknown widget name: " + widget_id)

    # Convert between radius and size
    if kwargs.get('size') and 'size' not in valid_args:
        if 'radius' in valid_args and not kwargs.get('radius'):
            kwargs['radius'] = kwargs['size'] / 2

    elif kwargs.get('radius') and 'radius' not in valid_args:
        if 'size' in valid_args and not kwargs.get('size'):
            kwargs['size'] = kwargs['radius'] * 2

    args = {**default_args, **kwargs}

    return callback(obj, bone_name, **{k: v for k, v in args.items() if k in valid_args})


##############################################
# Widget geometry
##############################################

class GeometryData:
    verts: list[AnyVector]
    edges: list[tuple[int, int]]
    faces: list[tuple[int, ...]]

    def __init__(self):
        self.verts = []
        self.edges = []
        self.faces = []


def widget_generator(generate_func=None, *, register=None, subsurf=0) -> Callable:
    """
    Decorator that encapsulates a call to create_widget, and only requires
    the actual function to fill the provided vertex and edge lists.

    Accepts parameters of create_widget, plus any keyword arguments the
    wrapped function has.
    """
    if generate_func is None:
        return functools.partial(widget_generator, register=register, subsurf=subsurf)

    @functools.wraps(generate_func)
    def wrapper(rig: ArmatureObject, bone_name: str, bone_transform_name=None,
                widget_name=None, widget_force_new=False, **kwargs):
        obj = create_widget(rig, bone_name, bone_transform_name,
                            widget_name=widget_name, widget_force_new=widget_force_new,
                            subsurf=subsurf)
        if obj is not None:
            geom = GeometryData()

            generate_func(geom, **kwargs)

            mesh: Mesh = obj.data
            mesh.from_pydata(geom.verts, geom.edges, geom.faces)
            mesh.update()

            return obj
        else:
            return None

    if register:
        register_widget(register, wrapper)

    return wrapper


def generate_lines_geometry(geom: GeometryData,
                            points: list[AnyVector], *,
                            matrix: Optional[Matrix] = None, closed_loop=False):
    """
    Generates a polyline using given points, optionally closing the loop.
    """
    assert len(points) >= 2

    base = len(geom.verts)

    for i, raw_point in enumerate(points):
        point = Vector(raw_point).to_3d()

        if matrix:
            point = matrix @ point

        geom.verts.append(point)

        if i > 0:
            geom.edges.append((base + i - 1, base + i))

    if closed_loop:
        geom.edges.append((len(geom.verts) - 1, base))


def generate_circle_geometry(geom: GeometryData, center: AnyVector, radius: float, *,
                             matrix: Optional[Matrix] = None,
                             angle_range: Optional[tuple[float, float]] = None,
                             steps=24, radius_x: Optional[float] = None, depth_x=0):
    """
    Generates a circle, adding vertices and edges to the lists.
    center, radius: parameters of the circle
    matrix: transformation matrix (by default the circle is in the XY plane)
    angle_range: a pair of angles to generate an arc of the circle
    steps: number of edges to cover the whole circle (reduced for arcs)
    """
    assert steps >= 3

    start = 0
    delta = math.pi * 2 / steps

    if angle_range:
        start, end = angle_range
        if start == end:
            steps = 1
        else:
            steps = max(3, math.ceil(abs(end - start) / delta) + 1)
            delta = (end - start) / (steps - 1)

    if radius_x is None:
        radius_x = radius

    center = Vector(center).to_3d()  # allow 2d center
    points = []

    for i in range(steps):
        angle = start + delta * i
        x = math.cos(angle)
        y = math.sin(angle)
        points.append(center + Vector((x * radius_x, y * radius, x * x * depth_x)))

    generate_lines_geometry(geom, points, matrix=matrix, closed_loop=not angle_range)


def generate_circle_hull_geometry(geom: GeometryData, points: list[AnyVector],
                                  radius: float, gap: float, *,
                                  matrix: Optional[Matrix] = None, steps=24):
    """
    Given a list of 2D points forming a convex hull, generate a contour around
    it, with each point being circumscribed with a circle arc of given radius,
    and keeping the given distance gap from the lines connecting the circles.
    """
    assert radius >= gap

    if len(points) <= 1:
        if points:
            generate_circle_geometry(
                geom, points[0], radius,
                matrix=matrix, steps=steps
            )
        return

    base = len(geom.verts)
    points_ex = [points[-1], *points, points[0]]
    angle_gap = math.asin(gap / radius)

    for i, pt_prev, pt_cur, pt_next in zip(count(0), points_ex[0:], points_ex[1:], points_ex[2:]):
        vec_prev = pt_prev - pt_cur
        vec_next = pt_next - pt_cur

        # Compute bearings to adjacent points
        angle_prev = math.atan2(vec_prev.y, vec_prev.x)
        angle_next = math.atan2(vec_next.y, vec_next.x)
        if angle_next <= angle_prev:
            angle_next += math.pi * 2

        # Adjust gap for circles that are too close
        angle_prev += max(angle_gap, math.acos(min(1, vec_prev.length/radius/2)))
        angle_next -= max(angle_gap, math.acos(min(1, vec_next.length/radius/2)))

        if angle_next > angle_prev:
            if len(geom.verts) > base:
                geom.edges.append((len(geom.verts)-1, len(geom.verts)))

            generate_circle_geometry(
                geom, pt_cur, radius, angle_range=(angle_prev, angle_next),
                matrix=matrix, steps=steps
            )

    if len(geom.verts) > base:
        geom.edges.append((len(geom.verts)-1, base))


def create_circle_polygon(number_verts: int, axis: str, radius=1.0, head_tail=0.0):
    """ Creates a basic circle around of an axis selected.
        number_verts: number of vertices of the polygon
        axis: axis normal to the circle
        radius: the radius of the circle
        head_tail: where along the length of the bone the circle is (0.0=head, 1.0=tail)
    """
    verts = []
    edges = []
    angle = 2 * math.pi / number_verts
    i = 0

    assert(axis in 'XYZ')

    while i < number_verts:
        a = math.cos(i * angle)
        b = math.sin(i * angle)

        if axis == 'X':
            verts.append((head_tail, a * radius, b * radius))
        elif axis == 'Y':
            verts.append((a * radius, head_tail, b * radius))
        elif axis == 'Z':
            verts.append((a * radius, b * radius, head_tail))

        if i < (number_verts - 1):
            edges.append((i, i + 1))

        i += 1

    edges.append((0, number_verts - 1))

    return verts, edges


##############################################
# Widget transformation
##############################################

def adjust_widget_axis(obj: Object, axis='y', offset=0.0):
    mesh = obj.data
    assert isinstance(mesh, Mesh)

    if axis[0] == '-':
        s = -1.0
        axis = axis[1]
    else:
        s = 1.0

    trans_matrix = Matrix.Translation((0.0, offset, 0.0))
    rot_matrix = Matrix.Diagonal((1.0, s, 1.0, 1.0))

    if axis == "x":
        rot_matrix = Matrix.Rotation(-s*math.pi/2, 4, 'Z')
        trans_matrix = Matrix.Translation((offset, 0.0, 0.0))

    elif axis == "z":
        rot_matrix = Matrix.Rotation(s*math.pi/2, 4, 'X')
        trans_matrix = Matrix.Translation((0.0, 0.0, offset))

    matrix = trans_matrix @ rot_matrix

    for vert in mesh.vertices:
        vert.co = matrix @ vert.co


def adjust_widget_transform_mesh(obj: Optional[Object], matrix: Matrix,
                                 local: bool | None = None):
    """Adjust the generated widget by applying a correction matrix to the mesh.
       If local is false, the matrix is in world space.
       If local is True, it's in the local space of the widget.
       If local is a bone, it's in the local space of the bone.
    """
    if obj:
        mesh = obj.data
        assert isinstance(mesh, Mesh)

        if local is not True:
            if local:
                assert isinstance(local, bpy.types.PoseBone)
                bone_mat = local.id_data.matrix_world @ local.bone.matrix_local
                matrix = bone_mat @ matrix @ bone_mat.inverted()

            obj_mat = obj.matrix_basis
            matrix = obj_mat.inverted() @ matrix @ obj_mat

        mesh.transform(matrix)


def write_widget(obj: Object, name='thing', use_size=True):
    """ Write a mesh object as a python script for widget use.
    """
    script = ""
    script += "@widget_generator\n"
    script += "def create_"+name+"_widget(geom"
    if use_size:
        script += ", *, size=1.0"
    script += "):\n"

    # Vertices
    szs = "*size" if use_size else ""
    width = 2 if use_size else 3

    mesh = obj.data
    assert isinstance(mesh, Mesh)

    script += "    geom.verts = ["
    for i, v in enumerate(mesh.vertices):
        script += "({:g}{}, {:g}{}, {:g}{}),".format(v.co[0], szs, v.co[1], szs, v.co[2], szs)
        script += "\n                  " if i % width == (width - 1) else " "
    script += "]\n"

    # Edges
    script += "    geom.edges = ["
    for i, e in enumerate(mesh.edges):
        script += "(" + str(e.vertices[0]) + ", " + str(e.vertices[1]) + "),"
        script += "\n                  " if i % 10 == 9 else " "
    script += "]\n"

    # Faces
    if mesh.polygons:
        script += "    geom.faces = ["
        for i, f in enumerate(mesh.polygons):
            script += "(" + ", ".join(str(v) for v in f.vertices) + "),"
            script += "\n                  " if i % 10 == 9 else " "
        script += "]\n"

    return script
