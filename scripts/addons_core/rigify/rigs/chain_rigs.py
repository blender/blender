# SPDX-FileCopyrightText: 2019-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy

from typing import Optional
from itertools import count

from bpy.types import PoseBone

from ..utils.rig import connected_children_names
from ..utils.naming import strip_org, make_derived_name
from ..utils.bones import (put_bone, flip_bone, flip_bone_chain, is_same_position,
                           is_connected_position)
from ..utils.bones import copy_bone_position, connect_bbone_chain_handles
from ..utils.widgets_basic import create_bone_widget, create_sphere_widget
from ..utils.misc import map_list

from ..base_rig import BaseRig, stage


class SimpleChainRig(BaseRig):
    """A rig that consists of 3 connected chains of control, org and deform bones."""
    def find_org_bones(self, bone: PoseBone):
        return [bone.name] + connected_children_names(self.obj, bone.name)

    min_chain_length = 2
    bbone_segments = None

    rig_parent_bone: str  # Bone to be used as parent of the whole rig

    def initialize(self):
        if len(self.bones.org) < self.min_chain_length:
            self.raise_error(
                "Input to rig type must be a chain of {} or more bones.", self.min_chain_length)

    def parent_bones(self):
        self.rig_parent_bone = self.get_bone_parent(self.bones.org[0])

    ##############################
    # BONES

    class CtrlBones(BaseRig.CtrlBones):
        fk: list[str]                  # FK control chain

    class MchBones(BaseRig.MchBones):
        pass

    bones: BaseRig.ToplevelBones[
        list[str],
        'SimpleChainRig.CtrlBones',
        'SimpleChainRig.MchBones',
        list[str]
    ]

    ##############################
    # Control chain

    @stage.generate_bones
    def make_control_chain(self):
        self.bones.ctrl.fk = map_list(self.make_control_bone, count(0), self.bones.org)

    def make_control_bone(self, i: int, org: str):
        return self.copy_bone(org, make_derived_name(org, 'ctrl'), parent=True)

    @stage.parent_bones
    def parent_control_chain(self):
        self.parent_bone_chain(self.bones.ctrl.fk, use_connect=True)

    @stage.configure_bones
    def configure_control_chain(self):
        for args in zip(count(0), self.bones.ctrl.fk, self.bones.org):
            self.configure_control_bone(*args)

    def configure_control_bone(self, i: int, ctrl: str, org: str):
        self.copy_bone_properties(org, ctrl)

    @stage.generate_widgets
    def make_control_widgets(self):
        for args in zip(count(0), self.bones.ctrl.fk):
            self.make_control_widget(*args)

    def make_control_widget(self, i: int, ctrl: str):
        create_bone_widget(self.obj, ctrl)

    ##############################
    # ORG chain

    @stage.parent_bones
    def parent_org_chain(self):
        pass

    @stage.rig_bones
    def rig_org_chain(self):
        for args in zip(count(0), self.bones.org, self.bones.ctrl.fk):
            self.rig_org_bone(*args)

    def rig_org_bone(self, i: int, org: str, ctrl: str):
        self.make_constraint(org, 'COPY_TRANSFORMS', ctrl)

    ##############################
    # Deform chain

    @stage.generate_bones
    def make_deform_chain(self):
        self.bones.deform = map_list(self.make_deform_bone, count(0), self.bones.org)

    def make_deform_bone(self, i: int, org: str):
        name = self.copy_bone(org, make_derived_name(org, 'def'), parent=True, bbone=True)
        if self.bbone_segments:
            self.get_bone(name).bbone_segments = self.bbone_segments
        return name

    @stage.parent_bones
    def parent_deform_chain(self):
        self.parent_bone_chain(self.bones.deform, use_connect=True)

    @stage.rig_bones
    def rig_deform_chain(self):
        for args in zip(count(0), self.bones.deform, self.bones.org):
            self.rig_deform_bone(*args)

    def rig_deform_bone(self, i: int, deform: str, org: str):
        self.make_constraint(deform, 'COPY_TRANSFORMS', org)


class TweakChainRig(SimpleChainRig):
    """A rig that adds tweak controls to the triple chain."""

    ##############################
    # BONES

    class CtrlBones(SimpleChainRig.CtrlBones):
        tweak: list[str]               # Tweak control chain

    class MchBones(SimpleChainRig.MchBones):
        pass

    bones: BaseRig.ToplevelBones[
        list[str],
        'TweakChainRig.CtrlBones',
        'TweakChainRig.MchBones',
        list[str]
    ]

    ##############################
    # Tweak chain

    @stage.generate_bones
    def make_tweak_chain(self):
        orgs = self.bones.org
        self.bones.ctrl.tweak = map_list(self.make_tweak_bone, count(0), orgs + orgs[-1:])

    def make_tweak_bone(self, i: int, org: str):
        name = self.copy_bone(org, 'tweak_' + strip_org(org), parent=False, scale=0.5)

        if i == len(self.bones.org):
            put_bone(self.obj, name, self.get_bone(org).tail)

        return name

    @stage.parent_bones
    def parent_tweak_chain(self):
        ctrl = self.bones.ctrl
        for tweak, main in zip(ctrl.tweak, ctrl.fk + ctrl.fk[-1:]):
            self.set_bone_parent(tweak, main)

    @stage.configure_bones
    def configure_tweak_chain(self):
        for args in zip(count(0), self.bones.ctrl.tweak):
            self.configure_tweak_bone(*args)

    def configure_tweak_bone(self, i: int, tweak: str):
        tweak_pb = self.get_bone(tweak)
        tweak_pb.rotation_mode = 'ZXY'

        if i == len(self.bones.org):
            tweak_pb.lock_rotation_w = True
            tweak_pb.lock_rotation = (True, True, True)
            tweak_pb.lock_scale = (True, True, True)
        else:
            tweak_pb.lock_rotation_w = False
            tweak_pb.lock_rotation = (True, False, True)
            tweak_pb.lock_scale = (False, True, False)

    @stage.generate_widgets
    def make_tweak_widgets(self):
        for tweak in self.bones.ctrl.tweak:
            self.make_tweak_widget(tweak)

    def make_tweak_widget(self, tweak: str):
        create_sphere_widget(self.obj, tweak)

    ##############################
    # ORG chain

    @stage.rig_bones
    def rig_org_chain(self):
        tweaks = self.bones.ctrl.tweak
        for args in zip(count(0), self.bones.org, tweaks, tweaks[1:]):
            self.rig_org_bone(*args)

    # noinspection PyMethodOverriding
    def rig_org_bone(self, i: int, org: str, tweak: str, next_tweak: Optional[str]):
        self.make_constraint(org, 'COPY_TRANSFORMS', tweak)
        if next_tweak:
            self.make_constraint(org, 'STRETCH_TO', next_tweak, keep_axis='SWING_Y')


class ConnectingChainRig(TweakChainRig):
    """Chain rig that can attach to an end of the parent, merging bbone chains."""

    bbone_segments = 8
    use_connect_reverse = None

    use_connect_chain: bool
    connected_tweak: Optional[str]

    def initialize(self):
        super().initialize()

        self.use_connect_chain = self.params.connect_chain
        self.connected_tweak = None

        if self.use_connect_chain:
            first_org = self.bones.org[0]
            parent = self.rigify_parent

            if not isinstance(parent, SimpleChainRig):
                self.raise_error("Cannot connect to non-chain parent rig.")

            parent_orgs = parent.bones.org

            ok_reverse = is_same_position(self.obj, parent_orgs[0], first_org)
            ok_direct = is_connected_position(self.obj, parent_orgs[-1], first_org)

            if self.use_connect_reverse is None:
                self.use_connect_reverse = ok_reverse and not ok_direct

            if not (ok_reverse if self.use_connect_reverse else ok_direct):
                self.raise_error("Cannot connect chain - bone position is disjoint.")

            if isinstance(parent, ConnectingChainRig) and parent.use_connect_reverse:
                self.raise_error("Cannot connect chain - parent is reversed.")

    def prepare_bones(self):
        # Exactly match bone position to parent
        if self.use_connect_chain:
            first_bone = self.get_bone(self.bones.org[0])
            parent_orgs = self.rigify_parent.bones.org

            if self.use_connect_reverse:
                first_bone.head = self.get_bone(parent_orgs[0]).head
            else:
                first_bone.head = self.get_bone(parent_orgs[-1]).tail

    def parent_bones(self):
        # Use the parent of the shared tweak as the rig parent
        root = self.connected_tweak or self.bones.org[0]

        self.rig_parent_bone = self.get_bone_parent(root)

    ##############################
    # Control chain

    @stage.parent_bones
    def parent_control_chain(self):
        super().parent_control_chain()

        self.set_bone_parent(self.bones.ctrl.fk[0], self.rig_parent_bone)

    ##############################
    # Tweak chain

    def check_connect_tweak(self, org: str):
        """ Check if it is possible to share the last parent tweak control. """

        assert self.connected_tweak is None

        if self.use_connect_chain and isinstance(self.rigify_parent, TweakChainRig):
            # Share the last tweak bone of the parent rig
            parent_tweaks = self.rigify_parent.bones.ctrl.tweak
            index = 0 if self.use_connect_reverse else -1
            name = parent_tweaks[index]

            if not is_same_position(self.obj, name, org):
                self.raise_error("Cannot connect tweaks - position mismatch.")

            if not self.use_connect_reverse:
                copy_bone_position(self.obj, org, name, scale=0.5)

                name = self.rename_bone(name, 'tweak_' + strip_org(org))

            self.connected_tweak = parent_tweaks[index] = name

            return name
        else:
            return None

    def make_tweak_bone(self, i: int, org: str):
        if i == 0 and self.check_connect_tweak(org):
            return self.connected_tweak
        else:
            return super().make_tweak_bone(i, org)

    @stage.parent_bones
    def parent_tweak_chain(self):
        ctrl = self.bones.ctrl
        for i, tweak, main in zip(count(0), ctrl.tweak, ctrl.fk + ctrl.fk[-1:]):
            if i > 0 or not (self.connected_tweak and self.use_connect_reverse):
                self.set_bone_parent(tweak, main)

    def configure_tweak_bone(self, i: int, tweak: str):
        super().configure_tweak_bone(i, tweak)

        if self.use_connect_chain and self.use_connect_reverse and i == len(self.bones.org):
            tweak_pb = self.get_bone(tweak)
            tweak_pb.lock_rotation_w = False
            tweak_pb.lock_rotation = (True, False, True)
            tweak_pb.lock_scale = (False, True, False)

    ##############################
    # ORG chain

    @stage.parent_bones
    def parent_org_chain(self):
        if self.use_connect_chain and self.use_connect_reverse:
            flip_bone_chain(self.obj, self.bones.org)

            for org, tweak in zip(self.bones.org, self.bones.ctrl.tweak[1:]):
                self.set_bone_parent(org, tweak)

        else:
            self.set_bone_parent(self.bones.org[0], self.rig_parent_bone)

    def rig_org_bone(self, i: int, org: str, tweak: str, next_tweak: Optional[str]):
        if self.use_connect_chain and self.use_connect_reverse:
            self.make_constraint(org, 'STRETCH_TO', tweak, keep_axis='SWING_Y')
        else:
            super().rig_org_bone(i, org, tweak, next_tweak)

    ##############################
    # Deform chain

    def make_deform_bone(self, i: int, org: str):
        name = super().make_deform_bone(i, org)

        if self.use_connect_chain and self.use_connect_reverse:
            self.set_bone_parent(name, None)
            flip_bone(self.obj, name)

        return name

    @stage.parent_bones
    def parent_deform_chain(self):
        if self.use_connect_chain:
            deform = self.bones.deform
            parent_deform = self.rigify_parent.bones.deform

            if self.use_connect_reverse:
                self.set_bone_parent(deform[-1], self.bones.org[-1])
                self.parent_bone_chain(reversed(deform), use_connect=True)

                connect_bbone_chain_handles(self.obj, [deform[0], parent_deform[0]])
                return

            else:
                self.set_bone_parent(deform[0], parent_deform[-1], use_connect=True)

        super().parent_deform_chain()

    ##############################
    # Settings

    @classmethod
    def add_parameters(cls, params):
        params.connect_chain = bpy.props.BoolProperty(
            name='Connect chain',
            default=False,
            description='Connect the B-Bone chain to the parent rig'
        )

    @classmethod
    def parameters_ui(cls, layout, params):
        r = layout.row()
        r.prop(params, "connect_chain")
