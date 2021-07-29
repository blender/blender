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
import imp
import importlib
import math
import random
import time
import re
import os
from mathutils import Vector, Matrix, Color
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

outdated_types = {"pitchipoy.limbs.super_limb": "limbs.super_limb",
                  "pitchipoy.limbs.super_arm": "limbs.super_limb",
                  "pitchipoy.limbs.super_leg": "limbs.super_limb",
                  "pitchipoy.limbs.super_front_paw": "limbs.super_limb",
                  "pitchipoy.limbs.super_rear_paw": "limbs.super_limb",
                  "pitchipoy.limbs.super_finger": "limbs.super_finger",
                  "pitchipoy.super_torso_turbo": "spines.super_spine",
                  "pitchipoy.simple_tentacle": "limbs.simple_tentacle",
                  "pitchipoy.super_face": "faces.super_face",
                  "pitchipoy.super_palm": "limbs.super_palm",
                  "pitchipoy.super_copy": "basic.super_copy",
                  "pitchipoy.tentacle": "",
                  "palm": "limbs.super_palm",
                  "basic.copy": "basic.super_copy",
                  "biped.arm": "",
                  "biped.leg": "",
                  "finger": "",
                  "neck_short": "",
                  "misc.delta": "",
                  "spine": ""
                  }

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

def strip_trailing_number(s):
    m = re.search(r'\.(\d{3})$', s)
    return s[0:-4] if m else s


def unique_name(collection, base_name):
    base_name = strip_trailing_number(base_name)
    count = 1
    name = base_name

    while collection.get(name):
        name = "%s.%03d" % (base_name, count)
        count += 1
    return name


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


def strip_mch(name):
    """ Returns the name with ORG_PREFIX stripped from it.
        """
    if name.startswith(MCH_PREFIX):
        return name[len(MCH_PREFIX):]
    else:
        return name

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


def upgradeMetarigTypes(metarig, revert=False):
    """Replaces rigify_type properties from old versions with their current names

    :param revert: revert types to previous version (if old type available)
    """

    if revert:
        vals = list(outdated_types.values())
        rig_defs = {v: k for k, v in outdated_types.items() if vals.count(v) == 1}
    else:
        rig_defs = outdated_types

    for bone in metarig.pose.bones:
        rig_type = bone.rigify_type
        if rig_type in rig_defs:
            bone.rigify_type = rig_defs[rig_type]
            if 'leg' in rig_type:
                bone.rigfy_parameters.limb_type = 'leg'
            if 'arm' in rig_type:
                bone.rigfy_parameters.limb_type = 'arm'
            if 'paw' in rig_type:
                bone.rigfy_parameters.limb_type = 'paw'
            if rig_type == "basic.copy":
                bone.rigify_parameters.make_widget = False



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


def create_widget(rig, bone_name, bone_transform_name=None):
    """ Creates an empty widget object for a bone, and returns the object.
    """
    if bone_transform_name is None:
        bone_transform_name = bone_name

    obj_name = WGT_PREFIX + rig.name + '_' + bone_name
    scene = bpy.context.scene
    id_store = bpy.context.window_manager

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
        wgts_group_name = 'WGTS_' + rig.name
        if wgts_group_name in bpy.data.objects.keys():
            obj.parent = bpy.data.objects[wgts_group_name]
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
    if obj != None:
        v = [(0.7071068286895752, 2.980232238769531e-07, -0.7071065306663513), (0.8314696550369263, 2.980232238769531e-07, -0.5555699467658997), (0.9238795042037964, 2.682209014892578e-07, -0.3826831877231598), (0.9807852506637573, 2.5331974029541016e-07, -0.19509011507034302), (1.0, 2.365559055306221e-07, 1.6105803979371558e-07), (0.9807853698730469, 2.2351741790771484e-07, 0.19509044289588928), (0.9238796234130859, 2.086162567138672e-07, 0.38268351554870605), (0.8314696550369263, 1.7881393432617188e-07, 0.5555704236030579), (0.7071068286895752, 1.7881393432617188e-07, 0.7071070075035095), (0.5555702447891235, 1.7881393432617188e-07, 0.8314698934555054), (0.38268327713012695, 1.7881393432617188e-07, 0.923879861831665), (0.19509008526802063, 1.7881393432617188e-07, 0.9807855486869812), (-3.2584136988589307e-07, 1.1920928955078125e-07, 1.000000238418579), (-0.19509072601795197, 1.7881393432617188e-07, 0.9807854294776917), (-0.3826838731765747, 1.7881393432617188e-07, 0.9238795638084412), (-0.5555707216262817, 1.7881393432617188e-07, 0.8314695358276367), (-0.7071071863174438, 1.7881393432617188e-07, 0.7071065902709961), (-0.8314700126647949, 1.7881393432617188e-07, 0.5555698871612549), (-0.923879861831665, 2.086162567138672e-07, 0.3826829195022583), (-0.9807853698730469, 2.2351741790771484e-07, 0.1950896978378296), (-1.0, 2.365559907957504e-07, -7.290432222362142e-07), (-0.9807850122451782, 2.5331974029541016e-07, -0.195091113448143), (-0.9238790273666382, 2.682209014892578e-07, -0.38268423080444336), (-0.831468939781189, 2.980232238769531e-07, -0.5555710196495056), (-0.7071058750152588, 2.980232238769531e-07, -0.707107424736023), (-0.555569052696228, 2.980232238769531e-07, -0.8314701318740845), (-0.38268208503723145, 2.980232238769531e-07, -0.923879861831665), (-0.19508881866931915, 2.980232238769531e-07, -0.9807853102684021), (1.6053570561780361e-06, 2.980232238769531e-07, -0.9999997615814209), (0.19509197771549225, 2.980232238769531e-07, -0.9807847142219543), (0.3826850652694702, 2.980232238769531e-07, -0.9238786101341248), (0.5555717945098877, 2.980232238769531e-07, -0.8314683437347412)]
        verts = [(a[0] * radius, head_tail, a[2] * radius) for a in v]
        if with_line:
            edges = [(28, 12), (0, 1), (1, 2), (2, 3), (3, 4), (4, 5), (5, 6), (6, 7), (7, 8), (8, 9), (9, 10), (10, 11), (11, 12), (12, 13), (13, 14), (14, 15), (15, 16), (16, 17), (17, 18), (18, 19), (19, 20), (20, 21), (21, 22), (22, 23), (23, 24), (24, 25), (25, 26), (26, 27), (27, 28), (28, 29), (29, 30), (30, 31), (0, 31)]
        else:
            edges = [(0, 1), (1, 2), (2, 3), (3, 4), (4, 5), (5, 6), (6, 7), (7, 8), (8, 9), (9, 10), (10, 11), (11, 12), (12, 13), (13, 14), (14, 15), (15, 16), (16, 17), (17, 18), (18, 19), (19, 20), (20, 21), (21, 22), (22, 23), (23, 24), (24, 25), (25, 26), (26, 27), (27, 28), (28, 29), (29, 30), (30, 31), (0, 31)]
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


def create_chain_widget(rig, bone_name, radius=0.5, invert=False, bone_transform_name=None):
    """Creates a basic chain widget
    """
    obj = create_widget(rig, bone_name, bone_transform_name)
    if obj != None:
        r = radius
        rh = radius/2
        if invert:
            verts = [(rh, rh, rh), (r, -r, r), (-r, -r, r), (-rh, rh, rh), (rh, rh, -rh), (r, -r, -r), (-r, -r, -r), (-rh, rh, -rh)]
        else:
            verts = [(r, r, r), (rh, -rh, rh), (-rh, -rh, rh), (-r, r, r), (r, r, -r), (rh, -rh, -rh), (-rh, -rh, -rh), (-r, r, -r)]
        edges = [(0, 1), (1, 2), (2, 3), (3, 0), (4, 5), (5, 6), (6, 7), (7, 4), (0, 4), (1, 5), (2, 6), (3, 7)]
        mesh = obj.data
        mesh.from_pydata(verts, edges, [])
        mesh.update()


def create_sphere_widget(rig, bone_name, bone_transform_name=None):
    """ Creates a basic sphere widget, three pependicular overlapping circles.
    """
    obj = create_widget(rig, bone_name, bone_transform_name)
    if obj != None:
        verts = [(0.3535533845424652, 0.3535533845424652, 0.0), (0.4619397521018982, 0.19134171307086945, 0.0), (0.5, -2.1855694143368964e-08, 0.0), (0.4619397521018982, -0.19134175777435303, 0.0), (0.3535533845424652, -0.3535533845424652, 0.0), (0.19134174287319183, -0.4619397521018982, 0.0), (7.549790126404332e-08, -0.5, 0.0), (-0.1913416087627411, -0.46193981170654297, 0.0), (-0.35355329513549805, -0.35355350375175476, 0.0), (-0.4619397521018982, -0.19134178757667542, 0.0), (-0.5, 5.962440319251527e-09, 0.0), (-0.4619397222995758, 0.1913418024778366, 0.0), (-0.35355326533317566, 0.35355350375175476, 0.0), (-0.19134148955345154, 0.46193987131118774, 0.0), (3.2584136988589307e-07, 0.5, 0.0), (0.1913420855998993, 0.46193960309028625, 0.0), (7.450580596923828e-08, 0.46193960309028625, 0.19134199619293213), (5.9254205098113744e-08, 0.5, 2.323586443253589e-07), (4.470348358154297e-08, 0.46193987131118774, -0.1913415789604187), (2.9802322387695312e-08, 0.35355350375175476, -0.3535533547401428), (2.9802322387695312e-08, 0.19134178757667542, -0.46193981170654297), (5.960464477539063e-08, -1.1151834122813398e-08, -0.5000000596046448), (5.960464477539063e-08, -0.1913418024778366, -0.46193984150886536), (5.960464477539063e-08, -0.35355350375175476, -0.3535533845424652), (7.450580596923828e-08, -0.46193981170654297, -0.19134166836738586), (9.348272556053416e-08, -0.5, 1.624372103492533e-08), (1.043081283569336e-07, -0.4619397521018982, 0.19134168326854706), (1.1920928955078125e-07, -0.3535533845424652, 0.35355329513549805), (1.1920928955078125e-07, -0.19134174287319183, 0.46193966269493103), (1.1920928955078125e-07, -4.7414250303745575e-09, 0.49999991059303284), (1.1920928955078125e-07, 0.19134172797203064, 0.46193966269493103), (8.940696716308594e-08, 0.3535533845424652, 0.35355329513549805), (0.3535534739494324, 0.0, 0.35355329513549805), (0.1913418173789978, -2.9802322387695312e-08, 0.46193966269493103), (8.303572940349113e-08, -5.005858838558197e-08, 0.49999991059303284), (-0.19134165346622467, -5.960464477539063e-08, 0.46193966269493103), (-0.35355329513549805, -8.940696716308594e-08, 0.35355329513549805), (-0.46193963289260864, -5.960464477539063e-08, 0.19134168326854706), (-0.49999991059303284, -5.960464477539063e-08, 1.624372103492533e-08), (-0.4619397521018982, -2.9802322387695312e-08, -0.19134166836738586), (-0.3535534143447876, -2.9802322387695312e-08, -0.3535533845424652), (-0.19134171307086945, 0.0, -0.46193984150886536), (7.662531942287387e-08, 9.546055501630235e-09, -0.5000000596046448), (0.19134187698364258, 5.960464477539063e-08, -0.46193981170654297), (0.3535535931587219, 5.960464477539063e-08, -0.3535533547401428), (0.4619399905204773, 5.960464477539063e-08, -0.1913415789604187), (0.5000000596046448, 5.960464477539063e-08, 2.323586443253589e-07), (0.4619396924972534, 2.9802322387695312e-08, 0.19134199619293213)]
        edges = [(0, 1), (1, 2), (2, 3), (3, 4), (4, 5), (5, 6), (6, 7), (7, 8), (8, 9), (9, 10), (10, 11), (11, 12), (12, 13), (13, 14), (14, 15), (0, 15), (16, 31), (16, 17), (17, 18), (18, 19), (19, 20), (20, 21), (21, 22), (22, 23), (23, 24), (24, 25), (25, 26), (26, 27), (27, 28), (28, 29), (29, 30), (30, 31), (32, 33), (33, 34), (34, 35), (35, 36), (36, 37), (37, 38), (38, 39), (39, 40), (40, 41), (41, 42), (42, 43), (43, 44), (44, 45), (45, 46), (46, 47), (32, 47)]
        mesh = obj.data
        mesh.from_pydata(verts, edges, [])
        mesh.update()


def create_limb_widget(rig, bone_name, bone_transform_name=None):
    """ Creates a basic limb widget, a line that spans the length of the
        bone, with a circle around the center.
    """
    obj = create_widget(rig, bone_name, bone_transform_name)
    if obj != None:
        verts = [(-1.1920928955078125e-07, 1.7881393432617188e-07, 0.0), (3.5762786865234375e-07, 1.0000004768371582, 0.0), (0.1767769455909729, 0.5000001192092896, 0.17677664756774902), (0.20786768198013306, 0.5000001192092896, 0.1388925313949585), (0.23097014427185059, 0.5000001192092896, 0.09567084908485413), (0.24519658088684082, 0.5000001192092896, 0.048772573471069336), (0.2500002384185791, 0.5000001192092896, -2.545945676502015e-09), (0.24519658088684082, 0.5000001192092896, -0.048772573471069336), (0.23097014427185059, 0.5000001192092896, -0.09567084908485413), (0.20786768198013306, 0.5000001192092896, -0.13889259099960327), (0.1767769455909729, 0.5000001192092896, -0.1767767071723938), (0.13889282941818237, 0.5000001192092896, -0.20786744356155396), (0.09567105770111084, 0.5000001192092896, -0.23096990585327148), (0.04877278208732605, 0.5000001192092896, -0.24519634246826172), (1.7279069197684294e-07, 0.5000000596046448, -0.25), (-0.0487724244594574, 0.5000001192092896, -0.24519634246826172), (-0.09567070007324219, 0.5000001192092896, -0.2309698462486267), (-0.13889241218566895, 0.5000001192092896, -0.20786738395690918), (-0.17677652835845947, 0.5000001192092896, -0.17677664756774902), (-0.20786726474761963, 0.5000001192092896, -0.13889244198799133), (-0.23096972703933716, 0.5000001192092896, -0.09567070007324219), (-0.24519610404968262, 0.5000001192092896, -0.04877239465713501), (-0.2499997615814209, 0.5000001192092896, 2.1997936983098043e-07), (-0.24519598484039307, 0.5000001192092896, 0.04877282679080963), (-0.23096948862075806, 0.5000001192092896, 0.09567108750343323), (-0.20786696672439575, 0.5000001192092896, 0.1388927698135376), (-0.1767762303352356, 0.5000001192092896, 0.17677688598632812), (-0.13889199495315552, 0.5000001192092896, 0.2078675627708435), (-0.09567028284072876, 0.5000001192092896, 0.23097002506256104), (-0.048771947622299194, 0.5000001192092896, 0.24519634246826172), (6.555903269145347e-07, 0.5000001192092896, 0.25), (0.04877324402332306, 0.5000001192092896, 0.24519622325897217), (0.09567153453826904, 0.5000001192092896, 0.23096966743469238), (0.13889318704605103, 0.5000001192092896, 0.20786714553833008)]
        edges = [(0, 1), (2, 3), (4, 3), (5, 4), (5, 6), (6, 7), (8, 7), (8, 9), (10, 9), (10, 11), (11, 12), (13, 12), (14, 13), (14, 15), (16, 15), (16, 17), (17, 18), (19, 18), (19, 20), (21, 20), (21, 22), (22, 23), (24, 23), (25, 24), (25, 26), (27, 26), (27, 28), (29, 28), (29, 30), (30, 31), (32, 31), (32, 33), (2, 33)]
        mesh = obj.data
        mesh.from_pydata(verts, edges, [])
        mesh.update()


def create_bone_widget(rig, bone_name, bone_transform_name=None):
    """ Creates a basic bone widget, a simple obolisk-esk shape.
    """
    obj = create_widget(rig, bone_name, bone_transform_name)
    if obj != None:
        verts = [(0.04, 1.0, -0.04), (0.1, 0.0, -0.1), (-0.1, 0.0, -0.1), (-0.04, 1.0, -0.04), (0.04, 1.0, 0.04), (0.1, 0.0, 0.1), (-0.1, 0.0, 0.1), (-0.04, 1.0, 0.04)]
        edges = [(1, 2), (0, 1), (0, 3), (2, 3), (4, 5), (5, 6), (6, 7), (4, 7), (1, 5), (0, 4), (2, 6), (3, 7)]
        mesh = obj.data
        mesh.from_pydata(verts, edges, [])
        mesh.update()


def create_compass_widget(rig, bone_name, bone_transform_name=None):
    """ Creates a compass-shaped widget.
    """
    obj = create_widget(rig, bone_name, bone_transform_name)
    if obj != None:
        verts = [(0.0, 1.2000000476837158, 0.0), (0.19509032368659973, 0.9807852506637573, 0.0), (0.3826834559440613, 0.9238795042037964, 0.0), (0.5555702447891235, 0.8314695954322815, 0.0), (0.7071067690849304, 0.7071067690849304, 0.0), (0.8314696550369263, 0.5555701851844788, 0.0), (0.9238795042037964, 0.3826834261417389, 0.0), (0.9807852506637573, 0.19509035348892212, 0.0), (1.2000000476837158, 7.549790126404332e-08, 0.0), (0.9807853102684021, -0.19509020447731018, 0.0), (0.9238795638084412, -0.38268327713012695, 0.0), (0.8314696550369263, -0.5555701851844788, 0.0), (0.7071067690849304, -0.7071067690849304, 0.0), (0.5555701851844788, -0.8314696550369263, 0.0), (0.38268327713012695, -0.9238796234130859, 0.0), (0.19509008526802063, -0.9807853102684021, 0.0), (-3.2584136988589307e-07, -1.2999999523162842, 0.0), (-0.19509072601795197, -0.9807851910591125, 0.0), (-0.3826838731765747, -0.9238793253898621, 0.0), (-0.5555707216262817, -0.8314692974090576, 0.0), (-0.7071072459220886, -0.707106351852417, 0.0), (-0.8314700126647949, -0.5555696487426758, 0.0), (-0.923879861831665, -0.3826826810836792, 0.0), (-0.9807854294776917, -0.1950894594192505, 0.0), (-1.2000000476837158, 9.655991561885457e-07, 0.0), (-0.980785071849823, 0.1950913518667221, 0.0), (-0.923879086971283, 0.38268446922302246, 0.0), (-0.831468939781189, 0.5555712580680847, 0.0), (-0.7071058750152588, 0.707107663154602, 0.0), (-0.5555691123008728, 0.8314703702926636, 0.0), (-0.38268208503723145, 0.9238801002502441, 0.0), (-0.19508881866931915, 0.9807855486869812, 0.0)]
        edges = [(0, 1), (1, 2), (2, 3), (3, 4), (4, 5), (5, 6), (6, 7), (7, 8), (8, 9), (9, 10), (10, 11), (11, 12), (12, 13), (13, 14), (14, 15), (15, 16), (16, 17), (17, 18), (18, 19), (19, 20), (20, 21), (21, 22), (22, 23), (23, 24), (24, 25), (25, 26), (26, 27), (27, 28), (28, 29), (29, 30), (30, 31), (0, 31)]
        mesh = obj.data
        mesh.from_pydata(verts, edges, [])
        mesh.update()


def create_root_widget(rig, bone_name, bone_transform_name=None):
    """ Creates a widget for the root bone.
    """
    obj = create_widget(rig, bone_name, bone_transform_name)
    if obj != None:
        verts = [(0.7071067690849304, 0.7071067690849304, 0.0), (0.7071067690849304, -0.7071067690849304, 0.0), (-0.7071067690849304, 0.7071067690849304, 0.0), (-0.7071067690849304, -0.7071067690849304, 0.0), (0.8314696550369263, 0.5555701851844788, 0.0), (0.8314696550369263, -0.5555701851844788, 0.0), (-0.8314696550369263, 0.5555701851844788, 0.0), (-0.8314696550369263, -0.5555701851844788, 0.0), (0.9238795042037964, 0.3826834261417389, 0.0), (0.9238795042037964, -0.3826834261417389, 0.0), (-0.9238795042037964, 0.3826834261417389, 0.0), (-0.9238795042037964, -0.3826834261417389, 0.0), (0.9807852506637573, 0.19509035348892212, 0.0), (0.9807852506637573, -0.19509035348892212, 0.0), (-0.9807852506637573, 0.19509035348892212, 0.0), (-0.9807852506637573, -0.19509035348892212, 0.0), (0.19509197771549225, 0.9807849526405334, 0.0), (0.19509197771549225, -0.9807849526405334, 0.0), (-0.19509197771549225, 0.9807849526405334, 0.0), (-0.19509197771549225, -0.9807849526405334, 0.0), (0.3826850652694702, 0.9238788485527039, 0.0), (0.3826850652694702, -0.9238788485527039, 0.0), (-0.3826850652694702, 0.9238788485527039, 0.0), (-0.3826850652694702, -0.9238788485527039, 0.0), (0.5555717945098877, 0.8314685821533203, 0.0), (0.5555717945098877, -0.8314685821533203, 0.0), (-0.5555717945098877, 0.8314685821533203, 0.0), (-0.5555717945098877, -0.8314685821533203, 0.0), (0.19509197771549225, 1.2807848453521729, 0.0), (0.19509197771549225, -1.2807848453521729, 0.0), (-0.19509197771549225, 1.2807848453521729, 0.0), (-0.19509197771549225, -1.2807848453521729, 0.0), (1.280785322189331, 0.19509035348892212, 0.0), (1.280785322189331, -0.19509035348892212, 0.0), (-1.280785322189331, 0.19509035348892212, 0.0), (-1.280785322189331, -0.19509035348892212, 0.0), (0.3950919806957245, 1.2807848453521729, 0.0), (0.3950919806957245, -1.2807848453521729, 0.0), (-0.3950919806957245, 1.2807848453521729, 0.0), (-0.3950919806957245, -1.2807848453521729, 0.0), (1.280785322189331, 0.39509034156799316, 0.0), (1.280785322189331, -0.39509034156799316, 0.0), (-1.280785322189331, 0.39509034156799316, 0.0), (-1.280785322189331, -0.39509034156799316, 0.0), (0.0, 1.5807849168777466, 0.0), (0.0, -1.5807849168777466, 0.0), (1.5807852745056152, 0.0, 0.0), (-1.5807852745056152, 0.0, 0.0)]
        edges = [(0, 4), (1, 5), (2, 6), (3, 7), (4, 8), (5, 9), (6, 10), (7, 11), (8, 12), (9, 13), (10, 14), (11, 15), (16, 20), (17, 21), (18, 22), (19, 23), (20, 24), (21, 25), (22, 26), (23, 27), (0, 24), (1, 25), (2, 26), (3, 27), (16, 28), (17, 29), (18, 30), (19, 31), (12, 32), (13, 33), (14, 34), (15, 35), (28, 36), (29, 37), (30, 38), (31, 39), (32, 40), (33, 41), (34, 42), (35, 43), (36, 44), (37, 45), (38, 44), (39, 45), (40, 46), (41, 46), (42, 47), (43, 47)]
        mesh = obj.data
        mesh.from_pydata(verts, edges, [])
        mesh.update()


def create_neck_bend_widget(rig, bone_name, radius=1.0, head_tail=0.0, bone_transform_name=None):
    obj = create_widget(rig, bone_name, bone_transform_name)
    size = 2.0
    if obj != None:
        v = [(-0.08855080604553223 * size, 0.7388765811920166 * size, -0.3940150737762451 * size),
                 (0.08855044841766357 * size, 0.7388765811920166 * size, -0.3940150737762451 * size),
                 (0.17710095643997192 * size, 0.5611097812652588 * size, -0.6478927135467529 * size),
                 (-4.0892032870942785e-07 * size, 0.4087378978729248 * size, -0.865501880645752 * size),
                 (-0.17710143327713013 * size, 0.5611097812652588 * size, -0.6478922367095947 * size),
                 (0.08855026960372925 * size, 0.5611097812652588 * size, -0.6478924751281738 * size),
                 (-0.08855092525482178 * size, 0.5611097812652588 * size, -0.6478927135467529 * size),
                 (-0.6478927135467529 * size, 0.5611097812652588 * size, 0.08855098485946655 * size),
                 (-0.6478927135467529 * size, 0.5611097812652588 * size, -0.08855020999908447 * size),
                 (-0.6478924751281738 * size, 0.5611097812652588 * size, 0.17710155248641968 * size),
                 (-0.865501880645752 * size, 0.4087378978729248 * size, 4.6876743908796925e-07 * size),
                 (-0.647892951965332 * size, 0.5611097812652588 * size, -0.17710083723068237 * size),
                 (-0.39401543140411377 * size, 0.7388765811920166 * size, -0.08855029940605164 * size),
                 (-0.39401543140411377 * size, 0.7388765811920166 * size, 0.08855095505714417 * size),
                 (0.6478927135467529 * size, 0.5611097812652588 * size, -0.08855059742927551 * size),
                 (0.6478927135467529 * size, 0.5611097812652588 * size, 0.08855065703392029 * size),
                 (0.6478924751281738 * size, 0.5611097812652588 * size, -0.17710113525390625 * size),
                 (0.865501880645752 * size, 0.4087378978729248 * size, -3.264514703005261e-08 * size),
                 (0.647892951965332 * size, 0.5611097812652588 * size, 0.1771012544631958 * size),
                 (0.08855065703392029 * size, 0.7388765811920166 * size, 0.3940155506134033 * size),
                 (-0.08855056762695312 * size, 0.7388765811920166 * size, 0.3940155506134033 * size),
                 (-0.17710107564926147 * size, 0.5611097812652588 * size, 0.647892951965332 * size),
                 (2.244429140318971e-07 * size, 0.4087378978729248 * size, 0.865502119064331 * size),
                 (0.17710131406784058 * size, 0.5611097812652588 * size, 0.6478927135467529 * size),
                 (-0.08855044841766357 * size, 0.5611097812652588 * size, 0.647892951965332 * size),
                 (0.08855074644088745 * size, 0.5611097812652588 * size, 0.647892951965332 * size),
                 (0.3940153121948242 * size, 0.7388765811920166 * size, 0.08855071663856506 * size),
                 (0.39401519298553467 * size, 0.7388765811920166 * size, -0.08855047821998596 * size),
                 (-8.416645869147032e-08 * size, 0.8255770206451416 * size, -0.2656517028808594 * size),
                 (-0.06875583529472351 * size, 0.8255770206451416 * size, -0.2565997838973999 * size),
                 (-0.13282597064971924 * size, 0.8255770206451416 * size, -0.2300611138343811 * size),
                 (-0.18784427642822266 * size, 0.8255770206451416 * size, -0.18784409761428833 * size),
                 (-0.2300613522529602 * size, 0.8255770206451416 * size, -0.1328257918357849 * size),
                 (-0.256600022315979 * size, 0.8255770206451416 * size, -0.06875564157962799 * size),
                 (-0.2656519412994385 * size, 0.8255770206451416 * size, 9.328307726264029e-08 * size),
                 (-0.25660014152526855 * size, 0.8255770206451416 * size, 0.06875583529472351 * size),
                 (-0.2300613522529602 * size, 0.8255770206451416 * size, 0.13282597064971924 * size),
                 (-0.18784433603286743 * size, 0.8255770206451416 * size, 0.18784421682357788 * size),
                 (-0.1328260898590088 * size, 0.8255770206451416 * size, 0.23006129264831543 * size),
                 (-0.06875592470169067 * size, 0.8255770206451416 * size, 0.256600022315979 * size),
                 (-1.8761508613351907e-07 * size, 0.8255770206451416 * size, 0.2656519412994385 * size),
                 (0.06875556707382202 * size, 0.8255770206451416 * size, 0.2566000819206238 * size),
                 (0.13282573223114014 * size, 0.8255770206451416 * size, 0.23006141185760498 * size),
                 (0.18784403800964355 * size, 0.8255770206451416 * size, 0.1878443956375122 * size),
                 (0.23006105422973633 * size, 0.8255770206451416 * size, 0.1328260898590088 * size),
                 (0.25659990310668945 * size, 0.8255770206451416 * size, 0.06875596940517426 * size),
                 (0.2656517028808594 * size, 0.8255770206451416 * size, 2.3684407324253698e-07 * size),
                 (0.25659990310668945 * size, 0.8255770206451416 * size, -0.06875550746917725 * size),
                 (0.23006117343902588 * size, 0.8255770206451416 * size, -0.13282567262649536 * size),
                 (0.18784427642822266 * size, 0.8255770206451416 * size, -0.18784397840499878 * size),
                 (0.13282597064971924 * size, 0.8255770206451416 * size, -0.23006099462509155 * size),
                 (0.0687558501958847 * size, 0.8255770206451416 * size, -0.2565997838973999 * size), ]
        edges = [(1, 0), (3, 2), (5, 2), (4, 3), (6, 4), (1, 5), (0, 6), (13, 7), (12, 8), (7, 9), (9, 10), (8, 11),
                 (27, 14), (26, 15), (14, 16), (16, 17), (15, 18), (17, 18), (10, 11), (12, 13), (20, 19), (22, 21),
                 (24, 21), (23, 22), (29, 28), (30, 29), (31, 30), (32, 31), (33, 32), (34, 33), (35, 34), (36, 35),
                 (37, 36), (38, 37), (39, 38), (40, 39), (41, 40), (42, 41), (43, 42), (44, 43), (45, 44), (46, 45),
                 (47, 46), (48, 47), (49, 48), (50, 49), (51, 50), (28, 51), (26, 27), (25, 23), (20, 24),
                 (19, 25), ]

        verts = [(a[0] * radius, head_tail, a[2] * radius) for a in v]
        mesh = obj.data
        mesh.from_pydata(verts, edges, [])
        mesh.update()


def create_neck_tweak_widget(rig, bone_name, size=1.0, bone_transform_name=None):
    obj = create_widget(rig, bone_name, bone_transform_name)

    if obj != None:
        verts = [(0.3535533845424652 * size, 0.3535533845424652 * size, 0.0 * size),
                 (0.4619397521018982 * size, 0.19134171307086945 * size, 0.0 * size),
                 (0.5 * size, -2.1855694143368964e-08 * size, 0.0 * size),
                 (0.4619397521018982 * size, -0.19134175777435303 * size, 0.0 * size),
                 (0.3535533845424652 * size, -0.3535533845424652 * size, 0.0 * size),
                 (0.19134174287319183 * size, -0.4619397521018982 * size, 0.0 * size),
                 (7.549790126404332e-08 * size, -0.5 * size, 0.0 * size),
                 (-0.1913416087627411 * size, -0.46193981170654297 * size, 0.0 * size),
                 (-0.35355329513549805 * size, -0.35355350375175476 * size, 0.0 * size),
                 (-0.4619397521018982 * size, -0.19134178757667542 * size, 0.0 * size),
                 (-0.5 * size, 5.962440319251527e-09 * size, 0.0 * size),
                 (-0.4619397222995758 * size, 0.1913418024778366 * size, 0.0 * size),
                 (-0.35355326533317566 * size, 0.35355350375175476 * size, 0.0 * size),
                 (-0.19134148955345154 * size, 0.46193987131118774 * size, 0.0 * size),
                 (3.2584136988589307e-07 * size, 0.5 * size, 0.0 * size),
                 (0.1913420855998993 * size, 0.46193960309028625 * size, 0.0 * size),
                 (7.450580596923828e-08 * size, 0.46193960309028625 * size, 0.19134199619293213 * size),
                 (5.9254205098113744e-08 * size, 0.5 * size, 2.323586443253589e-07 * size),
                 (4.470348358154297e-08 * size, 0.46193987131118774 * size, -0.1913415789604187 * size),
                 (2.9802322387695312e-08 * size, 0.35355350375175476 * size, -0.3535533547401428 * size),
                 (2.9802322387695312e-08 * size, 0.19134178757667542 * size, -0.46193981170654297 * size),
                 (5.960464477539063e-08 * size, -1.1151834122813398e-08 * size, -0.5000000596046448 * size),
                 (5.960464477539063e-08 * size, -0.1913418024778366 * size, -0.46193984150886536 * size),
                 (5.960464477539063e-08 * size, -0.35355350375175476 * size, -0.3535533845424652 * size),
                 (7.450580596923828e-08 * size, -0.46193981170654297 * size, -0.19134166836738586 * size),
                 (9.348272556053416e-08 * size, -0.5 * size, 1.624372103492533e-08 * size),
                 (1.043081283569336e-07 * size, -0.4619397521018982 * size, 0.19134168326854706 * size),
                 (1.1920928955078125e-07 * size, -0.3535533845424652 * size, 0.35355329513549805 * size),
                 (1.1920928955078125e-07 * size, -0.19134174287319183 * size, 0.46193966269493103 * size),
                 (1.1920928955078125e-07 * size, -4.7414250303745575e-09 * size, 0.49999991059303284 * size),
                 (1.1920928955078125e-07 * size, 0.19134172797203064 * size, 0.46193966269493103 * size),
                 (8.940696716308594e-08 * size, 0.3535533845424652 * size, 0.35355329513549805 * size),
                 (0.3535534739494324 * size, 0.0 * size, 0.35355329513549805 * size),
                 (0.1913418173789978 * size, -2.9802322387695312e-08 * size, 0.46193966269493103 * size),
                 (8.303572940349113e-08 * size, -5.005858838558197e-08 * size, 0.49999991059303284 * size),
                 (-0.19134165346622467 * size, -5.960464477539063e-08 * size, 0.46193966269493103 * size),
                 (-0.35355329513549805 * size, -8.940696716308594e-08 * size, 0.35355329513549805 * size),
                 (-0.46193963289260864 * size, -5.960464477539063e-08 * size, 0.19134168326854706 * size),
                 (-0.49999991059303284 * size, -5.960464477539063e-08 * size, 1.624372103492533e-08 * size),
                 (-0.4619397521018982 * size, -2.9802322387695312e-08 * size, -0.19134166836738586 * size),
                 (-0.3535534143447876 * size, -2.9802322387695312e-08 * size, -0.3535533845424652 * size),
                 (-0.19134171307086945 * size, 0.0 * size, -0.46193984150886536 * size),
                 (7.662531942287387e-08 * size, 9.546055501630235e-09 * size, -0.5000000596046448 * size),
                 (0.19134187698364258 * size, 5.960464477539063e-08 * size, -0.46193981170654297 * size),
                 (0.3535535931587219 * size, 5.960464477539063e-08 * size, -0.3535533547401428 * size),
                 (0.4619399905204773 * size, 5.960464477539063e-08 * size, -0.1913415789604187 * size),
                 (0.5000000596046448 * size, 5.960464477539063e-08 * size, 2.323586443253589e-07 * size),
                 (0.4619396924972534 * size, 2.9802322387695312e-08 * size, 0.19134199619293213 * size),
                 (1.563460111618042 * size, 2.778762819843905e-08 * size, 1.5634593963623047 * size),
                 (0.8461387157440186 * size, -1.0400220418205208e-07 * size, 2.0427582263946533 * size),
                 (7.321979467178608e-08 * size, -1.9357810288056498e-07 * size, 2.2110657691955566 * size),
                 (-0.8461385369300842 * size, -2.3579201524626114e-07 * size, 2.0427582263946533 * size),
                 (-1.5634597539901733 * size, -3.67581861837607e-07 * size, 1.5634593963623047 * size),
                 (-2.0427584648132324 * size, -2.3579204366797057e-07 * size, 0.8461383581161499 * size),
                 (-2.211066246032715 * size, -2.3579204366797057e-07 * size, 9.972505665700737e-08 * size),
                 (-2.0427589416503906 * size, -1.0400223260376151e-07 * size, -0.8461381196975708 * size),
                 (-1.5634604692459106 * size, -1.040022183929068e-07 * size, -1.563459873199463 * size),
                 (-0.8461387753486633 * size, 2.77876033294433e-08 * size, -2.042759418487549 * size),
                 (4.4872678017782164e-08 * size, 7.00015263532805e-08 * size, -2.211066484451294 * size),
                 (0.8461388349533081 * size, 2.913672290105751e-07 * size, -2.0427591800689697 * size),
                 (1.5634608268737793 * size, 2.9136725743228453e-07 * size, -1.563459873199463 * size),
                 (2.042759895324707 * size, 2.9136725743228453e-07 * size, -0.8461377024650574 * size),
                 (2.211066722869873 * size, 2.9136725743228453e-07 * size, 1.0554133496043505e-06 * size),
                 (2.0427587032318115 * size, 1.5957746768435754e-07 * size, 0.8461397886276245 * size), ]
        edges = [(0, 1), (1, 2), (2, 3), (3, 4), (4, 5), (5, 6), (6, 7), (7, 8), (8, 9), (9, 10), (10, 11),
                 (11, 12), (12, 13), (13, 14), (14, 15), (0, 15), (16, 31), (16, 17), (17, 18), (18, 19), (19, 20),
                 (20, 21), (21, 22), (22, 23), (23, 24), (24, 25), (25, 26), (26, 27), (27, 28), (28, 29), (29, 30),
                 (30, 31), (32, 33), (33, 34), (34, 35), (35, 36), (36, 37), (37, 38), (38, 39), (39, 40), (40, 41),
                 (41, 42), (42, 43), (43, 44), (44, 45), (45, 46), (46, 47), (32, 47), (48, 49), (49, 50), (50, 51),
                 (51, 52), (52, 53), (53, 54), (54, 55), (55, 56), (56, 57), (57, 58), (58, 59), (59, 60), (60, 61),
                 (61, 62), (62, 63), (48, 63), (21, 58), (10, 54), (29, 50), (2, 62), ]

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


def align_bone_y_axis(obj, bone, vec):
    """ Matches the bone y-axis to
        the given vector.
        Must be in edit mode.
    """

    bone_e = obj.data.edit_bones[bone]
    vec.normalize()
    vec = vec * bone_e.length

    bone_e.tail = bone_e.head + vec


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
    name = ".%s.%s" % (RIG_DIR, rig_type)
    submod = importlib.import_module(name, package=MODULE_NAME)
    importlib.reload(submod)
    return submod


def get_metarig_module(metarig_name, path=METARIG_DIR):
    """ Fetches a rig module by name, and returns it.
    """

    name = ".%s.%s" % (path, metarig_name)
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


def write_metarig(obj, layers=False, func_name="create", groups=False):
    """
    Write a metarig as a python script, this rig is to have all info needed for
    generating the real rig with rigify.
    """
    code = []

    code.append("import bpy\n\n")
    code.append("from mathutils import Color\n\n")

    code.append("def %s(obj):" % func_name)
    code.append("    # generated by rigify.utils.write_metarig")
    bpy.ops.object.mode_set(mode='EDIT')
    code.append("    bpy.ops.object.mode_set(mode='EDIT')")
    code.append("    arm = obj.data")

    arm = obj.data

    # Rigify bone group colors info
    if groups and len(arm.rigify_colors) > 0:
        code.append("\n    for i in range(" + str(len(arm.rigify_colors)) + "):")
        code.append("        arm.rigify_colors.add()\n")

        for i in range(len(arm.rigify_colors)):
            name = arm.rigify_colors[i].name
            active = arm.rigify_colors[i].active
            normal = arm.rigify_colors[i].normal
            select = arm.rigify_colors[i].select
            standard_colors_lock = arm.rigify_colors[i].standard_colors_lock
            code.append('    arm.rigify_colors[' + str(i) + '].name = "' + name + '"')
            code.append('    arm.rigify_colors[' + str(i) + '].active = Color(' + str(active[:]) + ')')
            code.append('    arm.rigify_colors[' + str(i) + '].normal = Color(' + str(normal[:]) + ')')
            code.append('    arm.rigify_colors[' + str(i) + '].select = Color(' + str(select[:]) + ')')
            code.append('    arm.rigify_colors[' + str(i) + '].standard_colors_lock = ' + str(standard_colors_lock))

    # Rigify layer layout info
    if layers and len(arm.rigify_layers) > 0:
        code.append("\n    for i in range(" + str(len(arm.rigify_layers)) + "):")
        code.append("        arm.rigify_layers.add()\n")

        for i in range(len(arm.rigify_layers)):
            name = arm.rigify_layers[i].name
            row = arm.rigify_layers[i].row
            set = arm.rigify_layers[i].set
            group = arm.rigify_layers[i].group
            code.append('    arm.rigify_layers[' + str(i) + '].name = "' + name + '"')
            code.append('    arm.rigify_layers[' + str(i) + '].row = ' + str(row))
            code.append('    arm.rigify_layers[' + str(i) + '].set = ' + str(set))
            code.append('    arm.rigify_layers[' + str(i) + '].group = ' + str(group))

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
            param = getattr(pbone.rigify_parameters, param_name, '')
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
    script += "    if obj != None:\n"

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


#=============================================
# Color correction functions
#=============================================

def linsrgb_to_srgb (linsrgb):
    """Convert physically linear RGB values into sRGB ones. The transform is
    uniform in the components, so *linsrgb* can be of any shape.

    *linsrgb* values should range between 0 and 1, inclusively.

    """
    # From Wikipedia, but easy analogue to the above.
    gamma = 1.055 * linsrgb**(1./2.4) - 0.055
    scale = linsrgb * 12.92
    # return np.where (linsrgb > 0.0031308, gamma, scale)
    if linsrgb > 0.0031308:
        return gamma
    return scale


def gamma_correct(color):

    corrected_color = Color()
    for i, component in enumerate(color):
        corrected_color[i] = linsrgb_to_srgb(color[i])
    return corrected_color


#=============================================
# Keyframing functions
#=============================================


def get_keyed_frames(rig):
    frames = []
    if rig.animation_data:
        if rig.animation_data.action:
            fcus = rig.animation_data.action.fcurves
            for fc in fcus:
                for kp in fc.keyframe_points:
                    if kp.co[0] not in frames:
                        frames.append(kp.co[0])

    frames.sort()

    return frames


def bones_in_frame(f, rig, *args):
    """
    True if one of the bones listed in args is animated at frame f
    :param f: the frame
    :param rig: the rig
    :param args: bone names
    :return:
    """

    if rig.animation_data and rig.animation_data.action:
        fcus = rig.animation_data.action.fcurves
    else:
        return False

    for fc in fcus:
        animated_frames = [kp.co[0] for kp in fc.keyframe_points]
        for bone in args:
            if bone in fc.data_path.split('"') and f in animated_frames:
                return True

    return False


def overwrite_prop_animation(rig, bone, prop_name, value, frames):
    act = rig.animation_data.action
    if not act:
        return

    bone_name = bone.name
    curve = None

    for fcu in act.fcurves:
        words = fcu.data_path.split('"')
        if words[0] == "pose.bones[" and words[1] == bone_name and words[-2] == prop_name:
            curve = fcu
            break

    if not curve:
        return

    for kp in curve.keyframe_points:
        if kp.co[0] in frames:
            kp.co[1] = value
