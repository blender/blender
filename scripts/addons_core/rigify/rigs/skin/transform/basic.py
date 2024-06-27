# SPDX-FileCopyrightText: 2021-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy

from bpy.types import PoseBone
from mathutils import Quaternion

from ..skin_nodes import BaseSkinNode
from ....utils.naming import make_derived_name
from ....utils.widgets_basic import create_cube_widget
from ....utils.misc import LazyRef, Lazy

from ....base_rig import stage

from ..skin_parents import ControlBoneParentArmature, ControlBoneParentBase
from ..skin_rigs import BaseSkinRig


class Rig(BaseSkinRig):
    """
    This rig transforms its child nodes' locations, but keeps
    their rotation and scale stable. This demonstrates implementing
    a basic parent controller rig.
    """

    def find_org_bones(self, bone: PoseBone) -> str:
        return bone.name

    make_control: bool
    input_ref: Lazy[str]
    transform_orientation: Quaternion

    def initialize(self):
        super().initialize()

        self.make_control = self.params.make_control

        # Choose the parent bone for the child nodes
        if self.make_control:
            self.input_ref = LazyRef(self.bones.ctrl, 'master')
        else:
            self.input_ref = self.base_bone

        # Retrieve the orientation of the control
        matrix = self.get_bone(self.base_bone).bone.matrix_local

        self.transform_orientation = matrix.to_quaternion()

    ####################################################
    # Control Nodes

    def build_control_node_parent(self, node: BaseSkinNode,
                                  parent_bone: str) -> ControlBoneParentBase:
        # Parent nodes to the control bone, but isolate rotation and scale
        return ControlBoneParentArmature(
            self, node, bones=[self.input_ref],
            orientation=self.transform_orientation,
            copy_scale=LazyRef(self.bones.mch, 'template'),
            copy_rotation=LazyRef(self.bones.mch, 'template'),
        )

    def get_child_chain_parent(self, rig: BaseSkinRig, parent_bone: str) -> str:
        # Forward child chain parenting to the next rig, so that
        # only control nodes are affected by this one.
        return self.get_child_chain_parent_next(rig)

    ####################################################
    # BONES

    class CtrlBones(BaseSkinRig.CtrlBones):
        master: str                    # Master control

    class MchBones(BaseSkinRig.MchBones):
        template: str                  # Bone used to lock rotation and scale of child nodes.

    bones: BaseSkinRig.ToplevelBones[
        str,
        'Rig.CtrlBones',
        'Rig.MchBones',
        str
    ]

    ####################################################
    # Master control

    @stage.generate_bones
    def make_master_control(self):
        if self.make_control:
            self.bones.ctrl.master = self.copy_bone(
                self.bones.org, make_derived_name(self.bones.org, 'ctrl'), parent=True)

    @stage.configure_bones
    def configure_master_control(self):
        if self.make_control:
            self.copy_bone_properties(self.bones.org, self.bones.ctrl.master)

    @stage.generate_widgets
    def make_master_control_widget(self):
        if self.make_control:
            create_cube_widget(self.obj, self.bones.ctrl.master)

    ####################################################
    # Template MCH

    @stage.generate_bones
    def make_mch_template_bone(self):
        self.bones.mch.template = self.copy_bone(
            self.bones.org, make_derived_name(self.bones.org, 'mch', '_orient'), parent=True)

    @stage.parent_bones
    def parent_mch_template_bone(self):
        self.set_bone_parent(self.bones.mch.template, self.get_child_chain_parent_next(self))

    ####################################################
    # ORG bone

    @stage.rig_bones
    def rig_org_bone(self):
        pass

    ####################################################
    # SETTINGS

    @classmethod
    def add_parameters(cls, params):
        params.make_control = bpy.props.BoolProperty(
            name="Control",
            default=True,
            description="Create a control bone for the copy"
        )

    @classmethod
    def parameters_ui(cls, layout, params):
        layout.prop(params, "make_control", text="Generate Control")


def create_sample(obj):
    from rigify.rigs.basic.super_copy import create_sample as inner
    obj.pose.bones[inner(obj)["Bone"]].rigify_type = 'skin.transform.basic'
