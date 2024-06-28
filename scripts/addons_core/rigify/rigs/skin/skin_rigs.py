# SPDX-FileCopyrightText: 2021-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy

from typing import TYPE_CHECKING, Optional

from mathutils import Quaternion

from ...utils.bones import BoneDict
from ...utils.naming import make_derived_name
from ...utils.misc import force_lazy, ArmatureObject

from ...base_rig import BaseRig

from .skin_parents import ControlBoneParentOrg, ControlBoneParentBase

if TYPE_CHECKING:
    from .skin_nodes import BaseSkinNode, ControlBoneNode


# noinspection PyMethodMayBeStatic
class BaseSkinRig(BaseRig):
    """
    Base type for all rigs involved in the skin system.
    This includes chain rigs and the parent provider rigs.
    """

    rig_parent_bone: str

    def initialize(self):
        self.rig_parent_bone = self.get_bone_parent(self.base_bone)

    ##########################
    # BONES

    bones: BaseRig.ToplevelBones[
        str | list[str] | BoneDict,
        BaseRig.CtrlBones,
        BaseRig.MchBones,
        str | list[str] | BoneDict,
    ]

    ##########################
    # Utilities

    def get_parent_skin_rig(self) -> Optional['BaseSkinRig']:
        """Find the closest BaseSkinRig parent."""
        parent = self.rigify_parent

        while parent:
            if isinstance(parent, BaseSkinRig):
                return parent
            parent = parent.rigify_parent

        return None

    def get_all_parent_skin_rigs(self) -> list['BaseSkinRig']:
        """Get a list of all BaseSkinRig parents, starting with this rig."""
        items = []
        current = self
        while current:
            items.append(current)
            current = current.get_parent_skin_rig()
        return items

    def get_child_chain_parent_next(self, rig: 'BaseSkinRig') -> str:
        """
        Retrieves the parent bone for the child chain rig
        as determined by the parent skin rig.
        """
        if isinstance(self.rigify_parent, BaseSkinRig):
            return self.rigify_parent.get_child_chain_parent(rig, self.rig_parent_bone)
        else:
            return self.rig_parent_bone

    def build_control_node_parent_next(self, node: 'BaseSkinNode') -> ControlBoneParentBase:
        """
        Retrieves the parent mechanism generator for the child control node
        as determined by the parent skin rig.
        """
        if isinstance(self.rigify_parent, BaseSkinRig):
            return self.rigify_parent.build_control_node_parent(node, self.rig_parent_bone)
        else:
            return ControlBoneParentOrg(self.rig_parent_bone)

    ##########################
    # Methods to override

    def get_child_chain_parent(self, rig: 'BaseSkinRig', parent_bone: str) -> str:
        """
        Returns the (lazy) parent bone to use for the given child chain rig.
        The parent_bone argument specifies the actual parent bone from caller.
        """
        return parent_bone

    def build_control_node_parent(self, node: 'BaseSkinNode',
                                  parent_bone: str) -> ControlBoneParentBase:
        """
        Returns the parent mechanism generator for the child control node.
        The parent_bone argument specifies the actual parent bone from caller.
        Called during the initialize stage.
        """
        return ControlBoneParentOrg(self.get_child_chain_parent(node.rig, parent_bone))

    def build_own_control_node_parent(self, node: 'BaseSkinNode') -> ControlBoneParentBase:
        """
        Returns the parent mechanism generator for nodes directly owned by this rig.
        Called during the initialize stage.
        """
        return self.build_control_node_parent_next(node)

    def extend_control_node_parent(self, parent: ControlBoneParentBase,
                                   _node: 'BaseSkinNode') -> ControlBoneParentBase:
        """
        First callback pass of adjustments to the parent mechanism generator for the given node.
        Called for all BaseSkinRig parents in parent to child order during the initialize stage.
        """
        return parent

    def extend_control_node_parent_post(self, parent: ControlBoneParentBase,
                                        _node: 'BaseSkinNode') -> ControlBoneParentBase:
        """
        Second callback pass of adjustments to the parent mechanism generator for the given node.
        Called for all BaseSkinRig parents in child to parent order during the initialize stage.
        """
        return parent

    def extend_control_node_rig(self, _node: 'ControlBoneNode'):
        """
        A callback pass for adding constraints directly to the generated control.
        Called for all BaseSkinRig parents in parent to child order during the rig stage.
        """
        pass


def get_bone_quaternion(obj: ArmatureObject, bone: str):
    return obj.pose.bones[bone].bone.matrix_local.to_quaternion()


class BaseSkinChainRig(BaseSkinRig):
    """
    Base type for all skin rigs that can own control nodes, rather than
    only modifying nodes of their children or other rigs.
    """

    chain_priority: float = 0

    def initialize(self):
        super().initialize()

        if type(self).chain_priority is None:
            self.chain_priority = self.params.skin_chain_priority

    def parent_bones(self):
        self.rig_parent_bone = force_lazy(self.get_child_chain_parent_next(self))

    def get_final_control_node_rotation(self, node: 'ControlBoneNode') -> Quaternion:
        """Returns the orientation to use for the given control node owned by this rig."""
        return self.get_control_node_rotation(node)

    ##########################
    # Methods to override

    def get_control_node_rotation(self, node: 'ControlBoneNode') -> Quaternion:
        """
        Returns the rig-specific orientation to use for the given control node of this rig,
        if not overridden by the Orientation Bone option.
        """
        return get_bone_quaternion(self.obj, self.base_bone)

    def get_control_node_layers(self, node: 'ControlBoneNode') -> list[bpy.types.BoneCollection]:
        """Returns the armature layers to use for the given control node owned by this rig."""
        return list(self.get_bone(self.base_bone).bone.collections)

    def make_control_node_widget(self, node: 'ControlBoneNode'):
        """Called to generate the widget for nodes with ControlNodeIcon.CUSTOM."""
        raise NotImplementedError()

    ##########################
    # UI

    @classmethod
    def add_parameters(cls, params):
        params.skin_chain_priority = bpy.props.IntProperty(
            name='Chain Priority',
            min=-10, max=10, default=0,
            description='When merging controls, chains with higher priority always win'
        )

    @classmethod
    def parameters_ui(cls, layout, params):
        if cls.chain_priority is None:
            layout.prop(params, "skin_chain_priority")


# noinspection PyAbstractClass
class BaseSkinChainRigWithRotationOption(BaseSkinChainRig):
    """
    Skin chain rig with an option to override the orientation to use
    for controls via specifying an arbitrary template bone.
    """

    use_skin_control_orientation_bone = True

    def get_final_control_node_rotation(self, node: 'ControlBoneNode') -> Quaternion:
        bone_name = self.params.skin_control_orientation_bone

        if bone_name and self.use_skin_control_orientation_bone:
            # Retrieve the orientation from the specified ORG bone
            try:
                org_name = make_derived_name(bone_name, 'org')

                if org_name not in self.obj.pose.bones:
                    org_name = bone_name

                return get_bone_quaternion(self.obj, org_name)

            except KeyError:
                self.raise_error('Could not find orientation bone {}', bone_name)

        else:
            # Use the rig-specific orientation
            return self.get_control_node_rotation(node)

    @classmethod
    def add_parameters(cls, params):
        params.skin_control_orientation_bone = bpy.props.StringProperty(
            name="Orientation Bone",
            description="If set, control orientation is taken from the specified bone",
        )

        super().add_parameters(params)

    @classmethod
    def parameters_ui(cls, layout, params):
        if cls.use_skin_control_orientation_bone:
            from rigify.operators.copy_mirror_parameters import make_copy_parameter_button

            row = layout.row()
            row.prop_search(params, "skin_control_orientation_bone",
                            bpy.context.active_object.pose, "bones", text="Orientation")

            make_copy_parameter_button(
                row, "skin_control_orientation_bone", mirror_bone=True,
                base_class=BaseSkinChainRigWithRotationOption
            )

        super().parameters_ui(layout, params)
