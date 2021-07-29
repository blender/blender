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
from mathutils import Vector

from .. import limb_common

from ....utils import MetarigError
from ....utils import align_bone_x_axis
from ....utils import copy_bone, flip_bone, put_bone
from ....utils import connected_children_names, has_connected_children
from ....utils import strip_org, make_mechanism_name, insert_before_lr
from ....utils import create_widget, create_circle_widget


class Rig:
    """ An IK leg rig, with an optional ik/fk switch.

    """
    def __init__(self, obj, bone, params, ikfk_switch=False):
        """ Gather and validate data about the rig.
            Store any data or references to data that will be needed later on.
            In particular, store references to bones that will be needed, and
            store names of bones that will be needed.
            Do NOT change any data in the scene.  This is a gathering phase only.
        """
        self.obj = obj
        self.params = params
        self.switch = ikfk_switch

        # Get the chain of 2 connected bones
        leg_bones = [bone] + connected_children_names(self.obj, bone)[:2]

        if len(leg_bones) != 2:
            raise MetarigError("RIGIFY ERROR: Bone '%s': incorrect bone configuration for rig type" % (strip_org(bone)))

        # Get the foot and heel
        foot = None
        heel = None
        rocker = None
        for b in self.obj.data.bones[leg_bones[1]].children:
            if b.use_connect is True:
                if len(b.children) >= 1 and has_connected_children(b):
                    foot = b.name
                else:
                    heel = b.name
                    if len(b.children) > 0:
                        rocker = b.children[0].name

        if foot is None or heel is None:
            print("blah")
            raise MetarigError("RIGIFY ERROR: Bone '%s': incorrect bone configuration for rig type" % (strip_org(bone)))

        # Get the toe
        toe = None
        for b in self.obj.data.bones[foot].children:
            if b.use_connect is True:
                toe = b.name

        # Get toe
        if toe is None:
            raise MetarigError("RIGIFY ERROR: Bone '%s': incorrect bone configuration for rig type" % (strip_org(bone)))

        self.org_bones = leg_bones + [foot, toe, heel, rocker]

        # Get rig parameters
        if params.separate_ik_layers:
            self.layers = list(params.ik_layers)
        else:
            self.layers = None
        bend_hint = params.bend_hint
        primary_rotation_axis = params.primary_rotation_axis
        pole_target_base_name = self.params.knee_base_name + "_target"

        # Leg is based on common limb
        self.ik_limb = limb_common.IKLimb(obj, self.org_bones[0], self.org_bones[1], self.org_bones[2], self.org_bones[2], pole_target_base_name, primary_rotation_axis, bend_hint, self.layers, ikfk_switch)

    def generate(self):
        """ Generate the rig.
            Do NOT modify any of the original bones, except for adding constraints.
            The main armature should be selected and active before this is called.
        """
        # Generate base IK limb
        bone_list = self.ik_limb.generate()
        thigh = bone_list[0]
        shin = bone_list[1]
        foot = bone_list[2]
        foot_mch = bone_list[3]
        pole = bone_list[4]
        # vispole = bone_list[5]
        # visfoot = bone_list[6]

        # Build IK foot rig
        bpy.ops.object.mode_set(mode='EDIT')
        make_rocker = False
        if self.org_bones[5] is not None:
            make_rocker = True

        # Create the bones
        toe = copy_bone(self.obj, self.org_bones[3], strip_org(self.org_bones[3]))
        toe_parent = copy_bone(self.obj, self.org_bones[2], make_mechanism_name(strip_org(self.org_bones[3] + ".parent")))
        toe_parent_socket1 = copy_bone(self.obj, self.org_bones[2], make_mechanism_name(strip_org(self.org_bones[3] + ".socket1")))
        toe_parent_socket2 = copy_bone(self.obj, self.org_bones[2], make_mechanism_name(strip_org(self.org_bones[3] + ".socket2")))

        foot_roll = copy_bone(self.obj, self.org_bones[4], strip_org(insert_before_lr(self.org_bones[2], "_roll.ik")))
        roll1 = copy_bone(self.obj, self.org_bones[4], make_mechanism_name(strip_org(self.org_bones[2] + ".roll.01")))
        roll2 = copy_bone(self.obj, self.org_bones[4], make_mechanism_name(strip_org(self.org_bones[2] + ".roll.02")))

        if make_rocker:
            rocker1 = copy_bone(self.obj, self.org_bones[5], make_mechanism_name(strip_org(self.org_bones[2] + ".rocker.01")))
            rocker2 = copy_bone(self.obj, self.org_bones[5], make_mechanism_name(strip_org(self.org_bones[2] + ".rocker.02")))

        # Get edit bones
        eb = self.obj.data.edit_bones

        org_foot_e = eb[self.org_bones[2]]
        foot_e = eb[foot]
        foot_ik_target_e = eb[foot_mch]
        toe_e = eb[toe]
        toe_parent_e = eb[toe_parent]
        toe_parent_socket1_e = eb[toe_parent_socket1]
        toe_parent_socket2_e = eb[toe_parent_socket2]
        foot_roll_e = eb[foot_roll]
        roll1_e = eb[roll1]
        roll2_e = eb[roll2]
        if make_rocker:
            rocker1_e = eb[rocker1]
            rocker2_e = eb[rocker2]

        # Parenting
        foot_ik_target_e.use_connect = False
        foot_ik_target_e.parent = roll2_e

        toe_e.parent = toe_parent_e
        toe_parent_e.use_connect = False
        toe_parent_e.parent = toe_parent_socket1_e
        toe_parent_socket1_e.use_connect = False
        toe_parent_socket1_e.parent = roll1_e
        toe_parent_socket2_e.use_connect = False
        toe_parent_socket2_e.parent = eb[self.org_bones[2]]

        foot_roll_e.use_connect = False
        foot_roll_e.parent = foot_e

        roll1_e.use_connect = False
        roll1_e.parent = foot_e

        roll2_e.use_connect = False
        roll2_e.parent = roll1_e

        if make_rocker:
            rocker1_e.use_connect = False
            rocker2_e.use_connect = False

            roll1_e.parent = rocker2_e
            rocker2_e.parent = rocker1_e
            rocker1_e.parent = foot_e

        # Positioning
        vec = Vector(toe_e.vector)
        vec.normalize()
        foot_e.tail = foot_e.head + (vec * foot_e.length)
        foot_e.roll = toe_e.roll

        flip_bone(self.obj, toe_parent_socket1)
        flip_bone(self.obj, toe_parent_socket2)
        toe_parent_socket1_e.head = Vector(org_foot_e.tail)
        toe_parent_socket2_e.head = Vector(org_foot_e.tail)
        toe_parent_socket1_e.tail = Vector(org_foot_e.tail) + (Vector((0, 0, 1)) * foot_e.length / 2)
        toe_parent_socket2_e.tail = Vector(org_foot_e.tail) + (Vector((0, 0, 1)) * foot_e.length / 3)
        toe_parent_socket2_e.roll = toe_parent_socket1_e.roll

        tail = Vector(roll1_e.tail)
        roll1_e.tail = Vector(org_foot_e.tail)
        roll1_e.tail = Vector(org_foot_e.tail)
        roll1_e.head = tail
        roll2_e.head = Vector(org_foot_e.tail)
        foot_roll_e.head = Vector(org_foot_e.tail)
        put_bone(self.obj, foot_roll, roll1_e.head)
        foot_roll_e.length /= 2

        roll_axis = roll1_e.vector.cross(org_foot_e.vector)
        align_bone_x_axis(self.obj, roll1, roll_axis)
        align_bone_x_axis(self.obj, roll2, roll_axis)
        foot_roll_e.roll = roll2_e.roll

        if make_rocker:
            d = toe_e.y_axis.dot(rocker1_e.x_axis)
            if d >= 0.0:
                flip_bone(self.obj, rocker2)
            else:
                flip_bone(self.obj, rocker1)

        # Object mode, get pose bones
        bpy.ops.object.mode_set(mode='OBJECT')
        pb = self.obj.pose.bones

        foot_p = pb[foot]
        foot_roll_p = pb[foot_roll]
        roll1_p = pb[roll1]
        roll2_p = pb[roll2]
        if make_rocker:
            rocker1_p = pb[rocker1]
            rocker2_p = pb[rocker2]
        toe_p = pb[toe]
        # toe_parent_p = pb[toe_parent]
        toe_parent_socket1_p = pb[toe_parent_socket1]

        # Foot roll control only rotates on x-axis, or x and y if rocker.
        foot_roll_p.rotation_mode = 'XYZ'
        if make_rocker:
            foot_roll_p.lock_rotation = False, False, True
        else:
            foot_roll_p.lock_rotation = False, True, True
        foot_roll_p.lock_location = True, True, True
        foot_roll_p.lock_scale = True, True, True

        # roll and rocker bones set to euler rotation
        roll1_p.rotation_mode = 'XYZ'
        roll2_p.rotation_mode = 'XYZ'
        if make_rocker:
            rocker1_p.rotation_mode = 'XYZ'
            rocker2_p.rotation_mode = 'XYZ'

        # toe_parent constraint
        con = toe_parent_socket1_p.constraints.new('COPY_LOCATION')
        con.name = "copy_location"
        con.target = self.obj
        con.subtarget = toe_parent_socket2

        con = toe_parent_socket1_p.constraints.new('COPY_SCALE')
        con.name = "copy_scale"
        con.target = self.obj
        con.subtarget = toe_parent_socket2

        con = toe_parent_socket1_p.constraints.new('COPY_TRANSFORMS')  # drive with IK switch
        con.name = "fk"
        con.target = self.obj
        con.subtarget = toe_parent_socket2

        fcurve = con.driver_add("influence")
        driver = fcurve.driver
        var = driver.variables.new()
        driver.type = 'AVERAGE'
        var.name = "var"
        var.targets[0].id_type = 'OBJECT'
        var.targets[0].id = self.obj
        var.targets[0].data_path = foot_p.path_from_id() + '["ikfk_switch"]'
        mod = fcurve.modifiers[0]
        mod.poly_order = 1
        mod.coefficients[0] = 1.0
        mod.coefficients[1] = -1.0

        # Foot roll drivers
        fcurve = roll1_p.driver_add("rotation_euler", 0)
        driver = fcurve.driver
        var = driver.variables.new()
        driver.type = 'SCRIPTED'
        driver.expression = "min(0,var)"
        var.name = "var"
        var.targets[0].id_type = 'OBJECT'
        var.targets[0].id = self.obj
        var.targets[0].data_path = foot_roll_p.path_from_id() + '.rotation_euler[0]'

        fcurve = roll2_p.driver_add("rotation_euler", 0)
        driver = fcurve.driver
        var = driver.variables.new()
        driver.type = 'SCRIPTED'
        driver.expression = "max(0,var)"
        var.name = "var"
        var.targets[0].id_type = 'OBJECT'
        var.targets[0].id = self.obj
        var.targets[0].data_path = foot_roll_p.path_from_id() + '.rotation_euler[0]'

        if make_rocker:
            fcurve = rocker1_p.driver_add("rotation_euler", 0)
            driver = fcurve.driver
            var = driver.variables.new()
            driver.type = 'SCRIPTED'
            driver.expression = "max(0,-var)"
            var.name = "var"
            var.targets[0].id_type = 'OBJECT'
            var.targets[0].id = self.obj
            var.targets[0].data_path = foot_roll_p.path_from_id() + '.rotation_euler[1]'

            fcurve = rocker2_p.driver_add("rotation_euler", 0)
            driver = fcurve.driver
            var = driver.variables.new()
            driver.type = 'SCRIPTED'
            driver.expression = "max(0,var)"
            var.name = "var"
            var.targets[0].id_type = 'OBJECT'
            var.targets[0].id = self.obj
            var.targets[0].data_path = foot_roll_p.path_from_id() + '.rotation_euler[1]'

        # Constrain toe bone to toe control
        con = pb[self.org_bones[3]].constraints.new('COPY_TRANSFORMS')
        con.name = "copy_transforms"
        con.target = self.obj
        con.subtarget = toe

        # Set layers if specified
        if self.layers:
            foot_roll_p.bone.layers = self.layers
            toe_p.bone.layers = [(i[0] or i[1]) for i in zip(toe_p.bone.layers, self.layers)]  # Both FK and IK layers

        # Create widgets
        create_circle_widget(self.obj, toe, radius=0.7, head_tail=0.5)

        ob = create_widget(self.obj, foot_roll)
        if ob is not None:
            verts = [(0.3999999761581421, 0.766044557094574, 0.6427875757217407), (0.17668449878692627, 3.823702598992895e-08, 3.2084670920085046e-08), (-0.17668461799621582, 9.874240447516058e-08, 8.285470443070153e-08), (-0.39999961853027344, 0.7660449147224426, 0.6427879333496094), (0.3562471270561218, 0.6159579753875732, 0.5168500542640686), (-0.35624682903289795, 0.6159582138061523, 0.5168502926826477), (0.20492683351039886, 0.09688037633895874, 0.0812922865152359), (-0.20492687821388245, 0.0968804731965065, 0.08129236847162247)]
            edges = [(1, 2), (0, 3), (0, 4), (3, 5), (1, 6), (4, 6), (2, 7), (5, 7)]
            mesh = ob.data
            mesh.from_pydata(verts, edges, [])
            mesh.update()

            mod = ob.modifiers.new("subsurf", 'SUBSURF')
            mod.levels = 2

        ob = create_widget(self.obj, foot)
        if ob is not None:
            verts = [(0.7, 1.5, 0.0), (0.7, -0.25, 0.0), (-0.7, -0.25, 0.0), (-0.7, 1.5, 0.0), (0.7, 0.723, 0.0), (-0.7, 0.723, 0.0), (0.7, 0.0, 0.0), (-0.7, 0.0, 0.0)]
            edges = [(1, 2), (0, 3), (0, 4), (3, 5), (4, 6), (1, 6), (5, 7), (2, 7)]
            mesh = ob.data
            mesh.from_pydata(verts, edges, [])
            mesh.update()

            mod = ob.modifiers.new("subsurf", 'SUBSURF')
            mod.levels = 2

        return [thigh, shin, foot, pole, foot_roll, foot_mch]
