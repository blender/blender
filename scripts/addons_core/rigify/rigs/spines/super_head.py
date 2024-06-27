# SPDX-FileCopyrightText: 2019-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy

from itertools import count

from ...utils.naming import make_derived_name
from ...utils.bones import align_bone_orientation
from ...utils.widgets_basic import create_circle_widget
from ...utils.widgets_special import create_neck_bend_widget, create_neck_tweak_widget
from ...utils.switch_parent import SwitchParentBuilder
from ...utils.misc import map_list

from ...base_rig import stage

from .spine_rigs import BaseHeadTailRig


class Rig(BaseHeadTailRig):
    """
    Head rig with long neck support and connect option.
    """

    use_connect_reverse = False
    min_chain_length = 1

    long_neck: bool
    has_neck: bool

    def initialize(self):
        super().initialize()

        self.long_neck = len(self.bones.org) > 3
        self.has_neck = len(self.bones.org) > 1

    ####################################################
    # BONES

    class CtrlBones(BaseHeadTailRig.CtrlBones):
        neck: str                      # Main neck control
        head: str                      # Main head control
        neck_bend: str                 # Extra neck bend control for long neck

    class MchBones(BaseHeadTailRig.MchBones):
        rot_neck: str                  # Main neck control parent for FK follow
        rot_head: str                  # Main head control parent for FK follow
        stretch: str                   # Long neck stretch helper
        ik: list[str]                  # Long neck IK system
        chain: list[str]               # Tweak parents

    bones: BaseHeadTailRig.ToplevelBones[
        list[str],
        'Rig.CtrlBones',
        'Rig.MchBones',
        list[str]
    ]

    ####################################################
    # Main control bones

    @stage.generate_bones
    def make_control_chain(self):
        orgs = self.bones.org
        ctrl = self.bones.ctrl

        if self.has_neck:
            ctrl.neck = self.make_neck_control_bone(orgs[0], 'neck', orgs[-1])

        ctrl.head = self.make_head_control_bone(orgs[-1], 'head')

        if self.long_neck:
            ctrl.neck_bend = self.make_neck_bend_control_bone(orgs[0], 'neck_bend', ctrl.neck)

        self.default_prop_bone = ctrl.head

    def make_neck_control_bone(self, org: str, name: str, org_head: str):
        name = self.copy_bone(org, name, parent=False)

        # Neck spans all neck bones (except head)
        self.get_bone(name).tail = self.get_bone(org_head).head

        return name

    def make_neck_bend_control_bone(self, org: str, name: str, neck: str):
        name = self.copy_bone(org, name, parent=False)
        neck_bend_eb = self.get_bone(name)

        # Neck pivot position
        neck_bones = self.bones.org
        if (len(neck_bones)-1) % 2:     # odd num of neck bones (head excluded)
            center_bone = self.get_bone(neck_bones[int((len(neck_bones))/2) - 1])
            neck_bend_eb.head = (center_bone.head + center_bone.tail)/2
        else:
            center_bone = self.get_bone(neck_bones[int((len(neck_bones)-1)/2) - 1])
            neck_bend_eb.head = center_bone.tail

        align_bone_orientation(self.obj, name, neck)
        neck_bend_eb.length = self.get_bone(neck).length / 2

        return name

    def make_head_control_bone(self, org: str, name: str):
        return self.copy_bone(org, name, parent=False)

    @stage.parent_bones
    def parent_control_chain(self):
        ctrl = self.bones.ctrl
        mch = self.bones.mch
        if self.has_neck:
            self.set_bone_parent(ctrl.neck, mch.rot_neck)
        self.set_bone_parent(ctrl.head, mch.rot_head)
        if self.long_neck:
            self.set_bone_parent(ctrl.neck_bend, mch.stretch)

    @stage.configure_bones
    def configure_control_chain(self):
        if self.has_neck:
            self.configure_control_bone(0, self.bones.ctrl.neck, self.bones.org[0])
        self.configure_control_bone(2, self.bones.ctrl.head, self.bones.org[-1])
        if self.long_neck:
            self.configure_neck_bend_bone(self.bones.ctrl.neck_bend, self.bones.org[0])

    def configure_neck_bend_bone(self, ctrl: str, _org: str):
        bone = self.get_bone(ctrl)
        bone.lock_rotation = (True, True, True)
        bone.lock_rotation_w = True
        bone.lock_scale = (True, True, True)

    @stage.generate_widgets
    def make_control_widgets(self):
        ctrl = self.bones.ctrl
        if self.has_neck:
            self.make_neck_widget(ctrl.neck)
        self.make_head_widget(ctrl.head)
        if self.long_neck:
            self.make_neck_bend_widget(ctrl.neck_bend)

    def make_neck_widget(self, ctrl: str):
        radius = 1/max(1, len(self.bones.mch.chain))

        create_circle_widget(
            self.obj, ctrl,
            radius=radius,
            head_tail=0.5,
        )

    def make_neck_bend_widget(self, ctrl: str):
        radius = 1/max(1, len(self.bones.mch.chain))

        create_neck_bend_widget(
            self.obj, ctrl,
            radius=radius/2,
            head_tail=0.0,
        )

    def make_head_widget(self, ctrl: str):
        # place wgt @ middle of head bone for long necks
        if self.long_neck:
            head_tail = 0.5
        else:
            head_tail = 1.0

        create_circle_widget(
            self.obj, ctrl,
            radius=0.5,
            head_tail=head_tail,
            with_line=False,
        )

    ####################################################
    # MCH bones associated with main controls

    @stage.generate_bones
    def make_mch_control_bones(self):
        orgs = self.bones.org
        mch = self.bones.mch

        if self.has_neck:
            mch.rot_neck = self.make_mch_follow_bone(orgs[0], 'neck', 0.5, copy_scale=True)
            mch.stretch = self.make_mch_stretch_bone(orgs[0], 'STR-neck', orgs[-1])
        mch.rot_head = self.make_mch_follow_bone(orgs[-1], 'head', 0.0, copy_scale=True)

    def make_mch_stretch_bone(self, org: str, name: str, org_head: str):
        name = self.copy_bone(org, make_derived_name(name, 'mch'), parent=False)
        self.get_bone(name).tail = self.get_bone(org_head).head
        return name

    @stage.parent_bones
    def parent_mch_control_bones(self):
        if self.has_neck:
            self.set_bone_parent(self.bones.mch.rot_neck, self.rig_parent_bone)
            self.set_bone_parent(self.bones.mch.rot_head, self.bones.ctrl.neck)
            self.set_bone_parent(self.bones.mch.stretch, self.bones.ctrl.neck)
        else:
            self.set_bone_parent(self.bones.mch.rot_head, self.rig_parent_bone)

    @stage.rig_bones
    def rig_mch_control_bones(self):
        if self.has_neck:
            self.rig_mch_stretch_bone(self.bones.mch.stretch, self.bones.ctrl.head)

    def rig_mch_stretch_bone(self, mch: str, head: str):
        self.make_constraint(mch, 'STRETCH_TO', head, keep_axis='SWING_Y')

    ####################################################
    # MCH IK chain for the long neck

    @stage.generate_bones
    def make_mch_ik_chain(self):
        orgs = self.bones.org
        if self.long_neck:
            self.bones.mch.ik = map_list(self.make_mch_ik_bone, orgs[0:-1])

    def make_mch_ik_bone(self, org: str):
        return self.copy_bone(org, make_derived_name(org, 'mch', '_ik'), parent=False)

    @stage.parent_bones
    def parent_mch_ik_chain(self):
        if self.long_neck:
            ik = self.bones.mch.ik
            self.set_bone_parent(ik[0], self.bones.ctrl.tweak[0])
            self.parent_bone_chain(ik, use_connect=True)

    @stage.rig_bones
    def rig_mch_ik_chain(self):
        if self.long_neck:
            ik = self.bones.mch.ik
            head = self.bones.ctrl.head
            for args in zip(count(0), ik):
                self.rig_mch_ik_bone(*args, len(ik), head)

    def rig_mch_ik_bone(self, i: int, mch: str, ik_len: int, head: str):
        if i == ik_len - 1:
            self.make_constraint(mch, 'IK', head, chain_count=ik_len)

        self.get_bone(mch).ik_stretch = 0.1

    ####################################################
    # MCH chain for the middle of the neck

    @stage.generate_bones
    def make_mch_chain(self):
        orgs = self.bones.org
        self.bones.mch.chain = map_list(self.make_mch_bone, orgs[1:-1])

    def make_mch_bone(self, org: str):
        return self.copy_bone(org, make_derived_name(org, 'mch'), parent=False, scale=1/4)

    @stage.parent_bones
    def align_mch_chain(self):
        for mch in self.bones.mch.chain:
            align_bone_orientation(self.obj, mch, self.bones.ctrl.neck)

    @stage.parent_bones
    def parent_mch_chain(self):
        mch = self.bones.mch
        for bone in mch.chain:
            self.set_bone_parent(bone, mch.stretch, inherit_scale='NONE')

    @stage.rig_bones
    def rig_mch_chain(self):
        chain = self.bones.mch.chain
        if self.long_neck:
            ik = self.bones.mch.ik
            for args in zip(count(0), chain, ik[1:]):
                self.rig_mch_bone_long(*args, len(chain))
        else:
            for args in zip(count(0), chain):
                self.rig_mch_bone(*args, len(chain))

    def rig_mch_bone_long(self, i: int, mch: str, ik: str, len_mch: int):
        ctrl = self.bones.ctrl

        self.make_constraint(mch, 'COPY_LOCATION', ik)

        step = 2/(len_mch+1)
        x_val = (i+1)*step
        influence = 2*x_val - x_val**2    # parabolic influence of pivot

        self.make_constraint(
            mch, 'COPY_LOCATION', ctrl.neck_bend,
            influence=influence, use_offset=True, space='LOCAL'
        )

        self.make_constraint(mch, 'COPY_SCALE', ctrl.neck)

    def rig_mch_bone(self, i, mch, len_mch):
        ctrl = self.bones.ctrl

        n_factor = float((i + 1) / (len_mch + 1))
        self.make_constraint(
            mch, 'COPY_ROTATION', ctrl.head,
            influence=n_factor, space='LOCAL'
        )

        self.make_constraint(mch, 'COPY_SCALE', ctrl.neck)

    ####################################################
    # Tweak bones

    @stage.generate_bones
    def make_tweak_chain(self):
        orgs = self.bones.org
        self.bones.ctrl.tweak = map_list(self.make_tweak_bone, count(0), orgs[0:-1])
        if not self.has_neck:
            self.check_connect_tweak(orgs[0])

    @stage.parent_bones
    def parent_tweak_chain(self):
        ctrl = self.bones.ctrl
        mch = self.bones.mch

        if self.has_neck:
            for args in zip(ctrl.tweak, [ctrl.neck, *mch.chain]):
                self.set_bone_parent(*args)
        elif self.connected_tweak:
            self.set_bone_parent(self.connected_tweak, ctrl.head)

    @stage.rig_bones
    def generate_neck_tweak_widget(self):
        # Generate the widget early to override connected parent
        if self.long_neck:
            bone = self.bones.ctrl.tweak[0]
            create_neck_tweak_widget(self.obj, bone, size=1.0)

    ####################################################
    # ORG and DEF bones

    @stage.generate_bones
    def register_parent_bones(self):
        rig = self.rigify_parent or self
        builder = SwitchParentBuilder(self.generator)
        builder.register_parent(
            self, self.bones.org[-1], name='Head',
            inject_into=rig, exclude_self=True, tags={'head'},
        )

    @stage.configure_bones
    def configure_bbone_chain(self):
        self.get_bone(self.bones.deform[-1]).bone.bbone_segments = 1

    @stage.rig_bones
    def rig_org_chain(self):
        if self.has_neck:
            tweaks = self.bones.ctrl.tweak + [self.bones.ctrl.head]
        else:
            tweaks = [self.connected_tweak or self.bones.ctrl.head]

        for args in zip(count(0), self.bones.org, tweaks, [*tweaks[1:], None]):
            self.rig_org_bone(*args)


def create_sample(obj, *, parent=None):
    # generated by rigify.utils.write_metarig
    bpy.ops.object.mode_set(mode='EDIT')
    arm = obj.data

    bones = {}

    bone = arm.edit_bones.new('neck')
    bone.head[:] = 0.0000, 0.0114, 1.6582
    bone.tail[:] = 0.0000, -0.0130, 1.7197
    bone.roll = 0.0000
    bone.use_connect = False
    if parent:
        bone.parent = arm.edit_bones[parent]
    bones['neck'] = bone.name
    bone = arm.edit_bones.new('neck.001')
    bone.head[:] = 0.0000, -0.0130, 1.7197
    bone.tail[:] = 0.0000, -0.0247, 1.7813
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['neck']]
    bones['neck.001'] = bone.name
    bone = arm.edit_bones.new('head')
    bone.head[:] = 0.0000, -0.0247, 1.7813
    bone.tail[:] = 0.0000, -0.0247, 1.9796
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['neck.001']]
    bones['head'] = bone.name

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones[bones['neck']]
    pbone.rigify_type = 'spines.super_head'
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    try:
        pbone.rigify_parameters.connect_chain = bool(parent)
    except AttributeError:
        pass
    pbone = obj.pose.bones[bones['neck.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['head']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'

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
