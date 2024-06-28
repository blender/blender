# SPDX-FileCopyrightText: 2021-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy

from bpy.types import PoseBone

from ...utils.naming import make_derived_name
from ...utils.widgets import layout_widget_dropdown, create_registered_widget
from ...utils.mechanism import move_all_constraints

from ...base_rig import stage

from .skin_nodes import ControlBoneNode, ControlNodeIcon, ControlNodeEnd
from .skin_rigs import BaseSkinChainRigWithRotationOption

from ..basic.raw_copy import RelinkConstraintsMixin


class Rig(BaseSkinChainRigWithRotationOption, RelinkConstraintsMixin):
    """Custom skin control node."""

    chain_priority = 20

    make_deform: bool

    def find_org_bones(self, bone: PoseBone) -> str:
        return bone.name

    def initialize(self):
        super().initialize()

        self.make_deform = self.params.make_extra_deform

    ####################################################
    # BONES

    bones: BaseSkinChainRigWithRotationOption.ToplevelBones[
        str,
        'Rig.CtrlBones',
        'Rig.MchBones',
        str
    ]

    ####################################################
    # CONTROL NODES

    control_node: ControlBoneNode

    @stage.initialize
    def init_control_nodes(self):
        org = self.bones.org
        name = make_derived_name(org, 'ctrl')

        self.control_node = node = ControlBoneNode(
            self, org, name, icon=ControlNodeIcon.CUSTOM, chain_end=ControlNodeEnd.START)

        node.hide_control = self.params.skin_anchor_hide

    def make_control_node_widget(self, node):
        create_registered_widget(self.obj, node.control_bone,
                                 self.params.pivot_master_widget_type or 'cube')

    def extend_control_node_rig(self, node):
        if node.rig == self:
            org = self.bones.org

            self.copy_bone_properties(org, node.control_bone)

            self.relink_bone_constraints(org)

            move_all_constraints(self.obj, org, node.control_bone)

    ##############################
    # ORG chain

    @stage.parent_bones
    def parent_org_chain(self):
        self.set_bone_parent(self.bones.org, self.control_node.control_bone)

    ##############################
    # Deform bone

    @stage.generate_bones
    def make_deform_bone(self):
        if self.make_deform:
            self.bones.deform = self.copy_bone(
                self.bones.org, make_derived_name(self.bones.org, 'def'))

    @stage.parent_bones
    def parent_deform_chain(self):
        if self.make_deform:
            self.set_bone_parent(self.bones.deform, self.bones.org)

    ####################################################
    # SETTINGS

    @classmethod
    def add_parameters(cls, params):
        params.make_extra_deform = bpy.props.BoolProperty(
            name="Extra Deform",
            default=False,
            description="Create an optional deform bone"
        )

        params.skin_anchor_hide = bpy.props.BoolProperty(
            name='Suppress Control',
            default=False,
            description='Make the control bone a mechanism bone invisible to the user and only affected by constraints'
        )

        params.pivot_master_widget_type = bpy.props.StringProperty(
            name="Widget Type",
            default='cube',
            description="Choose the type of the widget to create"
        )

        cls.add_relink_constraints_params(params)

        super().add_parameters(params)

    @classmethod
    def parameters_ui(cls, layout, params):
        col = layout.column()
        col.prop(params, "make_extra_deform", text='Generate Deform Bone')
        col.prop(params, "skin_anchor_hide")

        row = layout.row()
        row.active = not params.skin_anchor_hide
        layout_widget_dropdown(row, params, "pivot_master_widget_type")

        layout.prop(params, "relink_constraints")

        layout.label(text="All constraints are moved to the control bone.", icon='INFO')

        super().parameters_ui(layout, params)


def create_sample(obj):
    from rigify.rigs.basic.super_copy import create_sample as inner
    obj.pose.bones[inner(obj)["Bone"]].rigify_type = 'skin.anchor'
