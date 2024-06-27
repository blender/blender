# SPDX-FileCopyrightText: 2019-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
import json

from typing import Optional, NamedTuple, Sequence
from bpy.types import PoseBone, EditBone

from ...utils.animation import add_generic_snap_fk_to_ik, add_fk_ik_snap_buttons
from ...utils.rig import connected_children_names
from ...utils.bones import put_bone, align_bone_orientation, set_bone_widget_transform, TypedBoneDict
from ...utils.naming import strip_org, make_derived_name
from ...utils.layers import ControlLayersOption
from ...utils.misc import pairwise_nozip, padnone, map_list
from ...utils.switch_parent import SwitchParentBuilder
from ...utils.components import CustomPivotControl

from ...base_rig import stage, BaseRig

from ...utils.widgets_basic import create_circle_widget, create_sphere_widget, create_line_widget, create_limb_widget
from ..widgets import create_gear_widget, create_ik_arrow_widget

from ...rig_ui_template import UTILITIES_FUNC_COMMON_IK_FK, PanelLayout

from math import pi
from itertools import count
from mathutils import Vector


class SegmentEntry(NamedTuple):
    org: str                 # ORG bone
    org_idx: int             # Index in the ORG chain
    seg_idx: int | None      # Segment index within ORG bone
    pos: Vector              # Position of the segment start


class BaseLimbRig(BaseRig):
    """Common base for limb rigs."""

    segmented_orgs = 2  # Number of org bones to segment
    min_valid_orgs = None
    max_valid_orgs = None

    segments: int        # Number of tweak segments per org bone
    bbone_segments: int  # Number of B-Bone segments per def bone
    use_ik_pivot: bool
    use_uniform_scale: bool

    main_axis: str
    aux_axis: str

    segment_table: list[SegmentEntry]
    segment_table_end: list[SegmentEntry]
    segment_table_full: list[SegmentEntry]
    segment_table_tweak: list[SegmentEntry]

    rig_parent_bone: str
    prop_bone: str
    elbow_vector: Vector
    pole_angle: float

    def find_org_bones(self, bone: PoseBone) -> 'BaseLimbRig.OrgBones':
        return self.OrgBones(
            main=[bone.name] + connected_children_names(self.obj, bone.name),
        )

    def initialize(self):
        orgs = self.bones.org.main

        min_length = max(self.segmented_orgs + 1, self.min_valid_orgs or 0)
        if len(orgs) < min_length:
            self.raise_error("Input to rig type must be a chain of at least {} bones.", min_length)
        if self.max_valid_orgs and len(orgs) > self.max_valid_orgs:
            self.raise_error("Input to rig type must be a chain of at most {} bones.", self.max_valid_orgs)

        self.segments = self.params.segments
        self.bbone_segments = self.params.bbones
        self.use_ik_pivot = self.params.make_custom_pivot
        self.use_uniform_scale = self.params.limb_uniform_scale

        rot_axis = self.params.rotation_axis

        if rot_axis in {'x', 'automatic'}:
            self.main_axis, self.aux_axis = 'x', 'z'
        elif rot_axis == 'z':
            self.main_axis, self.aux_axis = 'z', 'x'
        else:
            self.raise_error('Unexpected axis value: {}', rot_axis)

        self.segment_table = [
            SegmentEntry(org, i, j, self.get_segment_pos(org, j))
            for i, org in enumerate(orgs[:self.segmented_orgs])
            for j in range(self.segments)
        ]
        self.segment_table_end = [
            SegmentEntry(org, i + self.segmented_orgs, None, self.get_bone(org).head)
            for i, org in enumerate(orgs[self.segmented_orgs:])
        ]

        self.segment_table_full = self.segment_table + self.segment_table_end
        self.segment_table_tweak = self.segment_table + self.segment_table_end[0:1]

    def generate_bones(self):
        bones = map_list(self.get_bone, self.bones.org.main[0:2])

        self.elbow_vector = self.compute_elbow_vector(bones)
        self.pole_angle = self.compute_pole_angle(bones, self.elbow_vector)
        self.rig_parent_bone = self.get_bone_parent(self.bones.org.main[0])

    ####################################################
    # Utilities

    # noinspection PyMethodMayBeStatic
    def compute_elbow_vector(self, bones: list[EditBone]) -> Vector:
        lo_vector = bones[1].vector
        tot_vector = bones[1].tail - bones[0].head
        return (lo_vector.project(tot_vector) - lo_vector).normalized() * tot_vector.length

    def get_main_axis(self, bone: PoseBone | EditBone) -> Vector:
        return getattr(bone, self.main_axis + '_axis')

    def get_aux_axis(self, bone: PoseBone | EditBone) -> Vector:
        return getattr(bone, self.aux_axis + '_axis')

    def compute_pole_angle(self, bones: list[PoseBone | EditBone], elbow_vector: Vector) -> float:
        if self.params.rotation_axis == 'z':
            return 0

        vector = self.get_aux_axis(bones[0]) + self.get_aux_axis(bones[1])

        if elbow_vector.angle(vector) > pi/2:
            return -pi/2
        else:
            return pi/2

    def get_segment_pos(self, org: str, seg: int):
        bone = self.get_bone(org)
        return bone.head + bone.vector * (seg / self.segments)

    @staticmethod
    def vector_without_z(vector: Vector) -> Vector:
        return Vector((vector.x, vector.y, 0))

    ####################################################
    # BONES

    class OrgBones(TypedBoneDict):
        main: list[str]

    class CtrlBones(BaseRig.CtrlBones):
        master: str                    # Main property control.
        fk: list[str]                  # FK control chain.
        tweak: list[str]               # Tweak control chain.
        ik_base: str                   # IK base control.
        ik_pole: str                   # IK pole control.
        ik: str                        # IK limb end control.
        ik_vispole: str                # IK pole visualization line.
        ik_pivot: str                  # Custom IK pivot (optional).

    class MchBones(BaseRig.MchBones):
        master: str                    # Parent of the master control.
        follow: str                    # FK follow behavior.
        fk: list[str | None]           # FK chain parents.
        tweak: list[str]               # Tweak chain parents.
        ik_pivot: str                  # Custom IK pivot result (optional).
        ik_scale: str                  # Helper bone that implements uniform scaling.
        ik_swing: str                  # Bone that tracks ik_target to manually handle limb swing.
        ik_target: str                 # Corrected target position.
        ik_base: str                   # Optionally the base of the ik chain (over ctrl.ik_base)
        ik_end: str                    # End of the IK chain: [ik_base, ik_end]

    bones: BaseRig.ToplevelBones[
        'BaseLimbRig.OrgBones',
        'BaseLimbRig.CtrlBones',
        'BaseLimbRig.MchBones',
        list[str]
    ]

    ####################################################
    # Master control

    @stage.generate_bones
    def make_master_control(self):
        org = self.bones.org.main[0]
        self.bones.mch.master = self.copy_bone(org, make_derived_name(org, 'mch', '_parent_widget'), scale=1/12)
        self.bones.ctrl.master = name = self.copy_bone(org, make_derived_name(org, 'ctrl', '_parent'), scale=1/4)
        self.get_bone(name).roll = 0
        self.prop_bone = self.bones.ctrl.master

    @stage.parent_bones
    def parent_master_control(self):
        self.set_bone_parent(self.bones.ctrl.master, self.rig_parent_bone, inherit_scale='NONE')
        self.set_bone_parent(self.bones.mch.master, self.bones.org.main[0], inherit_scale='NONE')

    @stage.configure_bones
    def configure_master_control(self):
        bone = self.get_bone(self.bones.ctrl.master)
        bone.lock_location = (True, True, True)
        bone.lock_rotation = (True, True, True)
        bone.lock_scale = (not self.use_uniform_scale,) * 3
        bone.lock_rotation_w = True

    @stage.rig_bones
    def rig_master_control(self):
        mch = self.bones.mch
        self.make_constraint(mch.master, 'COPY_SCALE', 'root', use_make_uniform=True)
        if self.use_uniform_scale:
            self.make_constraint(mch.master, 'COPY_SCALE', self.bones.ctrl.master, use_offset=True, space='LOCAL')

    @stage.generate_widgets
    def make_master_control_widget(self):
        master = self.bones.ctrl.master
        set_bone_widget_transform(self.obj, master, self.bones.mch.master)
        create_gear_widget(self.obj, master, radius=1)

    ####################################################
    # FK follow MCH

    @stage.generate_bones
    def make_mch_follow_bone(self):
        org = self.bones.org.main[0]
        self.bones.mch.follow = self.copy_bone(org, make_derived_name(org, 'mch', '_parent'), scale=1/4)

    @stage.parent_bones
    def parent_mch_follow_bone(self):
        mch = self.bones.mch.follow
        align_bone_orientation(self.obj, mch, 'root')
        self.set_bone_parent(mch, self.rig_parent_bone, inherit_scale='FIX_SHEAR')

    @stage.configure_bones
    def configure_mch_follow_bone(self):
        ctrl = self.bones.ctrl
        panel = self.script.panel_with_selected_check(self, [ctrl.master, *ctrl.fk])

        self.make_property(self.prop_bone, 'FK_limb_follow', default=0.0)
        panel.custom_prop(self.prop_bone, 'FK_limb_follow', text='FK Limb Follow', slider=True)

    @stage.rig_bones
    def rig_mch_follow_bone(self):
        mch = self.bones.mch.follow

        self.make_constraint(mch, 'COPY_SCALE', 'root', use_make_uniform=True)

        if self.use_uniform_scale:
            self.make_constraint(
                mch, 'COPY_SCALE', self.bones.ctrl.master,
                use_make_uniform=True, use_offset=True, space='LOCAL'
            )

        con = self.make_constraint(mch, 'COPY_ROTATION', 'root')

        self.make_driver(con, 'influence', variables=[(self.prop_bone, 'FK_limb_follow')])

    ####################################################
    # FK control chain

    @stage.generate_bones
    def make_fk_control_chain(self):
        self.bones.ctrl.fk = map_list(self.make_fk_control_bone, count(0), self.bones.org.main)

    fk_name_suffix_cutoff = 2
    fk_ik_layer_cutoff = 3

    def get_fk_name(self, i: int, org: str, kind: str):
        return make_derived_name(org, kind, '_fk' if i <= self.fk_name_suffix_cutoff else '')

    def make_fk_control_bone(self, i: int, org: str):
        return self.copy_bone(org, self.get_fk_name(i, org, 'ctrl'))

    @stage.parent_bones
    def parent_fk_control_chain(self):
        fk = self.bones.ctrl.fk
        for args in zip(count(0), fk, [self.bones.mch.follow]+fk, self.bones.org.main, self.bones.mch.fk):
            self.parent_fk_control_bone(*args)

    def parent_fk_control_bone(self, i: int, ctrl: str, prev: str, _org: str, parent_mch: str | None):
        if parent_mch:
            self.set_bone_parent(ctrl, parent_mch)
        elif i == 0:
            self.set_bone_parent(ctrl, prev, inherit_scale='AVERAGE')
        else:
            self.set_bone_parent(ctrl, prev, use_connect=True, inherit_scale='ALIGNED')

    @stage.configure_bones
    def configure_fk_control_chain(self):
        for args in zip(count(0), self.bones.ctrl.fk, self.bones.org.main):
            self.configure_fk_control_bone(*args)

        cut = self.fk_ik_layer_cutoff
        ControlLayersOption.FK.assign_rig(self, self.bones.ctrl.fk[0:cut])
        ControlLayersOption.FK.assign_rig(self, self.bones.ctrl.fk[cut:], combine=True, priority=1)

    def configure_fk_control_bone(self, i: int, ctrl: str, org: str):
        self.copy_bone_properties(org, ctrl)

        if i == 2:
            self.get_bone(ctrl).lock_location = True, True, True

    @stage.generate_widgets
    def make_fk_control_widgets(self):
        for args in zip(count(0), self.bones.ctrl.fk):
            self.make_fk_control_widget(*args)

    def make_fk_control_widget(self, i: int, ctrl: str):
        if i < 2:
            create_limb_widget(self.obj, ctrl)
        elif i == 2:
            create_circle_widget(self.obj, ctrl, radius=0.4, head_tail=0.0)
        else:
            create_circle_widget(self.obj, ctrl, radius=0.4, head_tail=0.5)

    ####################################################
    # FK parents MCH chain

    @stage.generate_bones
    def make_fk_parent_chain(self):
        self.bones.mch.fk = map_list(self.make_fk_parent_bone, count(0), self.bones.org.main)

    def make_fk_parent_bone(self, i: int, org: str):
        if i >= 2:
            return self.copy_bone(org, self.get_fk_name(i, org, 'mch'), parent=True, scale=1/4)

    @stage.parent_bones
    def parent_fk_parent_chain(self):
        mch = self.bones.mch
        orgs = self.bones.org.main
        for args in zip(count(0), mch.fk, [mch.follow]+self.bones.ctrl.fk, orgs, [None, *orgs]):
            self.parent_fk_parent_bone(*args)

    def parent_fk_parent_bone(self, i: int, parent_mch: str | None,
                              prev_ctrl: str, org: str, prev_org: str | None):
        if i >= 2:
            self.set_bone_parent(parent_mch, prev_ctrl, use_connect=True, inherit_scale='NONE')

    @stage.rig_bones
    def rig_fk_parent_chain(self):
        for args in zip(count(0), self.bones.mch.fk, self.bones.org.main):
            self.rig_fk_parent_bone(*args)

    def rig_fk_parent_bone(self, i: int, parent_mch: str | None, org: str):
        if i >= 2:
            self.make_constraint(
                parent_mch, 'COPY_SCALE', self.bones.mch.follow, use_make_uniform=True
            )

    ####################################################
    # IK controls

    def get_extra_ik_controls(self):
        if self.component_ik_pivot:
            return [self.component_ik_pivot.control]
        else:
            return []

    def get_middle_ik_controls(self):
        return []

    def get_tail_ik_controls(self):
        return []

    def get_ik_fk_position_chains(self):
        ik_chain = self.get_ik_output_chain()
        tail_chain = self.get_tail_ik_controls()
        return ik_chain, tail_chain, self.bones.ctrl.fk[0:len(ik_chain)+len(tail_chain)]

    def get_ik_control_chain(self):
        ctrl = self.bones.ctrl
        return [ctrl.ik_base, ctrl.ik_pole, *self.get_middle_ik_controls(), ctrl.ik]

    def get_all_ik_controls(self):
        return [
            *self.get_ik_control_chain(),
            *self.get_tail_ik_controls(),
            *self.get_extra_ik_controls(),
        ]

    component_ik_pivot: CustomPivotControl | None

    @stage.generate_bones
    def make_ik_controls(self):
        orgs = self.bones.org.main

        self.bones.ctrl.ik_base = self.make_ik_base_bone(orgs)
        self.bones.ctrl.ik_pole = self.make_ik_pole_bone(orgs)
        self.bones.ctrl.ik = ik_name = parent = self.make_ik_control_bone(orgs)

        if self.use_uniform_scale:
            self.bones.mch.ik_scale = parent = self.make_ik_scale_bone(ik_name, orgs)

        self.component_ik_pivot = self.build_ik_pivot(ik_name, parent=parent)
        self.build_ik_parent_switch(SwitchParentBuilder(self.generator))

    def make_ik_base_bone(self, orgs: list[str]):
        return self.copy_bone(orgs[0], make_derived_name(orgs[0], 'ctrl', '_ik'))

    def make_ik_pole_bone(self, orgs: list[str]):
        name = self.copy_bone(orgs[0], make_derived_name(orgs[0], 'ctrl', '_ik_target'))

        pole = self.get_bone(name)
        pole.head = self.get_bone(orgs[0]).tail + self.elbow_vector
        pole.tail = pole.head - self.elbow_vector/8
        pole.roll = 0

        return name

    def make_ik_control_bone(self, orgs: list[str]):
        return self.copy_bone(orgs[2], make_derived_name(orgs[2], 'ctrl', '_ik'))

    def make_ik_scale_bone(self, ctrl: str, orgs: list[str]):
        return self.copy_bone(ctrl, make_derived_name(orgs[2], 'mch', '_ik_scale'), scale=1/2)

    def build_ik_pivot(self, ik_name: str, **args) -> CustomPivotControl | None:
        if self.use_ik_pivot:
            return CustomPivotControl(self, 'ik_pivot', ik_name, **args)

    def get_ik_control_output(self) -> str:
        if self.component_ik_pivot:
            return self.component_ik_pivot.output
        elif self.use_uniform_scale:
            return self.bones.mch.ik_scale
        else:
            return self.bones.ctrl.ik

    def get_ik_pole_parents(self) -> list[tuple[str, str] | str]:
        return [(self.bones.mch.ik_target, self.bones.ctrl.ik)]

    def register_switch_parents(self, pbuilder: SwitchParentBuilder):
        if self.rig_parent_bone:
            pbuilder.register_parent(self, self.rig_parent_bone)

        pbuilder.register_parent(
            self, self.get_ik_control_output, name=self.bones.ctrl.ik,
            exclude_self=True, tags={'limb_ik', 'child'},
        )

    def build_ik_parent_switch(self, pbuilder: SwitchParentBuilder):
        ctrl = self.bones.ctrl

        def master(): return self.bones.ctrl.master
        def controls(): return [ctrl.master] + self.get_all_ik_controls()

        self.register_switch_parents(pbuilder)

        pbuilder.build_child(
            self, ctrl.ik, prop_bone=master, select_parent='root',
            prop_id='IK_parent', prop_name='IK Parent', controls=controls,
        )

        pbuilder.build_child(
            self, ctrl.ik_pole, prop_bone=master, extra_parents=self.get_ik_pole_parents,
            prop_id='pole_parent', prop_name='Pole Parent', controls=controls,
            no_fix_rotation=True, no_fix_scale=True,
        )

    @stage.parent_bones
    def parent_ik_controls(self):
        if self.use_mch_ik_base:
            self.set_bone_parent(self.bones.ctrl.ik_base, self.bones.mch.follow)
        else:
            self.set_bone_parent(self.bones.ctrl.ik_base, self.bones.mch.ik_swing)

        if self.use_uniform_scale:
            self.set_bone_parent(self.bones.mch.ik_scale, self.bones.ctrl.ik)

        self.set_ik_local_location(self.bones.ctrl.ik)
        self.set_ik_local_location(self.bones.ctrl.ik_pole)

    def set_ik_local_location(self, ctrl: str):
        self.get_bone(ctrl).use_local_location = self.params.ik_local_location

    @stage.configure_bones
    def configure_ik_controls(self):
        base = self.get_bone(self.bones.ctrl.ik_base)
        base.rotation_mode = 'ZXY'
        base.lock_rotation = True, False, True

    @stage.rig_bones
    def rig_ik_controls(self):
        self.rig_hide_pole_control(self.bones.ctrl.ik_pole)

        if self.use_uniform_scale:
            self.rig_ik_control_scale(self.bones.mch.ik_scale)

    def rig_ik_control_scale(self, mch: str):
        self.make_constraint(
            mch, 'COPY_SCALE', self.bones.ctrl.master,
            use_make_uniform=True, use_offset=True, space='LOCAL',
        )

    @stage.generate_widgets
    def make_ik_control_widgets(self):
        ctrl = self.bones.ctrl

        set_bone_widget_transform(self.obj, ctrl.ik, self.get_ik_control_output())

        if self.use_mch_ik_base:
            set_bone_widget_transform(self.obj, ctrl.ik_base, self.bones.mch.ik_base, target_size=True)

        self.make_ik_base_widget(ctrl.ik_base)
        self.make_ik_pole_widget(ctrl.ik_pole)
        self.make_ik_ctrl_widget(ctrl.ik)

    def make_ik_base_widget(self, ctrl: str):
        if self.main_axis == 'x':
            roll = 0
        else:
            roll = pi/2

        create_ik_arrow_widget(self.obj, ctrl, roll=roll)

    def make_ik_pole_widget(self, ctrl: str):
        create_sphere_widget(self.obj, ctrl)

    def make_ik_ctrl_widget(self, ctrl: str):
        raise NotImplementedError()

    ####################################################
    # IK pole visualization

    @stage.generate_bones
    def make_ik_vispole_bone(self):
        orgs = self.bones.org.main
        name = self.copy_bone(orgs[1], 'VIS_'+make_derived_name(orgs[0], 'ctrl', '_ik_pole'))
        self.bones.ctrl.ik_vispole = name

        bone = self.get_bone(name)
        bone.tail = bone.head + Vector((0, 0, bone.length / 10))
        bone.hide_select = True

    @stage.rig_bones
    def rig_ik_vispole_bone(self):
        name = self.bones.ctrl.ik_vispole

        self.make_constraint(name, 'COPY_LOCATION', self.bones.org.main[1])
        self.make_constraint(
            name, 'STRETCH_TO', self.bones.ctrl.ik_pole,
            volume='NO_VOLUME', rest_length=self.get_bone(name).length
        )

        self.rig_hide_pole_control(name)

    @stage.generate_widgets
    def make_ik_vispole_widget(self):
        create_line_widget(self.obj, self.bones.ctrl.ik_vispole)

    ####################################################
    # IK system MCH

    ik_input_head_tail = 0.0

    use_mch_ik_base = False

    def get_ik_input_bone(self):
        return self.get_ik_control_output()

    def get_ik_chain_base(self):
        return self.bones.mch.ik_base if self.use_mch_ik_base else self.bones.ctrl.ik_base

    def get_ik_output_chain(self):
        return [self.get_ik_chain_base(), self.bones.mch.ik_end, self.bones.mch.ik_target]

    @stage.generate_bones
    def make_ik_mch_chain(self):
        orgs = self.bones.org.main

        if self.use_mch_ik_base:
            self.bones.mch.ik_base = self.make_ik_mch_base_bone(orgs)

        self.bones.mch.ik_swing = self.make_ik_mch_swing_bone(orgs)
        self.bones.mch.ik_target = self.make_ik_mch_target_bone(orgs)
        self.bones.mch.ik_end = self.copy_bone(orgs[1], make_derived_name(orgs[1], 'mch', '_ik'))

    def make_ik_mch_base_bone(self, orgs):
        return self.copy_bone(orgs[0], make_derived_name(orgs[0], 'mch', '_ik'))

    def make_ik_mch_swing_bone(self, orgs):
        name = self.copy_bone(orgs[0], make_derived_name(orgs[0], 'mch', '_ik_swing'))
        bone = self.get_bone(name)
        bone.tail = bone.head + (self.get_bone(orgs[2]).head - bone.head).normalized() * bone.length * 0.3
        return name

    def make_ik_mch_target_bone(self, orgs):
        return self.copy_bone(orgs[2], make_derived_name(orgs[0], 'mch', '_ik_target'))

    @stage.parent_bones
    def parent_ik_mch_chain(self):
        if self.use_mch_ik_base:
            self.set_bone_parent(self.bones.mch.ik_swing, self.bones.ctrl.ik_base, inherit_scale='AVERAGE')
            self.set_bone_parent(self.bones.mch.ik_base, self.bones.mch.ik_swing)
        else:
            self.set_bone_parent(self.bones.mch.ik_swing, self.bones.mch.follow)

        self.set_bone_parent(self.bones.mch.ik_target, self.get_ik_input_bone())
        self.set_bone_parent(self.bones.mch.ik_end, self.get_ik_chain_base())

    @stage.configure_bones
    def configure_ik_mch_chain(self):
        bone = self.get_bone(self.get_ik_chain_base())
        bone.ik_stretch = 0.1

        bone = self.get_bone(self.bones.mch.ik_end)
        bone.ik_stretch = 0.1
        bone.lock_ik_x = bone.lock_ik_y = bone.lock_ik_z = True
        setattr(bone, 'lock_ik_' + self.main_axis, False)

    @stage.configure_bones
    def configure_ik_mch_panel(self):
        ctrl = self.bones.ctrl
        panel = self.script.panel_with_selected_check(self, ctrl.flatten())

        rig_name = strip_org(self.bones.org.main[2])

        self.make_property(self.prop_bone, 'IK_FK', default=0.0, description='IK/FK Switch')
        panel.custom_prop(self.prop_bone, 'IK_FK', text='IK-FK ({})'.format(rig_name), slider=True)

        self.add_global_buttons(panel, rig_name)

        panel = self.script.panel_with_selected_check(self, [ctrl.master, *self.get_all_ik_controls()])

        self.make_property(self.prop_bone, 'IK_Stretch', default=1.0, description='IK Stretch')
        panel.custom_prop(self.prop_bone, 'IK_Stretch', text='IK Stretch', slider=True)

        self.make_property(self.prop_bone, 'pole_vector', default=False,
                           description='Use a pole target control')

        self.add_ik_only_buttons(panel, rig_name)

    def add_global_buttons(self, panel: PanelLayout, rig_name: str):
        ctrl = self.bones.ctrl
        ik_chain, tail_chain, fk_chain = self.get_ik_fk_position_chains()

        add_generic_snap_fk_to_ik(
            panel,
            fk_bones=fk_chain, ik_bones=ik_chain+tail_chain,
            ik_ctrl_bones=self.get_all_ik_controls(),
            rig_name=rig_name
        )

        add_limb_snap_ik_to_fk(
            panel,
            master=ctrl.master,
            fk_bones=fk_chain, ik_bones=ik_chain, tail_bones=tail_chain,
            ik_ctrl_bones=self.get_ik_control_chain(),
            ik_extra_ctrls=self.get_extra_ik_controls(),
            rig_name=rig_name
        )

    def add_ik_only_buttons(self, panel: PanelLayout, rig_name: str):
        ctrl = self.bones.ctrl
        ik_chain, tail_chain, fk_chain = self.get_ik_fk_position_chains()

        add_limb_toggle_pole(
            panel, master=ctrl.master,
            ik_bones=ik_chain,
            ik_ctrl_bones=self.get_ik_control_chain(),
            ik_extra_ctrls=self.get_extra_ik_controls(),
        )

    @stage.rig_bones
    def rig_ik_mch_chain(self):
        mch = self.bones.mch
        input_bone = self.get_ik_input_bone()

        self.make_constraint(mch.ik_swing, 'DAMPED_TRACK', mch.ik_target)

        self.rig_ik_mch_stretch_limit(
            mch.ik_target, mch.follow, input_bone, self.ik_input_head_tail, 2)
        self.rig_ik_mch_end_bone(mch.ik_end, mch.ik_target, self.bones.ctrl.ik_pole)

    def rig_ik_mch_stretch_limit(self, mch_target: str, base_bone: str, input_bone: str,
                                 head_tail: float, org_count: int, bias=1.035):
        # Compute increase in length to fully straighten
        orgs = self.bones.org.main[0:org_count]
        len_full = sum(self.get_bone(org).length for org in orgs)

        # Snap the target to the input position
        self.make_constraint(mch_target, 'COPY_LOCATION', input_bone, head_tail=head_tail)

        # Limit distance from the base of the limb
        con = self.make_constraint(
            mch_target, 'LIMIT_DISTANCE', base_bone,
            limit_mode='LIMITDIST_INSIDE', distance=len_full*bias,
            # Use custom space to tolerate rig scaling
            space='CUSTOM', space_object=self.obj, space_subtarget=self.bones.mch.follow,
        )

        self.make_driver(con, "influence",
                         variables=[(self.prop_bone, 'IK_Stretch')], polynomial=[1.0, -1.0])

    def rig_ik_mch_end_bone(self, mch_ik: str, mch_target: str, ctrl_pole: str, chain=2):
        con = self.make_constraint(
            mch_ik, 'IK', mch_target, chain_count=chain,
        )

        self.make_driver(con, "mute",
                         variables=[(self.prop_bone, 'pole_vector')], polynomial=[0.0, 1.0])

        con_pole = self.make_constraint(
            mch_ik, 'IK', mch_target, chain_count=chain,
            pole_target=self.obj, pole_subtarget=ctrl_pole, pole_angle=self.pole_angle,
        )

        self.make_driver(con_pole, "mute",
                         variables=[(self.prop_bone, 'pole_vector')], polynomial=[1.0, -1.0])

    def rig_hide_pole_control(self, name: str):
        self.make_driver(
            self.get_bone(name).bone, "hide",
            variables=[(self.prop_bone, 'pole_vector')], polynomial=[1.0, -1.0],
        )

    ####################################################
    # ORG chain

    @stage.parent_bones
    def parent_org_chain(self):
        orgs = self.bones.org.main
        if len(orgs) > 3:
            self.get_bone(orgs[3]).use_connect = False

    @stage.rig_bones
    def rig_org_chain(self):
        ik = self.get_ik_output_chain() + self.get_tail_ik_controls()
        for args in zip(count(0), self.bones.org.main, self.bones.ctrl.fk, padnone(ik)):
            self.rig_org_bone(*args)

    def rig_org_bone(self, i: int, org: str, fk: str, ik: str | None):
        self.make_constraint(org, 'COPY_TRANSFORMS', fk)

        if ik:
            con = self.make_constraint(org, 'COPY_TRANSFORMS', ik)

            self.make_driver(con, 'influence',
                             variables=[(self.prop_bone, 'IK_FK')], polynomial=[1.0, -1.0])

    ####################################################
    # Tweak control chain

    @stage.generate_bones
    def make_tweak_chain(self):
        self.bones.ctrl.tweak = map_list(self.make_tweak_bone, count(0), self.segment_table_tweak)

    def make_tweak_bone(self, _i: int, entry: SegmentEntry):
        name = make_derived_name(entry.org, 'ctrl', '_tweak')
        name = self.copy_bone(entry.org, name, scale=1/(2 * self.segments))
        put_bone(self.obj, name, entry.pos)
        return name

    @stage.parent_bones
    def parent_tweak_chain(self):
        for ctrl, mch in zip(self.bones.ctrl.tweak, self.bones.mch.tweak):
            self.set_bone_parent(ctrl, mch)

    @stage.configure_bones
    def configure_tweak_chain(self):
        for args in zip(count(0), self.bones.ctrl.tweak, self.segment_table_tweak):
            self.configure_tweak_bone(*args)

        ControlLayersOption.TWEAK.assign_rig(self, self.bones.ctrl.tweak)

    def configure_tweak_bone(self, i: int, tweak: str, entry: SegmentEntry):
        tweak_pb = self.get_bone(tweak)
        tweak_pb.lock_rotation = (True, False, True)
        tweak_pb.lock_scale = (False, True, False)
        tweak_pb.rotation_mode = 'ZXY'

        if i > 0 and entry.seg_idx is not None:
            self.make_rubber_tweak_property(i, tweak, entry)

    def make_rubber_tweak_property(self, _i: int, tweak: str, entry: SegmentEntry):
        def_val = 1.0 if entry.seg_idx else 0.0
        text = 'Rubber Tweak ({})'.format(strip_org(entry.org))

        self.make_property(tweak, 'rubber_tweak', def_val, max=2.0, soft_max=1.0)

        panel = self.script.panel_with_selected_check(self, [tweak])
        panel.custom_prop(tweak, 'rubber_tweak', text=text, slider=True)

    @stage.generate_widgets
    def make_tweak_widgets(self):
        for tweak in self.bones.ctrl.tweak:
            self.make_tweak_widget(tweak)

    def make_tweak_widget(self, tweak: str):
        create_sphere_widget(self.obj, tweak)

    ####################################################
    # Tweak MCH chain

    @stage.generate_bones
    def make_tweak_mch_chain(self):
        self.bones.mch.tweak = map_list(self.make_tweak_mch_bone, count(0), self.segment_table_tweak)

    def make_tweak_mch_bone(self, _i: int, entry: SegmentEntry):
        name = make_derived_name(entry.org, 'mch', '_tweak')
        name = self.copy_bone(entry.org, name, scale=1/(4 * self.segments))
        put_bone(self.obj, name, entry.pos)
        return name

    @stage.parent_bones
    def parent_tweak_mch_chain(self):
        for args in zip(count(0), self.bones.mch.tweak, self.segment_table_tweak):
            self.parent_tweak_mch_bone(*args)

    def parent_tweak_mch_bone(self, i: int, mch: str, entry: SegmentEntry):
        if i == 0:
            self.set_bone_parent(mch, self.rig_parent_bone, inherit_scale='FIX_SHEAR')
        else:
            self.set_bone_parent(mch, entry.org)

    @stage.apply_bones
    def apply_tweak_mch_chain(self):
        for args in zip(count(0), self.bones.mch.tweak, self.segment_table_tweak):
            self.apply_tweak_mch_bone(*args)

    def apply_tweak_mch_bone(self, i: int, tweak: str, entry: SegmentEntry):
        if entry.seg_idx:
            prev_tweak, next_tweak, fac = self.get_tweak_blend(i, entry)

            # Apply the final roll resulting from mixing tweaks to rest pose
            prev_mat = self.get_bone(prev_tweak).matrix
            next_mat = self.get_bone(next_tweak).matrix
            rot_mat = prev_mat.lerp(next_mat, fac)

            bone = self.get_bone(tweak)
            bone.roll += (bone.matrix.inverted() @ rot_mat).to_quaternion().to_swing_twist('Y')[1]

    @stage.rig_bones
    def rig_tweak_mch_chain(self):
        for args in zip(count(0), self.bones.mch.tweak, self.segment_table_tweak):
            self.rig_tweak_mch_bone(*args)

    def get_tweak_blend(self, i: int, entry: SegmentEntry):
        assert entry.seg_idx

        tweaks = self.bones.ctrl.tweak
        prev_tweak = tweaks[i - entry.seg_idx]
        next_tweak = tweaks[i + self.segments - entry.seg_idx]
        fac = entry.seg_idx / self.segments

        return prev_tweak, next_tweak, fac

    def rig_tweak_mch_bone(self, i: int, tweak: str, entry: SegmentEntry):
        if entry.seg_idx:
            prev_tweak, next_tweak, fac = self.get_tweak_blend(i, entry)

            self.make_constraint(tweak, 'COPY_TRANSFORMS', prev_tweak)
            self.make_constraint(tweak, 'COPY_TRANSFORMS', next_tweak, influence=fac)
            self.make_constraint(tweak, 'DAMPED_TRACK', next_tweak)

        elif entry.seg_idx is not None:
            self.make_constraint(tweak, 'COPY_SCALE', self.bones.mch.follow, use_make_uniform=True)

        if i == 0:
            self.make_constraint(tweak, 'COPY_LOCATION', entry.org)
            self.make_constraint(tweak, 'DAMPED_TRACK', entry.org, head_tail=1)

    ####################################################
    # Deform chain

    @stage.generate_bones
    def make_deform_chain(self):
        self.bones.deform = map_list(self.make_deform_bone, count(0), self.segment_table_full)

    def make_deform_bone(self, _i: int, entry: SegmentEntry):
        name = make_derived_name(entry.org, 'def')

        if entry.seg_idx is None:
            name = self.copy_bone(entry.org, name)
        else:
            name = self.copy_bone(entry.org, name, bbone=True, scale=1/self.segments)
            put_bone(self.obj, name, entry.pos)
            self.get_bone(name).bbone_segments = self.bbone_segments

        return name

    @stage.parent_bones
    def parent_deform_chain(self):
        self.set_bone_parent(self.bones.deform[0], self.rig_parent_bone)
        self.parent_bone_chain(self.bones.deform, use_connect=True)

    @stage.rig_bones
    def rig_deform_chain(self):
        tweaks = pairwise_nozip(padnone(self.bones.ctrl.tweak))
        entries = pairwise_nozip(padnone(self.segment_table_full))

        for args in zip(count(0), self.bones.deform, *entries, *tweaks):
            self.rig_deform_bone(*args)

    def rig_deform_bone(self, i: int, deform: str,
                        entry: SegmentEntry, next_entry: SegmentEntry | None,
                        tweak: str | None, next_tweak: str | None):
        if tweak:
            self.make_constraint(deform, 'COPY_TRANSFORMS', tweak)

            if next_tweak:
                self.make_constraint(deform, 'STRETCH_TO', next_tweak, keep_axis='SWING_Y')

                self.rig_deform_easing(i, deform, tweak, next_tweak)

            elif next_entry:
                self.make_constraint(deform, 'STRETCH_TO', next_entry.org, keep_axis='SWING_Y')

        else:
            self.make_constraint(deform, 'COPY_TRANSFORMS', entry.org)

    def rig_deform_easing(self, _i: int, deform: str, tweak: str, next_tweak: str):
        pbone = self.get_bone(deform)

        if 'rubber_tweak' in self.get_bone(tweak):
            self.make_driver(pbone.bone, 'bbone_easein', variables=[(tweak, 'rubber_tweak')])
        else:
            pbone.bone.bbone_easein = 0.0

        if 'rubber_tweak' in self.get_bone(next_tweak):
            self.make_driver(pbone.bone, 'bbone_easeout', variables=[(next_tweak, 'rubber_tweak')])
        else:
            pbone.bone.bbone_easeout = 0.0

    ####################################################
    # Settings

    @classmethod
    def add_parameters(cls, params):
        """ Add the parameters of this rig type to the
            RigifyParameters PropertyGroup
        """

        items = [
            ('x', 'X manual', ''),
            ('z', 'Z manual', ''),
            ('automatic', 'Automatic', '')
        ]

        params.rotation_axis = bpy.props.EnumProperty(
            items=items,
            name="Rotation Axis",
            default='automatic'
        )

        params.auto_align_extremity = bpy.props.BoolProperty(
            name='auto_align_extremity',
            default=False,
            description="Auto Align Extremity Bone"
        )

        params.segments = bpy.props.IntProperty(
            name='Limb Segments',
            default=2,
            min=1,
            description='Number of limb segments'
        )

        params.bbones = bpy.props.IntProperty(
            name='B-Bone Segments',
            default=10,
            min=1,
            description='Number of B-Bone segments'
        )

        params.make_custom_pivot = bpy.props.BoolProperty(
            name="Custom Pivot Control",
            default=False,
            description="Create a rotation pivot control that can be repositioned arbitrarily"
        )

        params.ik_local_location = bpy.props.BoolProperty(
            name="IK Local Location",
            default=True,
            description="Specifies the value of the Local Location option for IK controls, which "
                        "decides if the location channels are aligned to the local control "
                        "orientation or world",
        )

        params.limb_uniform_scale = bpy.props.BoolProperty(
            name="Support Uniform Scaling",
            default=False,
            description="Support uniformly scaling the limb via the gear control at the base"
        )

        # Setting up extra layers for the FK and tweak
        ControlLayersOption.FK.add_parameters(params)
        ControlLayersOption.TWEAK.add_parameters(params)

    @classmethod
    def parameters_ui(cls, layout, params, end='End'):
        """ Create the ui for the rig parameters."""

        r = layout.row()
        r.prop(params, "rotation_axis")

        if 'auto' not in params.rotation_axis.lower():
            r = layout.row()
            r.prop(params, "auto_align_extremity", text="Auto Align "+end)

        r = layout.row()
        r.prop(params, "segments")

        r = layout.row()
        r.prop(params, "bbones")

        layout.prop(params, 'limb_uniform_scale')
        layout.prop(params, 'make_custom_pivot', text="Custom IK Pivot")
        layout.prop(params, 'ik_local_location')

        ControlLayersOption.FK.parameters_ui(layout, params)
        ControlLayersOption.TWEAK.parameters_ui(layout, params)


###########################
# Limb IK to FK operator ##
###########################

SCRIPT_REGISTER_OP_SNAP_IK_FK = ['POSE_OT_rigify_limb_ik2fk', 'POSE_OT_rigify_limb_ik2fk_bake']

SCRIPT_UTILITIES_OP_SNAP_IK_FK = UTILITIES_FUNC_COMMON_IK_FK + ['''
########################
## Limb Snap IK to FK ##
########################

class RigifyLimbIk2FkBase:
    prop_bone:    StringProperty(name="Settings Bone")
    pole_prop:    StringProperty(name="Pole target switch", default="pole_vector")
    fk_bones:     StringProperty(name="FK Bone Chain")
    ik_bones:     StringProperty(name="IK Result Bone Chain")
    ctrl_bones:   StringProperty(name="IK Controls")
    tail_bones:   StringProperty(name="Tail IK Controls", default="[]")
    extra_ctrls:  StringProperty(name="Extra IK Controls")

    def init_execute(self, context):
        if self.fk_bones:
            self.fk_bone_list = json.loads(self.fk_bones)
        self.ik_bone_list = json.loads(self.ik_bones)
        self.ctrl_bone_list = json.loads(self.ctrl_bones)
        self.tail_bone_list = json.loads(self.tail_bones)
        self.extra_ctrl_list = json.loads(self.extra_ctrls)

    def get_use_pole(self, obj):
        bone = obj.pose.bones[self.prop_bone]
        return self.pole_prop in bone and bone[self.pole_prop]

    def save_frame_state(self, context, obj):
        return get_chain_transform_matrices(obj, self.fk_bone_list)

    def compute_base_rotation(self, context, ik_bones, ctrl_bones, matrices, use_pole):
        context.view_layer.update()

        if use_pole:
            match_pole_target(
                context.view_layer,
                ik_bones[0], ik_bones[1], ctrl_bones[1], matrices[0],
                (ik_bones[0].length + ik_bones[1].length)
            )

        else:
            correct_rotation(context.view_layer, ik_bones[0], matrices[0], ctrl_ik=ctrl_bones[0])

    def assign_middle_controls(self, context, obj, matrices, ik_bones, ctrl_bones, *, lock=False, keyflags=None):
        for mat, ik, ctrl in reversed(list(zip(matrices[2:-1], ik_bones[2:-1], ctrl_bones[2:-1]))):
            ctrl.bone.use_inherit_rotation = not lock
            ctrl.bone.inherit_scale = 'NONE' if lock else 'FULL'
            context.view_layer.update()
            mat = convert_pose_matrix_via_rest_delta(mat, ik, ctrl)
            set_transform_from_matrix(obj, ctrl.name, mat, keyflags=keyflags)

    def assign_extra_controls(self, context, obj, all_matrices, ik_bones, ctrl_bones):
        for extra in self.extra_ctrl_list:
            set_transform_from_matrix(
                obj, extra, Matrix.Identity(4), space='LOCAL', keyflags=self.keyflags
            )

    def apply_frame_state(self, context, obj, all_matrices):
        ik_bones = [ obj.pose.bones[k] for k in self.ik_bone_list ]
        ctrl_bones = [ obj.pose.bones[k] for k in self.ctrl_bone_list ]
        tail_bones = [ obj.pose.bones[k] for k in self.tail_bone_list ]

        assert len(all_matrices) >= len(ik_bones) + len(tail_bones)

        matrices = all_matrices[0:len(ik_bones)]
        tail_matrices = all_matrices[len(ik_bones):]

        use_pole = self.get_use_pole(obj)

        # Remove foot heel transform, if present
        self.assign_extra_controls(context, obj, all_matrices, ik_bones, ctrl_bones)

        context.view_layer.update()

        # Set the end control position
        end_mat = convert_pose_matrix_via_pose_delta(matrices[-1], ik_bones[-1], ctrl_bones[-1])

        set_transform_from_matrix(
            obj, self.ctrl_bone_list[-1], end_mat, keyflags=self.keyflags,
            undo_copy_scale=True,
        )

        # Set the base bone position
        ctrl_bones[0].matrix_basis = Matrix.Identity(4)

        set_transform_from_matrix(
            obj, self.ctrl_bone_list[0], matrices[0],
            no_scale=True, no_rot=use_pole,
        )

        # Lock middle control transforms (first pass)
        self.assign_middle_controls(context, obj, matrices, ik_bones, ctrl_bones, lock=True)

        # Adjust the base bone state
        self.compute_base_rotation(context, ik_bones, ctrl_bones, matrices, use_pole)

        correct_scale(context.view_layer, ik_bones[0], matrices[0], ctrl_ik=ctrl_bones[0])

        # Assign middle control transforms (final pass)
        self.assign_middle_controls(context, obj, matrices, ik_bones, ctrl_bones, keyflags=self.keyflags)

        # Assign tail control transforms
        for mat, ctrl in zip(tail_matrices, tail_bones):
            context.view_layer.update()
            set_transform_from_matrix(obj, ctrl.name, mat, keyflags=self.keyflags)

        # Keyframe controls
        if self.keyflags is not None:
            if use_pole:
                keyframe_transform_properties(
                    obj, self.ctrl_bone_list[1], self.keyflags,
                    no_rot=True, no_scale=True,
                )

            keyframe_transform_properties(
                obj, self.ctrl_bone_list[0], self.keyflags,
                no_rot=use_pole,
            )

class POSE_OT_rigify_limb_ik2fk(RigifyLimbIk2FkBase, RigifySingleUpdateMixin, bpy.types.Operator):
    bl_idname = "pose.rigify_limb_ik2fk_" + rig_id
    bl_label = "Snap IK->FK"
    bl_description = "Snap the IK chain to FK result"

class POSE_OT_rigify_limb_ik2fk_bake(RigifyLimbIk2FkBase, RigifyBakeKeyframesMixin, bpy.types.Operator):
    bl_idname = "pose.rigify_limb_ik2fk_bake_" + rig_id
    bl_label = "Apply Snap IK->FK To Keyframes"
    bl_description = "Snap the IK chain keyframes to FK result"

    def execute_scan_curves(self, context, obj):
        self.bake_add_bone_frames(self.fk_bone_list, TRANSFORM_PROPS_ALL)
        return self.bake_get_all_bone_curves(self.ctrl_bone_list + self.extra_ctrl_list, TRANSFORM_PROPS_ALL)
''']


def add_limb_snap_ik_to_fk(panel: 'PanelLayout', *,
                           master: Optional[str] = None,
                           fk_bones: Sequence[str] = (),
                           ik_bones: Sequence[str] = (), tail_bones: Sequence[str] = (),
                           ik_ctrl_bones: Sequence[str] = (), ik_extra_ctrls: Sequence[str] = (),
                           rig_name=''):
    panel.use_bake_settings()
    panel.script.add_utilities(SCRIPT_UTILITIES_OP_SNAP_IK_FK)
    panel.script.register_classes(SCRIPT_REGISTER_OP_SNAP_IK_FK)

    assert len(fk_bones) == len(ik_bones) + len(tail_bones)

    op_props = {
        'prop_bone': master,
        'fk_bones': json.dumps(fk_bones),
        'ik_bones': json.dumps(ik_bones),
        'ctrl_bones': json.dumps(ik_ctrl_bones),
        'tail_bones': json.dumps(tail_bones),
        'extra_ctrls': json.dumps(ik_extra_ctrls),
    }

    add_fk_ik_snap_buttons(
        panel, 'pose.rigify_limb_ik2fk_{rig_id}', 'pose.rigify_limb_ik2fk_bake_{rig_id}',
        label='IK->FK', rig_name=rig_name, properties=op_props,
        clear_bones=[*ik_ctrl_bones, *tail_bones, *ik_extra_ctrls],
    )


#########################
# Toggle Pole operator ##
#########################

SCRIPT_REGISTER_OP_TOGGLE_POLE = ['POSE_OT_rigify_limb_toggle_pole', 'POSE_OT_rigify_limb_toggle_pole_bake']

SCRIPT_UTILITIES_OP_TOGGLE_POLE = SCRIPT_UTILITIES_OP_SNAP_IK_FK + ['''
####################
## Toggle IK Pole ##
####################

class RigifyLimbTogglePoleBase(RigifyLimbIk2FkBase):
    use_pole: bpy.props.BoolProperty(name="Use Pole Vector")

    def save_frame_state(self, context, obj):
        return get_chain_transform_matrices(obj, self.ik_bone_list)

    def apply_frame_state(self, context, obj, matrices):
        ik_bones = [ obj.pose.bones[k] for k in self.ik_bone_list ]
        ctrl_bones = [ obj.pose.bones[k] for k in self.ctrl_bone_list ]

        # Set the pole property
        set_custom_property_value(
            obj, self.prop_bone, self.pole_prop, bool(self.use_pole),
            keyflags=self.keyflags_switch
        )

        # Lock middle control transforms
        self.assign_middle_controls(context, obj, matrices, ik_bones, ctrl_bones, lock=True)

        # Reset the base bone rotation
        set_pose_rotation(ctrl_bones[0], Matrix.Identity(4))

        self.compute_base_rotation(context, ik_bones, ctrl_bones, matrices, self.use_pole)

        # Assign middle control transforms (final pass)
        self.assign_middle_controls(context, obj, matrices, ik_bones, ctrl_bones, keyflags=self.keyflags)

        # Keyframe controls
        if self.keyflags is not None:
            if self.use_pole:
                keyframe_transform_properties(
                    obj, self.ctrl_bone_list[2], self.keyflags,
                    no_rot=True, no_scale=True,
                )
            else:
                keyframe_transform_properties(
                    obj, self.ctrl_bone_list[0], self.keyflags,
                    no_loc=True, no_scale=True,
                )

    def init_invoke(self, context):
        self.use_pole = not bool(context.active_object.pose.bones[self.prop_bone][self.pole_prop])

class POSE_OT_rigify_limb_toggle_pole(RigifyLimbTogglePoleBase, RigifySingleUpdateMixin, bpy.types.Operator):
    bl_idname = "pose.rigify_limb_toggle_pole_" + rig_id
    bl_label = "Toggle Pole"
    bl_description = "Switch the IK chain between pole and rotation"

class POSE_OT_rigify_limb_toggle_pole_bake(RigifyLimbTogglePoleBase, RigifyBakeKeyframesMixin, bpy.types.Operator):
    bl_idname = "pose.rigify_limb_toggle_pole_bake_" + rig_id
    bl_label = "Apply Toggle Pole To Keyframes"
    bl_description = "Switch the IK chain between pole and rotation over a frame range"

    def execute_scan_curves(self, context, obj):
        self.bake_add_bone_frames(self.ctrl_bone_list, TRANSFORM_PROPS_ALL)

        rot_curves = self.bake_get_all_bone_curves(self.ctrl_bone_list[0], TRANSFORM_PROPS_ROTATION)
        pole_curves = self.bake_get_all_bone_curves(self.ctrl_bone_list[2], TRANSFORM_PROPS_LOCATION)
        return rot_curves + pole_curves

    def execute_before_apply(self, context, obj, range, range_raw):
        self.bake_replace_custom_prop_keys_constant(self.prop_bone, self.pole_prop, bool(self.use_pole))

    def draw(self, context):
        self.layout.prop(self, 'use_pole')
''']


def add_limb_toggle_pole(panel: 'PanelLayout', *,
                         master: Optional[str] = None,
                         ik_bones: Sequence[str] = (), ik_ctrl_bones: Sequence[str] = (),
                         ik_extra_ctrls: Sequence[str] = ()):
    panel.use_bake_settings()
    panel.script.add_utilities(SCRIPT_UTILITIES_OP_TOGGLE_POLE)
    panel.script.register_classes(SCRIPT_REGISTER_OP_TOGGLE_POLE)

    op_props = {
        'prop_bone': master,
        'ik_bones': json.dumps(ik_bones),
        'ctrl_bones': json.dumps(ik_ctrl_bones),
        'extra_ctrls': json.dumps(ik_extra_ctrls),
    }

    row = panel.row(align=True)
    left_split = row.split(factor=0.65, align=True)
    left_split.operator('pose.rigify_limb_toggle_pole_{rig_id}',
                        icon='FORCE_MAGNETIC', properties=op_props)
    text = left_split.expr_if_else(left_split.expr_bone(master)['pole_vector'], 'On', 'Off')
    left_split.custom_prop(master, 'pole_vector', text=text, toggle=True)
    row.operator('pose.rigify_limb_toggle_pole_bake_{rig_id}',
                 text='', icon='ACTION_TWEAK', properties=op_props)
