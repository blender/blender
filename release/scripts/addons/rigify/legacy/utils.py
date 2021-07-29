#====================== BEGIN GPL LICENSE BLOCK ======================
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
#======================= END GPL LICENSE BLOCK ========================

# <pep8 compliant>

import bpy
import importlib
import importlib
import math
import random
import time
from mathutils import Vector, Matrix
from rna_prop_ui import rna_idprop_ui_prop_get

RIG_DIR = "rigs"  # Name of the directory where rig types are kept
METARIG_DIR = "metarigs"  # Name of the directory where metarigs are kept

ORG_PREFIX = "ORG-"  # Prefix of original bones.
MCH_PREFIX = "MCH-"  # Prefix of mechanism bones.
DEF_PREFIX = "DEF-"  # Prefix of deformation bones.
WGT_PREFIX = "WGT-"  # Prefix for widget objects
ROOT_NAME = "root"   # Name of the root bone.

WGT_LAYERS = [x == 19 for x in range(0, 20)]  # Widgets go on the last scene layer.

MODULE_NAME = "rigify"  # Windows/Mac blender is weird, so __package__ doesn't work


#=======================================================================
# Error handling
#=======================================================================
class MetarigError(Exception):
    """ Exception raised for errors.
    """
    def __init__(self, message):
        self.message = message

    def __str__(self):
        return repr(self.message)


#=======================================================================
# Name manipulation
#=======================================================================
def org_name(name):
    """ Returns the name with ORG_PREFIX stripped from it.
    """
    if name.startswith(ORG_PREFIX):
        return name[len(ORG_PREFIX):]
    else:
        return name


def strip_org(name):
    """ Returns the name with ORG_PREFIX stripped from it.
    """
    if name.startswith(ORG_PREFIX):
        return name[len(ORG_PREFIX):]
    else:
        return name
org_name = strip_org


def org(name):
    """ Prepends the ORG_PREFIX to a name if it doesn't already have
        it, and returns it.
    """
    if name.startswith(ORG_PREFIX):
        return name
    else:
        return ORG_PREFIX + name
make_original_name = org


def mch(name):
    """ Prepends the MCH_PREFIX to a name if it doesn't already have
        it, and returns it.
    """
    if name.startswith(MCH_PREFIX):
        return name
    else:
        return MCH_PREFIX + name
make_mechanism_name = mch


def deformer(name):
    """ Prepends the DEF_PREFIX to a name if it doesn't already have
        it, and returns it.
    """
    if name.startswith(DEF_PREFIX):
        return name
    else:
        return DEF_PREFIX + name
make_deformer_name = deformer


def insert_before_lr(name, text):
    if name[-1] in ['l', 'L', 'r', 'R'] and name[-2] in ['.', '-', '_']:
        return name[:-2] + text + name[-2:]
    else:
        return name + text


#=======================
# Bone manipulation
#=======================
def new_bone(obj, bone_name):
    """ Adds a new bone to the given armature object.
        Returns the resulting bone's name.
    """
    if obj == bpy.context.active_object and bpy.context.mode == 'EDIT_ARMATURE':
        edit_bone = obj.data.edit_bones.new(bone_name)
        name = edit_bone.name
        edit_bone.head = (0, 0, 0)
        edit_bone.tail = (0, 1, 0)
        edit_bone.roll = 0
        bpy.ops.object.mode_set(mode='OBJECT')
        bpy.ops.object.mode_set(mode='EDIT')
        return name
    else:
        raise MetarigError("Can't add new bone '%s' outside of edit mode" % bone_name)

def copy_bone_simple(obj, bone_name, assign_name=''):
    """ Makes a copy of the given bone in the given armature object.
        but only copies head, tail positions and roll. Does not
        address parenting either.
    """
    #if bone_name not in obj.data.bones:
    if bone_name not in obj.data.edit_bones:
        raise MetarigError("copy_bone(): bone '%s' not found, cannot copy it" % bone_name)

    if obj == bpy.context.active_object and bpy.context.mode == 'EDIT_ARMATURE':
        if assign_name == '':
            assign_name = bone_name
        # Copy the edit bone
        edit_bone_1 = obj.data.edit_bones[bone_name]
        edit_bone_2 = obj.data.edit_bones.new(assign_name)
        bone_name_1 = bone_name
        bone_name_2 = edit_bone_2.name

        # Copy edit bone attributes
        edit_bone_2.layers = list(edit_bone_1.layers)

        edit_bone_2.head = Vector(edit_bone_1.head)
        edit_bone_2.tail = Vector(edit_bone_1.tail)
        edit_bone_2.roll = edit_bone_1.roll

        return bone_name_2
    else:
        raise MetarigError("Cannot copy bones outside of edit mode")

def copy_bone(obj, bone_name, assign_name=''):
    """ Makes a copy of the given bone in the given armature object.
        Returns the resulting bone's name.
    """
    #if bone_name not in obj.data.bones:
    if bone_name not in obj.data.edit_bones:
        raise MetarigError("copy_bone(): bone '%s' not found, cannot copy it" % bone_name)

    if obj == bpy.context.active_object and bpy.context.mode == 'EDIT_ARMATURE':
        if assign_name == '':
            assign_name = bone_name
        # Copy the edit bone
        edit_bone_1 = obj.data.edit_bones[bone_name]
        edit_bone_2 = obj.data.edit_bones.new(assign_name)
        bone_name_1 = bone_name
        bone_name_2 = edit_bone_2.name

        edit_bone_2.parent = edit_bone_1.parent
        edit_bone_2.use_connect = edit_bone_1.use_connect

        # Copy edit bone attributes
        edit_bone_2.layers = list(edit_bone_1.layers)

        edit_bone_2.head = Vector(edit_bone_1.head)
        edit_bone_2.tail = Vector(edit_bone_1.tail)
        edit_bone_2.roll = edit_bone_1.roll

        edit_bone_2.use_inherit_rotation = edit_bone_1.use_inherit_rotation
        edit_bone_2.use_inherit_scale = edit_bone_1.use_inherit_scale
        edit_bone_2.use_local_location = edit_bone_1.use_local_location

        edit_bone_2.use_deform = edit_bone_1.use_deform
        edit_bone_2.bbone_segments = edit_bone_1.bbone_segments
        edit_bone_2.bbone_in = edit_bone_1.bbone_in
        edit_bone_2.bbone_out = edit_bone_1.bbone_out

        bpy.ops.object.mode_set(mode='OBJECT')

        # Get the pose bones
        pose_bone_1 = obj.pose.bones[bone_name_1]
        pose_bone_2 = obj.pose.bones[bone_name_2]

        # Copy pose bone attributes
        pose_bone_2.rotation_mode = pose_bone_1.rotation_mode
        pose_bone_2.rotation_axis_angle = tuple(pose_bone_1.rotation_axis_angle)
        pose_bone_2.rotation_euler = tuple(pose_bone_1.rotation_euler)
        pose_bone_2.rotation_quaternion = tuple(pose_bone_1.rotation_quaternion)

        pose_bone_2.lock_location = tuple(pose_bone_1.lock_location)
        pose_bone_2.lock_scale = tuple(pose_bone_1.lock_scale)
        pose_bone_2.lock_rotation = tuple(pose_bone_1.lock_rotation)
        pose_bone_2.lock_rotation_w = pose_bone_1.lock_rotation_w
        pose_bone_2.lock_rotations_4d = pose_bone_1.lock_rotations_4d

        # Copy custom properties
        for key in pose_bone_1.keys():
            if key != "_RNA_UI" \
            and key != "rigify_parameters" \
            and key != "rigify_type":
                prop1 = rna_idprop_ui_prop_get(pose_bone_1, key, create=False)
                prop2 = rna_idprop_ui_prop_get(pose_bone_2, key, create=True)
                pose_bone_2[key] = pose_bone_1[key]
                for key in prop1.keys():
                    prop2[key] = prop1[key]

        bpy.ops.object.mode_set(mode='EDIT')

        return bone_name_2
    else:
        raise MetarigError("Cannot copy bones outside of edit mode")


def flip_bone(obj, bone_name):
    """ Flips an edit bone.
    """
    if bone_name not in obj.data.bones:
        raise MetarigError("flip_bone(): bone '%s' not found, cannot copy it" % bone_name)

    if obj == bpy.context.active_object and bpy.context.mode == 'EDIT_ARMATURE':
        bone = obj.data.edit_bones[bone_name]
        head = Vector(bone.head)
        tail = Vector(bone.tail)
        bone.tail = head + tail
        bone.head = tail
        bone.tail = head
    else:
        raise MetarigError("Cannot flip bones outside of edit mode")


def put_bone(obj, bone_name, pos):
    """ Places a bone at the given position.
    """
    if bone_name not in obj.data.bones:
        raise MetarigError("put_bone(): bone '%s' not found, cannot move it" % bone_name)

    if obj == bpy.context.active_object and bpy.context.mode == 'EDIT_ARMATURE':
        bone = obj.data.edit_bones[bone_name]

        delta = pos - bone.head
        bone.translate(delta)
    else:
        raise MetarigError("Cannot 'put' bones outside of edit mode")


def make_nonscaling_child(obj, bone_name, location, child_name_postfix=""):
    """ Takes the named bone and creates a non-scaling child of it at
        the given location.  The returned bone (returned by name) is not
        a true child, but behaves like one sans inheriting scaling.

        It is intended as an intermediate construction to prevent rig types
        from scaling with their parents.  The named bone is assumed to be
        an ORG bone.
    """
    if bone_name not in obj.data.bones:
        raise MetarigError("make_nonscaling_child(): bone '%s' not found, cannot copy it" % bone_name)

    if obj == bpy.context.active_object and bpy.context.mode == 'EDIT_ARMATURE':
        # Create desired names for bones
        name1 = make_mechanism_name(strip_org(insert_before_lr(bone_name, child_name_postfix + "_ns_ch")))
        name2 = make_mechanism_name(strip_org(insert_before_lr(bone_name, child_name_postfix + "_ns_intr")))

        # Create bones
        child = copy_bone(obj, bone_name, name1)
        intermediate_parent = copy_bone(obj, bone_name, name2)

        # Get edit bones
        eb = obj.data.edit_bones
        child_e = eb[child]
        intrpar_e = eb[intermediate_parent]

        # Parenting
        child_e.use_connect = False
        child_e.parent = None

        intrpar_e.use_connect = False
        intrpar_e.parent = eb[bone_name]

        # Positioning
        child_e.length *= 0.5
        intrpar_e.length *= 0.25

        put_bone(obj, child, location)
        put_bone(obj, intermediate_parent, location)

        # Object mode
        bpy.ops.object.mode_set(mode='OBJECT')
        pb = obj.pose.bones

        # Add constraints
        con = pb[child].constraints.new('COPY_LOCATION')
        con.name = "parent_loc"
        con.target = obj
        con.subtarget = intermediate_parent

        con = pb[child].constraints.new('COPY_ROTATION')
        con.name = "parent_loc"
        con.target = obj
        con.subtarget = intermediate_parent

        bpy.ops.object.mode_set(mode='EDIT')

        return child
    else:
        raise MetarigError("Cannot make nonscaling child outside of edit mode")


#=============================================
# Widget creation
#=============================================

def obj_to_bone(obj, rig, bone_name):
    """ Places an object at the location/rotation/scale of the given bone.
    """
    if bpy.context.mode == 'EDIT_ARMATURE':
        raise MetarigError("obj_to_bone(): does not work while in edit mode")

    bone = rig.data.bones[bone_name]

    mat = rig.matrix_world * bone.matrix_local

    obj.location = mat.to_translation()

    obj.rotation_mode = 'XYZ'
    obj.rotation_euler = mat.to_euler()

    scl = mat.to_scale()
    scl_avg = (scl[0] + scl[1] + scl[2]) / 3
    obj.scale = (bone.length * scl_avg), (bone.length * scl_avg), (bone.length * scl_avg)


def create_circle_polygon(number_verts, axis, radius=1.0, head_tail=0.0):
    """ Creates a basic circle around of an axis selected.
        number_verts: number of vertices of the poligon
        axis: axis normal to the circle
        radius: the radius of the circle
        head_tail: where along the length of the bone the circle is (0.0=head, 1.0=tail)
    """
    verts = []
    edges = []
    angle = 2 * math.pi / number_verts
    i = 0

    assert(axis in 'XYZ')

    while i < (number_verts):
        a = math.cos(i * angle)
        b = math.sin(i * angle)

        if axis == 'X':
            verts.append((head_tail, a * radius, b * radius))
        elif axis == 'Y':
            verts.append((a * radius, head_tail, b * radius))
        elif axis == 'Z':
            verts.append((a * radius, b * radius, head_tail))

        if i < (number_verts - 1):
            edges.append((i , i + 1))

        i += 1

    edges.append((0, number_verts - 1))

    return verts, edges


def create_widget(rig, bone_name, bone_transform_name=None):
    """ Creates an empty widget object for a bone, and returns the object.
    """
    if bone_transform_name is None:
        bone_transform_name = bone_name

    obj_name = WGT_PREFIX + bone_name
    scene = bpy.context.scene

    # Check if it already exists in the scene
    if obj_name in scene.objects:
        # Move object to bone position, in case it changed
        obj = scene.objects[obj_name]
        obj_to_bone(obj, rig, bone_transform_name)

        return None
    else:
        # Delete object if it exists in blend data but not scene data.
        # This is necessary so we can then create the object without
        # name conflicts.
        if obj_name in bpy.data.objects:
            bpy.data.objects[obj_name].user_clear()
            bpy.data.objects.remove(bpy.data.objects[obj_name])

        # Create mesh object
        mesh = bpy.data.meshes.new(obj_name)
        obj = bpy.data.objects.new(obj_name, mesh)
        scene.objects.link(obj)

        # Move object to bone position and set layers
        obj_to_bone(obj, rig, bone_transform_name)
        obj.layers = WGT_LAYERS

        return obj


# Common Widgets

def create_line_widget(rig, bone_name, bone_transform_name=None):
    """ Creates a basic line widget, a line that spans the length of the bone.
    """
    obj = create_widget(rig, bone_name, bone_transform_name)
    if obj is not None:
        mesh = obj.data
        mesh.from_pydata([(0, 0, 0), (0, 1, 0)], [(0, 1)], [])
        mesh.update()


def create_circle_widget(rig, bone_name, radius=1.0, head_tail=0.0, with_line=False, bone_transform_name=None):
    """ Creates a basic circle widget, a circle around the y-axis.
        radius: the radius of the circle
        head_tail: where along the length of the bone the circle is (0.0=head, 1.0=tail)
    """
    obj = create_widget(rig, bone_name, bone_transform_name)
    if obj is not None:
        verts, edges = create_circle_polygon(32, 'Y', radius, head_tail)

        if with_line:
            edges.append((8, 24))

        mesh = obj.data
        mesh.from_pydata(verts, edges, [])
        mesh.update()
        return obj
    else:
        return None


def create_cube_widget(rig, bone_name, radius=0.5, bone_transform_name=None):
    """ Creates a basic cube widget.
    """
    obj = create_widget(rig, bone_name, bone_transform_name)
    if obj is not None:
        r = radius
        verts = [(r, r, r), (r, -r, r), (-r, -r, r), (-r, r, r), (r, r, -r), (r, -r, -r), (-r, -r, -r), (-r, r, -r)]
        edges = [(0, 1), (1, 2), (2, 3), (3, 0), (4, 5), (5, 6), (6, 7), (7, 4), (0, 4), (1, 5), (2, 6), (3, 7)]
        mesh = obj.data
        mesh.from_pydata(verts, edges, [])
        mesh.update()


def create_sphere_widget(rig, bone_name, bone_transform_name=None):
    """ Creates a basic sphere widget, three pependicular overlapping circles.
    """
    obj = create_widget(rig, bone_name, bone_transform_name)
    if obj is not None:
        verts_x, edges_x = create_circle_polygon(16, 'X', 0.5)
        verts_y, edges_y = create_circle_polygon(16, 'Y', 0.5)
        verts_z, edges_z = create_circle_polygon(16, 'Z', 0.5)

        verts = verts_x + verts_y + verts_z

        edges = [
            (0, 1), (1, 2), (2, 3), (3, 4), (4, 5), (5, 6), (6, 7), (7, 8),
            (8, 9), (9, 10), (10, 11), (11, 12), (12, 13), (13, 14), (14, 15), (0, 15),
            (16, 17), (17, 18), (18, 19), (19, 20), (20, 21), (21, 22), (22, 23), (23, 24),
            (24, 25), (25, 26), (26, 27), (27, 28), (28, 29), (29, 30), (30, 31), (16, 31),
            (32, 33), (33, 34), (34, 35), (35, 36), (36, 37), (37, 38), (38, 39), (39, 40),
            (40, 41), (41, 42), (42, 43), (43, 44), (44, 45), (45, 46), (46, 47), (32, 47),
            ]
        mesh = obj.data
        mesh.from_pydata(verts, edges, [])
        mesh.update()


def create_limb_widget(rig, bone_name, bone_transform_name=None):
    """ Creates a basic limb widget, a line that spans the length of the
        bone, with a circle around the center.
    """
    obj = create_widget(rig, bone_name, bone_transform_name)
    if obj is not None:
        verts, edges = create_circle_polygon(32, "Y", 0.25, 0.5)
        verts.append((0, 0, 0))
        verts.append((0, 1, 0))
        edges.append((32, 33))
        mesh = obj.data
        mesh.from_pydata(verts, edges, [])
        mesh.update()


def create_bone_widget(rig, bone_name, bone_transform_name=None):
    """ Creates a basic bone widget, a simple obolisk-esk shape.
    """
    obj = create_widget(rig, bone_name, bone_transform_name)
    if obj is not None:
        verts = [(0.04, 1.0, -0.04), (0.1, 0.0, -0.1), (-0.1, 0.0, -0.1), (-0.04, 1.0, -0.04), (0.04, 1.0, 0.04), (0.1, 0.0, 0.1), (-0.1, 0.0, 0.1), (-0.04, 1.0, 0.04)]
        edges = [(1, 2), (0, 1), (0, 3), (2, 3), (4, 5), (5, 6), (6, 7), (4, 7), (1, 5), (0, 4), (2, 6), (3, 7)]
        mesh = obj.data
        mesh.from_pydata(verts, edges, [])
        mesh.update()


def create_compass_widget(rig, bone_name, bone_transform_name=None):
    """ Creates a compass-shaped widget.
    """
    obj = create_widget(rig, bone_name, bone_transform_name)
    if obj is not None:
        verts, edges = create_circle_polygon(32, "Z", 1.0, 0.0)
        i = 0
        for v in verts:
            if i in {0, 8, 16, 24}:
                verts[i] = (v[0] * 1.2, v[1] * 1.2, v[2])
            i += 1

        mesh = obj.data
        mesh.from_pydata(verts, edges, [])
        mesh.update()


def create_root_widget(rig, bone_name, bone_transform_name=None):
    """ Creates a widget for the root bone.
    """
    obj = create_widget(rig, bone_name, bone_transform_name)
    if obj is not None:
        verts = [(0.7071067690849304, 0.7071067690849304, 0.0), (0.7071067690849304, -0.7071067690849304, 0.0), (-0.7071067690849304, 0.7071067690849304, 0.0), (-0.7071067690849304, -0.7071067690849304, 0.0), (0.8314696550369263, 0.5555701851844788, 0.0), (0.8314696550369263, -0.5555701851844788, 0.0), (-0.8314696550369263, 0.5555701851844788, 0.0), (-0.8314696550369263, -0.5555701851844788, 0.0), (0.9238795042037964, 0.3826834261417389, 0.0), (0.9238795042037964, -0.3826834261417389, 0.0), (-0.9238795042037964, 0.3826834261417389, 0.0), (-0.9238795042037964, -0.3826834261417389, 0.0), (0.9807852506637573, 0.19509035348892212, 0.0), (0.9807852506637573, -0.19509035348892212, 0.0), (-0.9807852506637573, 0.19509035348892212, 0.0), (-0.9807852506637573, -0.19509035348892212, 0.0), (0.19509197771549225, 0.9807849526405334, 0.0), (0.19509197771549225, -0.9807849526405334, 0.0), (-0.19509197771549225, 0.9807849526405334, 0.0), (-0.19509197771549225, -0.9807849526405334, 0.0), (0.3826850652694702, 0.9238788485527039, 0.0), (0.3826850652694702, -0.9238788485527039, 0.0), (-0.3826850652694702, 0.9238788485527039, 0.0), (-0.3826850652694702, -0.9238788485527039, 0.0), (0.5555717945098877, 0.8314685821533203, 0.0), (0.5555717945098877, -0.8314685821533203, 0.0), (-0.5555717945098877, 0.8314685821533203, 0.0), (-0.5555717945098877, -0.8314685821533203, 0.0), (0.19509197771549225, 1.2807848453521729, 0.0), (0.19509197771549225, -1.2807848453521729, 0.0), (-0.19509197771549225, 1.2807848453521729, 0.0), (-0.19509197771549225, -1.2807848453521729, 0.0), (1.280785322189331, 0.19509035348892212, 0.0), (1.280785322189331, -0.19509035348892212, 0.0), (-1.280785322189331, 0.19509035348892212, 0.0), (-1.280785322189331, -0.19509035348892212, 0.0), (0.3950919806957245, 1.2807848453521729, 0.0), (0.3950919806957245, -1.2807848453521729, 0.0), (-0.3950919806957245, 1.2807848453521729, 0.0), (-0.3950919806957245, -1.2807848453521729, 0.0), (1.280785322189331, 0.39509034156799316, 0.0), (1.280785322189331, -0.39509034156799316, 0.0), (-1.280785322189331, 0.39509034156799316, 0.0), (-1.280785322189331, -0.39509034156799316, 0.0), (0.0, 1.5807849168777466, 0.0), (0.0, -1.5807849168777466, 0.0), (1.5807852745056152, 0.0, 0.0), (-1.5807852745056152, 0.0, 0.0)]
        edges = [(0, 4), (1, 5), (2, 6), (3, 7), (4, 8), (5, 9), (6, 10), (7, 11), (8, 12), (9, 13), (10, 14), (11, 15), (16, 20), (17, 21), (18, 22), (19, 23), (20, 24), (21, 25), (22, 26), (23, 27), (0, 24), (1, 25), (2, 26), (3, 27), (16, 28), (17, 29), (18, 30), (19, 31), (12, 32), (13, 33), (14, 34), (15, 35), (28, 36), (29, 37), (30, 38), (31, 39), (32, 40), (33, 41), (34, 42), (35, 43), (36, 44), (37, 45), (38, 44), (39, 45), (40, 46), (41, 46), (42, 47), (43, 47)]
        mesh = obj.data
        mesh.from_pydata(verts, edges, [])
        mesh.update()


#=============================================
# Math
#=============================================

def angle_on_plane(plane, vec1, vec2):
    """ Return the angle between two vectors projected onto a plane.
    """
    plane.normalize()
    vec1 = vec1 - (plane * (vec1.dot(plane)))
    vec2 = vec2 - (plane * (vec2.dot(plane)))
    vec1.normalize()
    vec2.normalize()

    # Determine the angle
    angle = math.acos(max(-1.0, min(1.0, vec1.dot(vec2))))

    if angle < 0.00001:  # close enough to zero that sign doesn't matter
        return angle

    # Determine the sign of the angle
    vec3 = vec2.cross(vec1)
    vec3.normalize()
    sign = vec3.dot(plane)
    if sign >= 0:
        sign = 1
    else:
        sign = -1

    return angle * sign


def align_bone_roll(obj, bone1, bone2):
    """ Aligns the roll of two bones.
    """
    bone1_e = obj.data.edit_bones[bone1]
    bone2_e = obj.data.edit_bones[bone2]

    bone1_e.roll = 0.0

    # Get the directions the bones are pointing in, as vectors
    y1 = bone1_e.y_axis
    x1 = bone1_e.x_axis
    y2 = bone2_e.y_axis
    x2 = bone2_e.x_axis

    # Get the shortest axis to rotate bone1 on to point in the same direction as bone2
    axis = y1.cross(y2)
    axis.normalize()

    # Angle to rotate on that shortest axis
    angle = y1.angle(y2)

    # Create rotation matrix to make bone1 point in the same direction as bone2
    rot_mat = Matrix.Rotation(angle, 3, axis)

    # Roll factor
    x3 = rot_mat * x1
    dot = x2 * x3
    if dot > 1.0:
        dot = 1.0
    elif dot < -1.0:
        dot = -1.0
    roll = math.acos(dot)

    # Set the roll
    bone1_e.roll = roll

    # Check if we rolled in the right direction
    x3 = rot_mat * bone1_e.x_axis
    check = x2 * x3

    # If not, reverse
    if check < 0.9999:
        bone1_e.roll = -roll


def align_bone_x_axis(obj, bone, vec):
    """ Rolls the bone to align its x-axis as closely as possible to
        the given vector.
        Must be in edit mode.
    """
    bone_e = obj.data.edit_bones[bone]

    vec = vec.cross(bone_e.y_axis)
    vec.normalize()

    dot = max(-1.0, min(1.0, bone_e.z_axis.dot(vec)))
    angle = math.acos(dot)

    bone_e.roll += angle

    dot1 = bone_e.z_axis.dot(vec)

    bone_e.roll -= angle * 2

    dot2 = bone_e.z_axis.dot(vec)

    if dot1 > dot2:
        bone_e.roll += angle * 2


def align_bone_z_axis(obj, bone, vec):
    """ Rolls the bone to align its z-axis as closely as possible to
        the given vector.
        Must be in edit mode.
    """
    bone_e = obj.data.edit_bones[bone]

    vec = bone_e.y_axis.cross(vec)
    vec.normalize()

    dot = max(-1.0, min(1.0, bone_e.x_axis.dot(vec)))
    angle = math.acos(dot)

    bone_e.roll += angle

    dot1 = bone_e.x_axis.dot(vec)

    bone_e.roll -= angle * 2

    dot2 = bone_e.x_axis.dot(vec)

    if dot1 > dot2:
        bone_e.roll += angle * 2


#=============================================
# Misc
#=============================================

def copy_attributes(a, b):
    keys = dir(a)
    for key in keys:
        if not key.startswith("_") \
        and not key.startswith("error_") \
        and key != "group" \
        and key != "is_valid" \
        and key != "rna_type" \
        and key != "bl_rna":
            try:
                setattr(b, key, getattr(a, key))
            except AttributeError:
                pass


def get_rig_type(rig_type):
    """ Fetches a rig module by name, and returns it.
    """
    name = "rigify.legacy.%s.%s" % (RIG_DIR, rig_type)
    submod = importlib.import_module(name, package=MODULE_NAME)
    importlib.reload(submod)
    return submod


def get_metarig_module(metarig_name):
    """ Fetches a rig module by name, and returns it.
    """
    name = "rigify.legacy.%s.%s" % (METARIG_DIR, metarig_name)
    submod = importlib.import_module(name, package=MODULE_NAME)
    importlib.reload(submod)
    return submod


def connected_children_names(obj, bone_name):
    """ Returns a list of bone names (in order) of the bones that form a single
        connected chain starting with the given bone as a parent.
        If there is a connected branch, the list stops there.
    """
    bone = obj.data.bones[bone_name]
    names = []

    while True:
        connects = 0
        con_name = ""

        for child in bone.children:
            if child.use_connect:
                connects += 1
                con_name = child.name

        if connects == 1:
            names += [con_name]
            bone = obj.data.bones[con_name]
        else:
            break

    return names


def has_connected_children(bone):
    """ Returns true/false whether a bone has connected children or not.
    """
    t = False
    for b in bone.children:
        t = t or b.use_connect
    return t


def get_layers(layers):
    """ Does it's best to exctract a set of layers from any data thrown at it.
    """
    if type(layers) == int:
        return [x == layers for x in range(0, 32)]
    elif type(layers) == str:
        s = layers.split(",")
        l = []
        for i in s:
            try:
                l += [int(float(i))]
            except ValueError:
                pass
        return [x in l for x in range(0, 32)]
    elif type(layers) == tuple or type(layers) == list:
        return [x in layers for x in range(0, 32)]
    else:
        try:
            list(layers)
        except TypeError:
            pass
        else:
            return [x in layers for x in range(0, 32)]


def write_metarig(obj, layers=False, func_name="create"):
    """
    Write a metarig as a python script, this rig is to have all info needed for
    generating the real rig with rigify.
    """
    code = []

    code.append("import bpy\n\n")

    code.append("def %s(obj):" % func_name)
    code.append("    # generated by rigify.utils.write_metarig")
    bpy.ops.object.mode_set(mode='EDIT')
    code.append("    bpy.ops.object.mode_set(mode='EDIT')")
    code.append("    arm = obj.data")

    arm = obj.data

    # Rigify layer layout info
    if layers and len(arm.rigify_layers) > 0:
        code.append("\n    for i in range(" + str(len(arm.rigify_layers)) + "):")
        code.append("        arm.rigify_layers.add()\n")

        for i in range(len(arm.rigify_layers)):
            name = arm.rigify_layers[i].name
            row = arm.rigify_layers[i].row
            code.append('    arm.rigify_layers[' + str(i) + '].name = "' + name + '"')
            code.append('    arm.rigify_layers[' + str(i) + '].row = ' + str(row))

    # write parents first
    bones = [(len(bone.parent_recursive), bone.name) for bone in arm.edit_bones]
    bones.sort(key=lambda item: item[0])
    bones = [item[1] for item in bones]

    code.append("\n    bones = {}\n")

    for bone_name in bones:
        bone = arm.edit_bones[bone_name]
        code.append("    bone = arm.edit_bones.new(%r)" % bone.name)
        code.append("    bone.head[:] = %.4f, %.4f, %.4f" % bone.head.to_tuple(4))
        code.append("    bone.tail[:] = %.4f, %.4f, %.4f" % bone.tail.to_tuple(4))
        code.append("    bone.roll = %.4f" % bone.roll)
        code.append("    bone.use_connect = %s" % str(bone.use_connect))
        if bone.parent:
            code.append("    bone.parent = arm.edit_bones[bones[%r]]" % bone.parent.name)
        code.append("    bones[%r] = bone.name" % bone.name)

    bpy.ops.object.mode_set(mode='OBJECT')
    code.append("")
    code.append("    bpy.ops.object.mode_set(mode='OBJECT')")

    # Rig type and other pose properties
    for bone_name in bones:
        pbone = obj.pose.bones[bone_name]

        code.append("    pbone = obj.pose.bones[bones[%r]]" % bone_name)
        code.append("    pbone.rigify_type = %r" % pbone.rigify_type)
        code.append("    pbone.lock_location = %s" % str(tuple(pbone.lock_location)))
        code.append("    pbone.lock_rotation = %s" % str(tuple(pbone.lock_rotation)))
        code.append("    pbone.lock_rotation_w = %s" % str(pbone.lock_rotation_w))
        code.append("    pbone.lock_scale = %s" % str(tuple(pbone.lock_scale)))
        code.append("    pbone.rotation_mode = %r" % pbone.rotation_mode)
        if layers:
            code.append("    pbone.bone.layers = %s" % str(list(pbone.bone.layers)))
        # Rig type parameters
        for param_name in pbone.rigify_parameters.keys():
            param = getattr(pbone.rigify_parameters, param_name)
            if str(type(param)) == "<class 'bpy_prop_array'>":
                param = list(param)
            if type(param) == str:
                param = '"' + param + '"'
            code.append("    try:")
            code.append("        pbone.rigify_parameters.%s = %s" % (param_name, str(param)))
            code.append("    except AttributeError:")
            code.append("        pass")

    code.append("\n    bpy.ops.object.mode_set(mode='EDIT')")
    code.append("    for bone in arm.edit_bones:")
    code.append("        bone.select = False")
    code.append("        bone.select_head = False")
    code.append("        bone.select_tail = False")

    code.append("    for b in bones:")
    code.append("        bone = arm.edit_bones[bones[b]]")
    code.append("        bone.select = True")
    code.append("        bone.select_head = True")
    code.append("        bone.select_tail = True")
    code.append("        arm.edit_bones.active = bone")

    # Set appropriate layers visible
    if layers:
        # Find what layers have bones on them
        active_layers = []
        for bone_name in bones:
            bone = obj.data.bones[bone_name]
            for i in range(len(bone.layers)):
                if bone.layers[i]:
                    if i not in active_layers:
                        active_layers.append(i)
        active_layers.sort()

        code.append("\n    arm.layers = [(x in " + str(active_layers) + ") for x in range(" + str(len(arm.layers)) + ")]")

    code.append('\nif __name__ == "__main__":')
    code.append("    " + func_name + "(bpy.context.active_object)")

    return "\n".join(code)


def write_widget(obj):
    """ Write a mesh object as a python script for widget use.
    """
    script = ""
    script += "def create_thing_widget(rig, bone_name, size=1.0, bone_transform_name=None):\n"
    script += "    obj = create_widget(rig, bone_name, bone_transform_name)\n"
    script += "    if obj is not None:\n"

    # Vertices
    if len(obj.data.vertices) > 0:
        script += "        verts = ["
        for v in obj.data.vertices:
            script += "(" + str(v.co[0]) + "*size, " + str(v.co[1]) + "*size, " + str(v.co[2]) + "*size), "
        script += "]\n"

    # Edges
    if len(obj.data.edges) > 0:
        script += "        edges = ["
        for e in obj.data.edges:
            script += "(" + str(e.vertices[0]) + ", " + str(e.vertices[1]) + "), "
        script += "]\n"

    # Faces
    if len(obj.data.polygons) > 0:
        script += "        faces = ["
        for f in obj.data.polygons:
            script += "("
            for v in f.vertices:
                script += str(v) + ", "
            script += "), "
        script += "]\n"

    # Build mesh
    script += "\n        mesh = obj.data\n"
    script += "        mesh.from_pydata(verts, edges, faces)\n"
    script += "        mesh.update()\n"
    script += "        mesh.update()\n"
    script += "        return obj\n"
    script += "    else:\n"
    script += "        return None\n"

    return script


def random_id(length=8):
    """ Generates a random alphanumeric id string.
    """
    tlength = int(length / 2)
    rlength = int(length / 2) + int(length % 2)

    chars = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z']
    text = ""
    for i in range(0, rlength):
        text += random.choice(chars)
    text += str(hex(int(time.time())))[2:][-tlength:].rjust(tlength, '0')[::-1]
    return text
