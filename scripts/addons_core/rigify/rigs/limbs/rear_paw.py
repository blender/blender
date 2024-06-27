# SPDX-FileCopyrightText: 2017-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

from ...utils.bones import align_bone_roll
from ...utils.naming import make_derived_name
from ...utils.misc import map_list

from itertools import count

from ...base_rig import stage

from .limb_rigs import BaseLimbRig
from .paw import Rig as pawRig, create_sample as create_paw_sample


class Rig(pawRig):
    """Rear paw rig with special IK automation."""

    ####################################################
    # BONES

    class CtrlBones(pawRig.CtrlBones):
        pass

    class MchBones(pawRig.MchBones):
        ik2_target: str                # Three bone IK stretch limit
        ik2_chain: list[str]           # Second IK system (pre-driving thigh and ik3)
        ik3_chain: list[str]           # Third IK system (pre-driving heel)

    bones: pawRig.ToplevelBones[
        pawRig.OrgBones,
        'Rig.CtrlBones',
        'Rig.MchBones',
        list[str]
    ]

    ####################################################
    # IK controls

    def get_middle_ik_controls(self):
        return [self.bones.ctrl.heel]

    def get_ik_fk_position_chains(self):
        ik_chain, tail_chain, fk_chain = super().get_ik_fk_position_chains()
        assert not tail_chain
        if not self.use_heel2:
            return [*ik_chain, ik_chain[-1]], [], [*fk_chain, fk_chain[-1]]
        return ik_chain, tail_chain, fk_chain

    def get_extra_ik_controls(self):
        extra = [self.bones.ctrl.heel2] if self.use_heel2 else []
        return BaseLimbRig.get_extra_ik_controls(self) + extra

    def get_ik_pole_parents(self):
        return [(self.bones.mch.ik2_target, self.bones.ctrl.ik)]

    ####################################################
    # Heel control

    @stage.parent_bones
    def parent_heel_control_bone(self):
        self.set_bone_parent(self.bones.ctrl.heel, self.bones.mch.ik3_chain[-1])

    ####################################################
    # Second IK system (pre-driving thigh)

    use_mch_ik_base = True

    def get_ik2_input_bone(self):
        return self.bones.ctrl.heel2 if self.use_heel2 else self.bones.mch.toe_socket

    @stage.generate_bones
    def make_ik2_mch_stretch(self):
        orgs = self.bones.org.main

        self.bones.mch.ik2_target = self.make_ik2_mch_target_bone(orgs)

    def make_ik2_mch_target_bone(self, orgs: list[str]):
        return self.copy_bone(orgs[3], make_derived_name(orgs[0], 'mch', '_ik2_target'), scale=1/2)

    @stage.generate_bones
    def make_ik2_mch_chain(self):
        orgs = self.bones.org.main
        chain = map_list(self.make_ik2_mch_bone, count(0), orgs[0:2])
        self.bones.mch.ik2_chain = chain

        org_bones = map_list(self.get_bone, orgs)
        chain_bones = map_list(self.get_bone, chain)

        # Extend the base IK control (used in the ik2 chain) with the projected length of org2
        chain_bones[0].length += org_bones[2].vector.dot(chain_bones[0].vector.normalized())
        chain_bones[1].head = chain_bones[0].tail
        chain_bones[1].tail = org_bones[2].tail
        align_bone_roll(self.obj, chain[1], orgs[1])

    def make_ik2_mch_bone(self, _i: int, org: str):
        return self.copy_bone(org, make_derived_name(org, 'mch', '_ik2'))

    @stage.parent_bones
    def parent_ik2_mch_chain(self):
        mch = self.bones.mch
        self.set_bone_parent(mch.ik2_target, self.get_ik2_input_bone())
        self.set_bone_parent(mch.ik2_chain[0], self.bones.ctrl.ik_base, inherit_scale='AVERAGE')
        self.parent_bone_chain(mch.ik2_chain, use_connect=True)

    @stage.configure_bones
    def configure_ik2_mch_chain(self):
        for i, mch in enumerate(self.bones.mch.ik2_chain):
            self.configure_ik2_mch_bone(i, mch)

    def configure_ik2_mch_bone(self, i: int, mch: str):
        bone = self.get_bone(mch)
        bone.ik_stretch = 0.1
        if i == 1:
            bone.lock_ik_x = bone.lock_ik_y = bone.lock_ik_z = True
            setattr(bone, 'lock_ik_' + self.main_axis, False)

    @stage.rig_bones
    def rig_ik2_mch_chain(self):
        mch = self.bones.mch
        input_bone = self.get_ik2_input_bone()
        head_tail = 1 if self.use_heel2 else 0

        self.rig_ik_mch_stretch_limit(mch.ik2_target, mch.follow, input_bone, head_tail, 3)
        self.rig_ik_mch_end_bone(mch.ik2_chain[-1], mch.ik2_target, self.bones.ctrl.ik_pole)

    ####################################################
    # Third IK system (pre-driving heel control)

    @stage.generate_bones
    def make_ik3_mch_chain(self):
        self.bones.mch.ik3_chain = map_list(self.make_ik3_mch_bone, count(0), self.bones.org.main[1:3])

    def make_ik3_mch_bone(self, _i: int, org: str):
        return self.copy_bone(org, make_derived_name(org, 'mch', '_ik3'))

    @stage.parent_bones
    def parent_ik3_mch_chain(self):
        mch = self.bones.mch

        self.set_bone_parent(mch.ik3_chain[0], mch.ik2_chain[0])
        self.parent_bone_chain(mch.ik3_chain, use_connect=True)

    @stage.configure_bones
    def configure_ik3_mch_chain(self):
        for i, mch in enumerate(self.bones.mch.ik3_chain):
            self.configure_ik3_mch_bone(i, mch)

    def configure_ik3_mch_bone(self, i: int, mch: str):
        bone = self.get_bone(mch)
        bone.ik_stretch = 0.1
        if i == 0:
            bone.lock_ik_x = bone.lock_ik_y = bone.lock_ik_z = True
            setattr(bone, 'lock_ik_' + self.main_axis, False)

    @stage.rig_bones
    def rig_ik3_mch_chain(self):
        mch = self.bones.mch
        # Mostly cancel ik2 scaling.
        self.make_constraint(
            mch.ik3_chain[0], 'COPY_SCALE', self.bones.ctrl.ik_base,
            use_make_uniform=True, influence=0.75,
        )
        self.make_constraint(mch.ik3_chain[-1], 'IK', mch.ik2_target, chain_count=2)


def create_sample(obj):
    bones = create_paw_sample(obj)
    pbone = obj.pose.bones[bones['thigh.L']]
    pbone.rigify_type = 'limbs.rear_paw'
    return bones
