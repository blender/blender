# SPDX-FileCopyrightText: 2019-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
import math

from itertools import count, repeat
from mathutils import Matrix

from ...utils.layers import ControlLayersOption
from ...utils.naming import strip_org, make_mechanism_name, make_derived_name
from ...utils.bones import (put_bone, align_bone_to_axis, align_bone_orientation,
                            set_bone_widget_transform, TypedBoneDict)
from ...utils.widgets import adjust_widget_transform_mesh
from ...utils.widgets_basic import create_circle_widget
from ...utils.misc import map_list

from ...base_rig import stage
from .spine_rigs import BaseSpineRig


class Rig(BaseSpineRig):
    """
    Spine rig with fixed pivot, hip/chest controls and tweaks.
    """

    pivot_pos: int
    use_fk: bool

    def initialize(self):
        super().initialize()

        # Check if user provided the pivot position
        self.pivot_pos = self.params.pivot_pos
        self.use_fk = self.params.make_fk_controls

        if not (0 < self.pivot_pos < len(self.bones.org)):
            self.raise_error("Please specify a valid pivot bone position.")

    ####################################################
    # BONES

    class SplitChainBones(TypedBoneDict):
        chest: list[str]
        hips: list[str]

    class CtrlBones(BaseSpineRig.CtrlBones):
        hips: str                      # Hip control
        chest: str                     # Chest control
        fk: 'Rig.SplitChainBones'      # FK controls

    class MchBones(BaseSpineRig.MchBones):
        pivot: str                     # Central pivot between sub-chains
        chain: 'Rig.SplitChainBones'   # Tweak parents, distributing master deform.
        wgt_hips: str                  # Hip widget position bone.
        wgt_chest: str                 # Chest widget position bone.

    bones: BaseSpineRig.ToplevelBones[
        list[str],
        'Rig.CtrlBones',
        'Rig.MchBones',
        list[str]
    ]

    ####################################################
    # Master control bone

    def get_master_control_pos(self, orgs: list[str]):
        base_bone = self.get_bone(orgs[0])
        return (base_bone.head + base_bone.tail) / 2

    ####################################################
    # Main control bones

    @stage.generate_bones
    def make_end_control_bones(self):
        orgs = self.bones.org
        pivot = self.pivot_pos

        self.bones.ctrl.hips = self.make_hips_control_bone(orgs[pivot-1], 'hips')
        self.bones.ctrl.chest = self.make_chest_control_bone(orgs[pivot], 'chest')

    def make_hips_control_bone(self, org: str, name: str):
        name = self.copy_bone(org, name, parent=False)
        align_bone_to_axis(self.obj, name, 'y', length=self.length / 4, flip=True)
        return name

    def make_chest_control_bone(self, org: str, name: str):
        name = self.copy_bone(org, name, parent=False)
        align_bone_to_axis(self.obj, name, 'y', length=self.length / 3)
        return name

    @stage.parent_bones
    def parent_end_control_bones(self):
        ctrl = self.bones.ctrl
        pivot = self.get_master_control_output()
        self.set_bone_parent(ctrl.hips, pivot)
        self.set_bone_parent(ctrl.chest, pivot)

    @stage.generate_widgets
    def make_end_control_widgets(self):
        ctrl = self.bones.ctrl
        mch = self.bones.mch
        self.make_end_control_widget(ctrl.hips, mch.wgt_hips)
        self.make_end_control_widget(ctrl.chest, mch.wgt_chest)

    def make_end_control_widget(self, ctrl: str, wgt_mch: str):
        shape_bone = self.get_bone(wgt_mch)
        is_horizontal = abs(shape_bone.z_axis.normalized().y) < 0.7

        set_bone_widget_transform(self.obj, ctrl, wgt_mch)

        obj = create_circle_widget(
            self.obj, ctrl,
            radius=1.2 if is_horizontal else 1.1,
            head_tail=0.0,
            head_tail_x=1.0,
            with_line=False,
        )

        if is_horizontal:
            # Tilt the widget toward the ground for horizontal (animal) spines
            angle = math.copysign(28, shape_bone.x_axis.x)
            rot_mat = Matrix.Rotation(math.radians(angle), 4, 'X')
            adjust_widget_transform_mesh(obj, rot_mat, local=True)

    ####################################################
    # FK control bones

    fk_result: 'Rig.SplitChainBones'  # ctrl.fk or mch.chain depending on use_fk

    @stage.generate_bones
    def make_control_chain(self):
        if self.use_fk:
            orgs = self.bones.org
            self.bones.ctrl.fk = self.fk_result = self.SplitChainBones(
                hips=map_list(self.make_control_bone,
                              count(0), orgs[0:self.pivot_pos], repeat(True)),
                chest=map_list(self.make_control_bone,
                               count(self.pivot_pos), orgs[self.pivot_pos:], repeat(False)),
            )

    # noinspection PyMethodOverriding
    def make_control_bone(self, i: int, org: str, is_hip: bool):
        name = self.copy_bone(org, make_derived_name(org, 'ctrl', '_fk'), parent=False)
        if is_hip:
            put_bone(self.obj, name, self.get_bone(name).tail)
        return name

    @stage.parent_bones
    def parent_control_chain(self):
        if self.use_fk:
            chain = self.bones.mch.chain
            fk = self.bones.ctrl.fk
            for child, parent in zip(fk.hips, chain.hips):
                self.set_bone_parent(child, parent)
            for child, parent in zip(fk.chest, chain.chest):
                self.set_bone_parent(child, parent)

    @stage.configure_bones
    def configure_control_chain(self):
        if self.use_fk:
            fk = self.bones.ctrl.fk
            for args in zip(count(0), fk.hips + fk.chest, self.bones.org):
                self.configure_control_bone(*args)

            ControlLayersOption.FK.assign_rig(self, fk.hips + fk.chest)

    @stage.generate_widgets
    def make_control_widgets(self):
        if self.use_fk:
            fk = self.bones.ctrl.fk
            for ctrl in fk.hips:
                self.make_control_widget(ctrl, True)
            for ctrl in fk.chest:
                self.make_control_widget(ctrl, False)

    def make_control_widget(self, ctrl: str, is_hip: bool):
        obj = create_circle_widget(self.obj, ctrl, radius=1.0, head_tail=0.5)
        if is_hip:
            adjust_widget_transform_mesh(obj, Matrix.Diagonal((1, -1, 1, 1)), local=True)

    ####################################################
    # MCH bones associated with main controls

    @stage.generate_bones
    def make_mch_control_bones(self):
        orgs = self.bones.org
        mch = self.bones.mch

        mch.pivot = self.make_mch_pivot_bone(orgs[self.pivot_pos], 'pivot')
        mch.wgt_hips = self.make_mch_widget_bone(orgs[0], 'WGT-hips')
        mch.wgt_chest = self.make_mch_widget_bone(orgs[-1], 'WGT-chest')

    def make_mch_pivot_bone(self, org: str, name: str):
        name = self.copy_bone(org, make_mechanism_name(name), parent=False)
        align_bone_to_axis(self.obj, name, 'y', length=self.length * 0.6 / 4)
        return name

    def make_mch_widget_bone(self, org: str, name: str):
        return self.copy_bone(org, make_mechanism_name(name), parent=False)

    @stage.parent_bones
    def parent_mch_control_bones(self):
        mch = self.bones.mch
        fk = self.fk_result
        self.set_bone_parent(mch.pivot, fk.chest[0])
        self.set_bone_parent(mch.wgt_hips, fk.hips[0])
        self.set_bone_parent(mch.wgt_chest, fk.chest[-1])
        align_bone_orientation(self.obj, mch.pivot, fk.hips[-1])

    @stage.rig_bones
    def rig_mch_control_bones(self):
        mch = self.bones.mch
        self.make_constraint(mch.pivot, 'COPY_TRANSFORMS', self.fk_result.hips[-1], influence=0.5)

    ####################################################
    # MCH chain for distributing hip & chest transform

    @stage.generate_bones
    def make_mch_chain(self):
        orgs = self.bones.org
        self.bones.mch.chain = self.SplitChainBones(
            hips=map_list(self.make_mch_bone, orgs[0:self.pivot_pos], repeat(True)),
            chest=map_list(self.make_mch_bone, orgs[self.pivot_pos:], repeat(False)),
        )
        if not self.use_fk:
            self.fk_result = self.bones.mch.chain

    def make_mch_bone(self, org: str, is_hip: bool):
        name = self.copy_bone(org, make_mechanism_name(strip_org(org)), parent=False)
        align_bone_to_axis(self.obj, name, 'y', length=self.length / 10, flip=is_hip)
        return name

    @stage.parent_bones
    def parent_mch_chain(self):
        master = self.get_master_control_output()
        chain = self.bones.mch.chain
        fk = self.fk_result
        for child, parent in zip(reversed(chain.hips), [master, *reversed(fk.hips)]):
            self.set_bone_parent(child, parent)
        for child, parent in zip(chain.chest, [master, *fk.chest]):
            self.set_bone_parent(child, parent)

    @stage.rig_bones
    def rig_mch_chain(self):
        ctrl = self.bones.ctrl
        chain = self.bones.mch.chain
        for mch in chain.hips:
            self.rig_mch_bone(mch, ctrl.hips, len(chain.hips))
        for mch in chain.chest:
            self.rig_mch_bone(mch, ctrl.chest, len(chain.chest))

    def rig_mch_bone(self, mch: str, control: str, chain_len: int):
        self.make_constraint(mch, 'COPY_TRANSFORMS', control,
                             space='LOCAL', influence=1 / chain_len)

    ####################################################
    # Tweak bones

    @stage.parent_bones
    def parent_tweak_chain(self):
        mch = self.bones.mch
        chain = self.fk_result
        parents = [chain.hips[0], *chain.hips[0:-1], mch.pivot, *chain.chest[1:], chain.chest[-1]]
        for args in zip(self.bones.ctrl.tweak, parents):
            self.set_bone_parent(*args)

    ####################################################
    # SETTINGS

    @classmethod
    def add_parameters(cls, params):
        params.pivot_pos = bpy.props.IntProperty(
            name='pivot_position',
            default=2,
            min=0,
            description='Position of the torso control and pivot point'
        )

        super().add_parameters(params)

        params.make_fk_controls = bpy.props.BoolProperty(
            name="FK Controls", default=True,
            description="Generate an FK control chain"
        )

        ControlLayersOption.FK.add_parameters(params)

    @classmethod
    def parameters_ui(cls, layout, params):
        r = layout.row()
        r.prop(params, "pivot_pos")

        super().parameters_ui(layout, params)

        layout.prop(params, 'make_fk_controls')

        if params.make_fk_controls:
            ControlLayersOption.FK.parameters_ui(layout, params)


def create_sample(obj):
    # generated by rigify.utils.write_metarig
    bpy.ops.object.mode_set(mode='EDIT')
    arm = obj.data

    bones = {}

    bone = arm.edit_bones.new('spine')
    bone.head[:] = 0.0000, 0.0552, 1.0099
    bone.tail[:] = 0.0000, 0.0172, 1.1573
    bone.roll = 0.0000
    bone.use_connect = False
    bones['spine'] = bone.name

    bone = arm.edit_bones.new('spine.001')
    bone.head[:] = 0.0000, 0.0172, 1.1573
    bone.tail[:] = 0.0000, 0.0004, 1.2929
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['spine']]
    bones['spine.001'] = bone.name

    bone = arm.edit_bones.new('spine.002')
    bone.head[:] = 0.0000, 0.0004, 1.2929
    bone.tail[:] = 0.0000, 0.0059, 1.4657
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['spine.001']]
    bones['spine.002'] = bone.name

    bone = arm.edit_bones.new('spine.003')
    bone.head[:] = 0.0000, 0.0059, 1.4657
    bone.tail[:] = 0.0000, 0.0114, 1.6582
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['spine.002']]
    bones['spine.003'] = bone.name

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones[bones['spine']]
    pbone.rigify_type = 'spines.basic_spine'
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'

    pbone = obj.pose.bones[bones['spine.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['spine.002']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['spine.003']]
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

    return bones
