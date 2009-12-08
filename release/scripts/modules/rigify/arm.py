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
from rigify import bone_class_instance, copy_bone_simple, add_pole_target_bone, add_stretch_to
from rna_prop_ui import rna_idprop_ui_prop_get

METARIG_NAMES = "shoulder", "arm", "forearm", "hand"


def metarig_template():
    bpy.ops.object.mode_set(mode='EDIT')
    obj = bpy.context.object
    arm = obj.data
    bone = arm.edit_bones.new('shoulder')
    bone.head[:] = 0.0000, -0.4515, 0.0000
    bone.tail[:] = 1.0000, -0.0794, 0.3540
    bone.roll = -0.2227
    bone.connected = False
    bone = arm.edit_bones.new('upper_arm')
    bone.head[:] = 1.1319, -0.0808, -0.0101
    bone.tail[:] = 3.0319, 0.2191, -0.1101
    bone.roll = 1.6152
    bone.connected = False
    bone.parent = arm.edit_bones['shoulder']
    bone = arm.edit_bones.new('forearm')
    bone.head[:] = 3.0319, 0.2191, -0.1101
    bone.tail[:] = 4.8319, -0.0809, -0.0242
    bone.roll = 1.5153
    bone.connected = True
    bone.parent = arm.edit_bones['upper_arm']
    bone = arm.edit_bones.new('hand')
    bone.head[:] = 4.8319, -0.0809, -0.0242
    bone.tail[:] = 5.7590, -0.1553, -0.1392
    bone.roll = -3.0083
    bone.connected = True
    bone.parent = arm.edit_bones['forearm']

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones['upper_arm']
    pbone['type'] = 'arm'


def metarig_definition(obj, orig_bone_name):
    mt = bone_class_instance(obj, METARIG_NAMES) # meta
    mt.arm = orig_bone_name
    mt.update()

    mt.shoulder_p = mt.arm_p.parent

    if not mt.shoulder_p:
        raise Exception("could not find 'arm' parent, skipping:", orig_bone_name)
    print(mt.shoulder_p)
    mt.shoulder = mt.shoulder_p.name

    # We could have some bones attached, find the bone that has this as its 2nd parent
    hands = []
    for pbone in obj.pose.bones:
        index = pbone.parent_index(mt.arm_p)
        if index == 2:
            hands.append(pbone)

    if len(hands) != 1:
        raise Exception("Expected more then 1 hand found on:", orig_bone_name)

    # first add the 2 new bones
    mt.hand_p = hands[0]
    mt.hand = mt.hand_p.name

    mt.forearm_p = mt.hand_p.parent
    mt.forearm = mt.forearm_p.name

    return mt.names()


def main(obj, definitions, base_names):
    """
    the bone with the 'arm' property is the upper arm, this assumes a chain as follows.
    [shoulder, upper_arm, forearm, hand]
    ...where this bone is 'upper_arm'

    there are 3 chains
    - Original
    - IK, MCH-%s_ik
    - IKSwitch, MCH-%s ()


    """

    # Since there are 3 chains, this gets confusing so divide into 3 chains
    # Initialize container classes for convenience
    mt = bone_class_instance(obj, METARIG_NAMES) # meta
    mt.shoulder, mt.arm, mt.forearm, mt.hand = definitions

    ik = bone_class_instance(obj, ["arm", "forearm", "pole", "hand"]) # ik
    sw = bone_class_instance(obj, ["socket", "shoulder", "arm", "forearm", "hand"]) # hinge
    ex = bone_class_instance(obj, ["arm_hinge"]) # hinge & extras

    arm = obj.data

    def chain_ik(prefix="MCH-%s_ik"):

        mt.update()

        # Add the edit bones
        ik.hand_e = copy_bone_simple(arm, mt.hand, prefix % base_names[mt.hand])
        ik.hand = ik.hand_e.name

        ik.arm_e = copy_bone_simple(arm, mt.arm, prefix % base_names[mt.arm])
        ik.arm = ik.arm_e.name

        ik.forearm_e = copy_bone_simple(arm, mt.forearm, prefix % base_names[mt.forearm])
        ik.forearm = ik.forearm_e.name

        ik.arm_e.parent = mt.arm_e.parent
        ik.forearm_e.connected = mt.arm_e.connected

        ik.forearm_e.parent = ik.arm_e
        ik.forearm_e.connected = True


        # Add the bone used for the arms poll target
        ik.pole = add_pole_target_bone(obj, mt.forearm, "elbow_poll", mode='+Z')

        bpy.ops.object.mode_set(mode='OBJECT')

        ik.update()

        con = ik.forearm_p.constraints.new('IK')
        con.target = obj
        con.subtarget = ik.hand
        con.pole_target = obj
        con.pole_subtarget = ik.pole

        con.use_tail = True
        con.use_stretch = True
        con.use_target = True
        con.use_rotation = False
        con.chain_length = 2
        con.pole_angle = -90.0 # XXX, RAD2DEG

        # ID Propery on the hand for IK/FK switch

        prop = rna_idprop_ui_prop_get(ik.hand_p, "ik", create=True)
        ik.hand_p["ik"] = 0.5
        prop["soft_min"] = 0.0
        prop["soft_max"] = 1.0

        bpy.ops.object.mode_set(mode='EDIT')

    def chain_switch(prefix="MCH-%s"):
        print(mt.obj.mode)
        sw.update()
        mt.update()

        sw.shoulder_e = copy_bone_simple(arm, mt.shoulder, prefix % base_names[mt.shoulder])
        sw.shoulder = sw.shoulder_e.name
        sw.shoulder_e.parent = mt.shoulder_e.parent
        sw.shoulder_e.connected = mt.shoulder_e.connected

        sw.arm_e = copy_bone_simple(arm, mt.arm, prefix % base_names[mt.arm])
        sw.arm = sw.arm_e.name
        sw.arm_e.parent = sw.shoulder_e
        sw.arm_e.connected = arm.edit_bones[mt.shoulder].connected

        sw.forearm_e = copy_bone_simple(arm, mt.forearm, prefix % base_names[mt.forearm])
        sw.forearm = sw.forearm_e.name
        sw.forearm_e.parent = sw.arm_e
        sw.forearm_e.connected = arm.edit_bones[mt.forearm].connected

        sw.hand_e = copy_bone_simple(arm, mt.hand, prefix % base_names[mt.hand])
        sw.hand = sw.hand_e.name
        sw.hand_e.parent = sw.forearm_e
        sw.hand_e.connected = arm.edit_bones[mt.hand].connected

        # The sw.hand_e needs to own all the children on the metarig's hand
        for child in mt.hand_e.children:
            child.parent = sw.hand_e


        # These are made the children of sw.shoulder_e


        bpy.ops.object.mode_set(mode='OBJECT')

        # Add constraints
        sw.update()

        #dummy, ik.arm, ik.forearm, ik.hand, ik.pole = ik_chain_tuple

        ik_driver_path = obj.pose.bones[ik.hand].path_to_id() + '["ik"]'

        def ik_fk_driver(con):
            '''
            3 bones use this for ik/fk switching
            '''
            fcurve = con.driver_add("influence", 0)
            driver = fcurve.driver
            tar = driver.targets.new()
            driver.type = 'AVERAGE'
            tar.name = "ik"
            tar.id_type = 'OBJECT'
            tar.id = obj
            tar.rna_path = ik_driver_path

        # ***********
        con = sw.arm_p.constraints.new('COPY_ROTATION')
        con.name = "FK"
        con.target = obj
        con.subtarget = mt.arm

        con = sw.arm_p.constraints.new('COPY_ROTATION')

        con.target = obj
        con.subtarget = ik.arm
        con.influence = 0.5
        ik_fk_driver(con)

        # ***********
        con = sw.forearm_p.constraints.new('COPY_ROTATION')
        con.name = "FK"
        con.target = obj
        con.subtarget = mt.forearm

        con = sw.forearm_p.constraints.new('COPY_ROTATION')
        con.name = "IK"
        con.target = obj
        con.subtarget = ik.forearm
        con.influence = 0.5
        ik_fk_driver(con)

        # ***********
        con = sw.hand_p.constraints.new('COPY_ROTATION')
        con.name = "FK"
        con.target = obj
        con.subtarget = mt.hand

        con = sw.hand_p.constraints.new('COPY_ROTATION')
        con.name = "IK"
        con.target = obj
        con.subtarget = ik.hand
        con.influence = 0.5
        ik_fk_driver(con)


        add_stretch_to(obj, sw.forearm, ik.pole, "VIS-elbow_ik_poll")
        add_stretch_to(obj, sw.hand, ik.hand, "VIS-hand_ik")

        bpy.ops.object.mode_set(mode='EDIT')

    def chain_shoulder(prefix="MCH-%s"):

        sw.socket_e = copy_bone_simple(arm, mt.arm, (prefix % base_names[mt.arm]) + "_socket")
        sw.socket = sw.socket_e.name
        sw.socket_e.tail = arm.edit_bones[mt.shoulder].tail


        # Set the shoulder as parent
        ik.update()
        sw.update()
        mt.update()

        sw.socket_e.parent = sw.shoulder_e
        ik.arm_e.parent = sw.shoulder_e


        # ***** add the shoulder hinge
        # yes this is correct, the shoulder copy gets the arm's name
        ex.arm_hinge_e = copy_bone_simple(arm, mt.shoulder, (prefix % base_names[mt.arm]) + "_hinge")
        ex.arm_hinge = ex.arm_hinge_e.name
        offset = ex.arm_hinge_e.length / 2.0

        ex.arm_hinge_e.head.y += offset
        ex.arm_hinge_e.tail.y += offset

        # Note: meta arm becomes child of hinge
        mt.arm_e.parent = ex.arm_hinge_e


        bpy.ops.object.mode_set(mode='OBJECT')

        ex.update()

        con = mt.arm_p.constraints.new('COPY_LOCATION')
        con.target = obj
        con.subtarget = sw.socket


        # Hinge constraint & driver
        con = ex.arm_hinge_p.constraints.new('COPY_ROTATION')
        con.name = "hinge"
        con.target = obj
        con.subtarget = sw.shoulder
        driver_fcurve = con.driver_add("influence", 0)
        driver = driver_fcurve.driver


        controller_path = mt.arm_p.path_to_id()
        # add custom prop
        mt.arm_p["hinge"] = 0.0
        prop = rna_idprop_ui_prop_get(mt.arm_p, "hinge", create=True)
        prop["soft_min"] = 0.0
        prop["soft_max"] = 1.0


        # *****
        driver = driver_fcurve.driver
        driver.type = 'AVERAGE'

        tar = driver.targets.new()
        tar.name = "hinge"
        tar.id_type = 'OBJECT'
        tar.id = obj
        tar.rna_path = controller_path + '["hinge"]'


        bpy.ops.object.mode_set(mode='EDIT')

        # remove the shoulder and re-parent

    chain_ik()
    chain_switch()
    chain_shoulder()

    # Shoulder with its delta and hinge.

    # TODO - return a list for fk and IK
    return None
