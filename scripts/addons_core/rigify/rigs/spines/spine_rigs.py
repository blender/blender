# SPDX-FileCopyrightText: 2019-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy

from typing import Optional, NamedTuple
from itertools import count

from ...utils.layers import ControlLayersOption
from ...utils.naming import make_derived_name
from ...utils.bones import align_bone_orientation, align_bone_to_axis, put_bone, set_bone_widget_transform
from ...utils.widgets_basic import create_cube_widget
from ...utils.switch_parent import SwitchParentBuilder
from ...utils.components import CustomPivotControl

from ...base_rig import stage

from ..chain_rigs import TweakChainRig, ConnectingChainRig


class BaseSpineRig(TweakChainRig):
    """
    Spine rig with tweaks.
    """

    bbone_segments = 8
    min_chain_length = 3

    use_torso_pivot: bool  # Generate the custom pivot control
    length: float          # Total length of the chain bones

    def initialize(self):
        super().initialize()

        self.use_torso_pivot = self.params.make_custom_pivot
        self.length = sum([self.get_bone(b).length for b in self.bones.org])

    ####################################################
    # BONES

    class CtrlBones(TweakChainRig.CtrlBones):
        master: str                    # Master control
        master_pivot: str              # Custom pivot under the master control (conditional)

    class MchBones(TweakChainRig.MchBones):
        master_pivot: str              # Final output of the custom pivot (conditional)

    bones: TweakChainRig.ToplevelBones[
        list[str],
        'BaseSpineRig.CtrlBones',
        'BaseSpineRig.MchBones',
        list[str]
    ]

    ####################################################
    # Master control bone

    component_torso_pivot: Optional[CustomPivotControl]

    @stage.generate_bones
    def make_master_control(self):
        self.bones.ctrl.master = name = self.make_master_control_bone(self.bones.org)

        self.component_torso_pivot = self.build_master_pivot(name)
        self.build_parent_switch(name)

    def get_master_control_pos(self, orgs: list[str]):
        return self.get_bone(orgs[0]).head

    def make_master_control_bone(self, orgs: list[str]):
        name = self.copy_bone(orgs[0], 'torso')
        put_bone(self.obj, name, self.get_master_control_pos(orgs))
        align_bone_to_axis(self.obj, name, 'y', length=self.length * 0.6)
        return name

    def build_master_pivot(self, master_name: str, **args):
        if self.use_torso_pivot:
            return CustomPivotControl(
                self, 'master_pivot', master_name, parent=master_name, **args
            )

    def get_master_control_output(self):
        if self.component_torso_pivot:
            return self.component_torso_pivot.output
        else:
            return self.bones.ctrl.master

    def build_parent_switch(self, master_name: str):
        pbuilder = SwitchParentBuilder(self.generator)

        org_parent = self.get_bone_parent(self.bones.org[0])
        parents = [org_parent] if org_parent else []

        pbuilder.register_parent(self, self.get_master_control_output, name='Torso', tags={'torso', 'child'})

        pbuilder.build_child(
            self, master_name, exclude_self=True,
            extra_parents=parents, select_parent=org_parent,
            prop_id='torso_parent', prop_name='Torso Parent',
            controls=lambda: self.bones.flatten('ctrl'),
        )

        self.register_parent_bones(pbuilder)

    def register_parent_bones(self, pbuilder: SwitchParentBuilder):
        pbuilder.register_parent(self, self.bones.org[0], name='Hips', exclude_self=True, tags={'hips'})
        pbuilder.register_parent(self, self.bones.org[-1], name='Chest', exclude_self=True, tags={'chest'})

    @stage.parent_bones
    def parent_master_control(self):
        pass

    @stage.configure_bones
    def configure_master_control(self):
        pass

    @stage.generate_widgets
    def make_master_control_widget(self):
        master = self.bones.ctrl.master
        set_bone_widget_transform(self.obj, master, self.get_master_control_output())
        create_cube_widget(self.obj, master, radius=0.5)

    ####################################################
    # ORG bones

    @stage.parent_bones
    def parent_org_chain(self):
        ctrl = self.bones.ctrl
        org = self.bones.org
        for tweak, org in zip(ctrl.tweak, org):
            self.set_bone_parent(org, tweak)

    def rig_org_bone(self, i, org, tweak, next_tweak):
        # For spine rigs, these constraints go on the deformation bones. See T74483#902192.
        pass

    ####################################################
    # Tweak bones

    @stage.configure_bones
    def configure_tweak_chain(self):
        super().configure_tweak_chain()

        ControlLayersOption.TWEAK.assign(self.params, self.obj, self.bones.ctrl.tweak)

    ####################################################
    # Deform bones

    @stage.rig_bones
    def rig_deform_chain(self):
        tweaks = self.bones.ctrl.tweak
        for args in zip(count(0), self.bones.deform, tweaks, tweaks[1:]):
            self.rig_deform_bone(*args)

    # noinspection PyMethodOverriding
    def rig_deform_bone(self, i: int, deform: str, tweak: str, next_tweak: Optional[str]):
        self.make_constraint(deform, 'COPY_TRANSFORMS', tweak)
        if next_tweak:
            self.make_constraint(deform, 'STRETCH_TO', next_tweak, keep_axis='SWING_Y')

    @stage.configure_bones
    def configure_bbone_chain(self):
        self.get_bone(self.bones.deform[0]).bone.bbone_easein = 0.0

    ####################################################
    # SETTINGS

    @classmethod
    def add_parameters(cls, params):
        params.make_custom_pivot = bpy.props.BoolProperty(
            name="Custom Pivot Control",
            default=False,
            description="Create a rotation pivot control that can be repositioned arbitrarily"
        )

        # Setting up extra layers for the FK and tweak
        ControlLayersOption.TWEAK.add_parameters(params)

    @classmethod
    def parameters_ui(cls, layout, params):
        layout.prop(params, 'make_custom_pivot')

        ControlLayersOption.TWEAK.parameters_ui(layout, params)


class BaseHeadTailRig(ConnectingChainRig):
    """ Base for head and tail rigs. """

    class RotationBone(NamedTuple):
        org: str
        name: str
        bone: str
        def_val: float     # Default value of the follow slider
        copy_scale: bool

    rotation_bones: list[RotationBone]

    follow_bone: str
    default_prop_bone: str
    prop_bone: str

    def initialize(self):
        super().initialize()

        self.rotation_bones = []

    ####################################################
    # Utilities

    def get_parent_master(self, default_bone: str) -> str:
        """ Return the parent's master control bone if connecting and found. """

        if self.use_connect_chain and 'master' in self.rigify_parent.bones.ctrl:
            return self.rigify_parent.bones.ctrl.master
        else:
            return default_bone

    def get_parent_master_panel(self, default_bone: str):
        """ Return the parent's master control bone if connecting and found, and script panel. """

        controls = self.bones.ctrl.flatten()
        prop_bone = self.get_parent_master(default_bone)

        if prop_bone != default_bone:
            owner = self.rigify_parent
            controls += self.rigify_parent.bones.ctrl.flatten()
        else:
            owner = self

        return prop_bone, self.script.panel_with_selected_check(owner, controls)

    ####################################################
    # Rotation follow

    def make_mch_follow_bone(self, org: str, name: str, def_val: float, *, copy_scale=False):
        bone = self.copy_bone(org, make_derived_name('ROT-'+name, 'mch'), parent=True)
        self.rotation_bones.append(self.RotationBone(org, name, bone, def_val, copy_scale))
        return bone

    @stage.parent_bones
    def align_mch_follow_bones(self):
        self.follow_bone = self.get_parent_master('root')

        for org, name, bone, def_val, copy_scale in self.rotation_bones:
            align_bone_orientation(self.obj, bone, self.follow_bone)

    @stage.configure_bones
    def configure_mch_follow_bones(self):
        self.prop_bone, panel = self.get_parent_master_panel(self.default_prop_bone)

        for org, name, bone, def_val, copy_scale in self.rotation_bones:
            text_name = name.replace('_', ' ').title() + ' Follow'

            self.make_property(self.prop_bone, name+'_follow', default=float(def_val))
            panel.custom_prop(self.prop_bone, name+'_follow', text=text_name, slider=True)

    @stage.rig_bones
    def rig_mch_follow_bones(self):
        for org, name, bone, def_val, copy_scale in self.rotation_bones:
            self.rig_mch_rotation_bone(bone, name+'_follow', copy_scale)

    def rig_mch_rotation_bone(self, mch: str, prop_name: str, copy_scale: bool):
        con = self.make_constraint(mch, 'COPY_ROTATION', self.follow_bone)

        self.make_driver(con, 'influence',
                         variables=[(self.prop_bone, prop_name)], polynomial=[1, -1])

        if copy_scale:
            self.make_constraint(mch, 'COPY_SCALE', self.follow_bone)

    ####################################################
    # Tweak chain

    @stage.configure_bones
    def configure_tweak_chain(self):
        super().configure_tweak_chain()

        ControlLayersOption.TWEAK.assign(self.params, self.obj, self.bones.ctrl.tweak)

    ####################################################
    # Settings

    @classmethod
    def add_parameters(cls, params):
        super().add_parameters(params)

        # Setting up extra layers for the FK and tweak
        ControlLayersOption.TWEAK.add_parameters(params)

    @classmethod
    def parameters_ui(cls, layout, params):
        super().parameters_ui(layout, params)

        ControlLayersOption.TWEAK.parameters_ui(layout, params)
