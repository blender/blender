# SPDX-FileCopyrightText: 2016-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy

from ...utils.bones import compute_chain_x_axis, align_bone_x_axis, align_bone_z_axis
from ...utils.bones import align_bone_to_axis, flip_bone
from ...utils.naming import make_derived_name
from ...utils.widgets_basic import create_circle_widget, create_limb_widget

from ..widgets import create_foot_widget, create_ball_socket_widget

from ...base_rig import stage

from .limb_rigs import BaseLimbRig


class Rig(BaseLimbRig):
    """Paw rig with an optional second heel control."""

    segmented_orgs = 3
    min_valid_orgs = 4
    max_valid_orgs = 5
    toe_bone_index = 3

    use_heel2: bool

    def initialize(self):
        self.use_heel2 = len(self.bones.org.main) > 4

        if self.use_heel2:
            self.toe_bone_index = 4
            self.fk_name_suffix_cutoff = 3
            self.fk_ik_layer_cutoff = 4

        super().initialize()

    def prepare_bones(self):
        orgs = self.bones.org.main

        foot_x = self.vector_without_z(self.get_bone(orgs[2]).y_axis).cross((0, 0, 1))

        if self.params.rotation_axis == 'automatic':
            axis = compute_chain_x_axis(self.obj, orgs[0:2])

            for bone in orgs:
                align_bone_x_axis(self.obj, bone, axis)

        elif self.params.auto_align_extremity:
            if self.main_axis == 'x':
                align_bone_x_axis(self.obj, orgs[2], foot_x)
                align_bone_x_axis(self.obj, orgs[3], -foot_x)
            else:
                align_bone_z_axis(self.obj, orgs[2], foot_x)
                align_bone_z_axis(self.obj, orgs[3], -foot_x)

    ####################################################
    # Utilities

    def align_ik_control_bone(self, name: str):
        if self.params.rotation_axis == 'automatic' or self.params.auto_align_extremity:
            align_bone_to_axis(self.obj, name, 'y', flip=True)

        else:
            flip_bone(self.obj, name)

            bone = self.get_bone(name)
            bone.tail[2] = bone.head[2]
            bone.roll = 0

    ####################################################
    # EXTRA BONES

    class CtrlBones(BaseLimbRig.CtrlBones):
        heel: str                      # Foot heel control
        heel2: str                     # Second foot heel control (optional)

    class MchBones(BaseLimbRig.MchBones):
        toe_socket: str                # IK toe orientation bone
        ik_heel2: str                  # Final position of heel2 in the IK output

    bones: BaseLimbRig.ToplevelBones[
        BaseLimbRig.OrgBones,
        'Rig.CtrlBones',
        'Rig.MchBones',
        list[str]
    ]

    ####################################################
    # IK controls

    def get_middle_ik_controls(self):
        return [self.bones.ctrl.heel] if self.use_heel2 else []

    def get_extra_ik_controls(self):
        extra = [self.bones.ctrl.heel2] if self.use_heel2 else [self.bones.ctrl.heel]
        return super().get_extra_ik_controls() + extra

    def make_ik_control_bone(self, orgs: list[str]):
        return self.make_paw_ik_control_bone(orgs[-2], orgs[-1], orgs[2])

    def make_paw_ik_control_bone(self, org_one: str, org_two: str, org_name: str):
        name = self.copy_bone(org_two, make_derived_name(org_name, 'ctrl', '_ik'))

        self.align_ik_control_bone(name)

        vec = self.get_bone(org_two).tail - self.get_bone(org_one).head
        self.get_bone(name).length = self.vector_without_z(vec).length

        return name

    def register_switch_parents(self, pbuilder):
        super().register_switch_parents(pbuilder)

        pbuilder.register_parent(self, self.bones.org.main[3], exclude_self=True, tags={'limb_end'})

    def make_ik_ctrl_widget(self, ctrl):
        create_foot_widget(self.obj, ctrl)

    ####################################################
    # Heel control

    @stage.generate_bones
    def make_heel_control_bone(self):
        org = self.bones.org.main[2]
        name = self.copy_bone(org, make_derived_name(org, 'ctrl', '_heel_ik'))
        self.bones.ctrl.heel = name

        flip_bone(self.obj, name)

    @stage.parent_bones
    def parent_heel_control_bone(self):
        if self.use_heel2:
            self.set_bone_parent(self.bones.ctrl.heel, self.bones.ctrl.heel2)
        else:
            self.set_bone_parent(self.bones.ctrl.heel, self.get_ik_control_output())

    @stage.configure_bones
    def configure_heel_control_bone(self):
        bone = self.get_bone(self.bones.ctrl.heel)
        bone.lock_location = True, True, True

    @stage.generate_widgets
    def generate_heel_control_widget(self):
        create_ball_socket_widget(self.obj, self.bones.ctrl.heel)

    ####################################################
    # Second Heel control

    @stage.generate_bones
    def make_heel2_control_bone(self):
        if self.use_heel2:
            org = self.bones.org.main[3]
            name = self.copy_bone(org, make_derived_name(org, 'ctrl', '_ik'))
            self.bones.ctrl.heel2 = name

            flip_bone(self.obj, name)

    @stage.parent_bones
    def parent_heel2_control_bone(self):
        if self.use_heel2:
            self.set_bone_parent(self.bones.ctrl.heel2, self.get_ik_control_output())

    @stage.configure_bones
    def configure_heel2_control_bone(self):
        if self.use_heel2:
            bone = self.get_bone(self.bones.ctrl.heel2)
            bone.lock_location = True, True, True

    @stage.generate_widgets
    def generate_heel2_control_widget(self):
        if self.use_heel2:
            create_ball_socket_widget(self.obj, self.bones.ctrl.heel2)

    ####################################################
    # FK control chain

    def make_fk_control_widget(self, i, ctrl):
        if i < self.toe_bone_index - 1:
            create_limb_widget(self.obj, ctrl)
        elif i == self.toe_bone_index - 1:
            create_circle_widget(self.obj, ctrl, radius=0.4, head_tail=0.0)
        else:
            create_circle_widget(self.obj, ctrl, radius=0.4, head_tail=0.5)

    ####################################################
    # FK parents MCH chain

    @stage.generate_bones
    def make_toe_socket_bone(self):
        org = self.bones.org.main[self.toe_bone_index]
        self.bones.mch.toe_socket = self.copy_bone(org, make_derived_name(org, 'mch', '_ik_socket'))

    @stage.parent_bones
    def parent_toe_socket_bone(self):
        self.set_bone_parent(self.bones.mch.toe_socket, self.get_ik_control_output())

    def parent_fk_parent_bone(self, i, parent_mch, prev_ctrl, org, prev_org):
        if i == self.toe_bone_index:
            self.set_bone_parent(parent_mch, prev_org, use_connect=True, inherit_scale='ALIGNED')

        else:
            super().parent_fk_parent_bone(i, parent_mch, prev_ctrl, org, prev_org)

    def rig_fk_parent_bone(self, i, parent_mch, org):
        if i == self.toe_bone_index:
            con = self.make_constraint(parent_mch, 'COPY_TRANSFORMS', self.bones.mch.toe_socket)

            self.make_driver(con, 'influence', variables=[(self.prop_bone, 'IK_FK')], polynomial=[1.0, -1.0])

        else:
            super().rig_fk_parent_bone(i, parent_mch, org)

    ####################################################
    # IK system MCH

    ik_input_head_tail = 1.0

    def get_ik_input_bone(self):
        return self.bones.ctrl.heel

    @stage.parent_bones
    def parent_ik_mch_chain(self):
        super().parent_ik_mch_chain()

        self.set_bone_parent(self.bones.mch.ik_target, self.bones.ctrl.heel)

    ####################################################
    # IK heel2 output

    def get_ik_output_chain(self):
        tail = [self.bones.mch.ik_heel2] if self.use_heel2 else []
        return super().get_ik_output_chain() + tail

    @stage.generate_bones
    def make_ik_heel2_bone(self):
        if self.use_heel2:
            orgs = self.bones.org.main
            self.bones.mch.ik_heel2 = self.copy_bone(orgs[3], make_derived_name(orgs[3], 'mch', '_ik_out'))

    @stage.parent_bones
    def parent_ik_heel2_bone(self):
        if self.use_heel2:
            self.set_bone_parent(self.bones.mch.ik_heel2, self.bones.ctrl.heel2)

    @stage.rig_bones
    def rig_ik_heel2_bone(self):
        if self.use_heel2:
            self.make_constraint(self.bones.mch.ik_heel2, 'COPY_LOCATION', self.bones.mch.ik_target, head_tail=1)

    ####################################################
    # Deform chain

    def rig_deform_bone(self, i, deform, entry, next_entry, tweak, next_tweak):
        super().rig_deform_bone(i, deform, entry, next_entry, tweak, next_tweak)

        if tweak and not (next_tweak or next_entry):
            self.make_constraint(deform, 'STRETCH_TO', entry.org, head_tail=1.0, keep_axis='SWING_Y')

    ####################################################
    # Settings

    @classmethod
    def parameters_ui(cls, layout, params, end='Claw'):
        super().parameters_ui(layout, params, end)


def create_sample(obj):
    # generated by rigify.utils.write_metarig
    bpy.ops.object.mode_set(mode='EDIT')
    arm = obj.data

    bones = {}

    bone = arm.edit_bones.new('thigh.L')
    bone.head[:] = 0.0000, 0.0017, 0.7379
    bone.tail[:] = 0.0000, -0.0690, 0.4731
    bone.roll = 0.0000
    bone.use_connect = False
    bones['thigh.L'] = bone.name
    bone = arm.edit_bones.new('shin.L')
    bone.head[:] = 0.0000, -0.0690, 0.4731
    bone.tail[:] = 0.0000, 0.1364, 0.2473
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['thigh.L']]
    bones['shin.L'] = bone.name
    bone = arm.edit_bones.new('foot.L')
    bone.head[:] = 0.0000, 0.1364, 0.2473
    bone.tail[:] = 0.0000, 0.0736, 0.0411
    bone.roll = -0.0002
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['shin.L']]
    bones['foot.L'] = bone.name
    bone = arm.edit_bones.new('toe.L')
    bone.head[:] = 0.0000, 0.0736, 0.0411
    bone.tail[:] = 0.0000, -0.0594, 0.0000
    bone.roll = -3.1416
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['foot.L']]
    bones['toe.L'] = bone.name

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones[bones['thigh.L']]
    pbone.rigify_type = 'limbs.paw'
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    try:
        pbone.rigify_parameters.limb_type = "paw"
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.ik_local_location = False
    except AttributeError:
        pass
    pbone = obj.pose.bones[bones['shin.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['foot.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['toe.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    try:
        pbone.rigify_parameters.limb_type = "paw"
    except AttributeError:
        pass

    bpy.ops.object.mode_set(mode='EDIT')
    for bone in arm.edit_bones:
        bone.select = False
        bone.select_head = False
        bone.select_tail = False
    for b in bones:
        bone = arm.edit_bones[bones[b]]
        bone.select = True
        bone.select_head = True
        bone.select_tail = True
        arm.edit_bones.active = bone
        if bcoll := arm.collections.active:
            bcoll.assign(bone)

    return bones
