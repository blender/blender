# SPDX-FileCopyrightText: 2021-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
import math

from typing import Sequence, Optional
from itertools import count, repeat

from bpy.types import PoseBone
from mathutils import Quaternion

from math import acos
from bl_math import smoothstep

from ...utils.rig import connected_children_names, rig_is_child
from ...utils.naming import make_derived_name
from ...utils.bones import align_bone_roll
from ...utils.mechanism import driver_var_distance
from ...utils.widgets_basic import create_sphere_widget
from ...utils.misc import map_list, matrix_from_axis_roll

from ...base_rig import stage

from .skin_nodes import ControlBoneNode, ControlNodeEnd
from .skin_rigs import BaseSkinChainRigWithRotationOption, get_bone_quaternion


class Rig(BaseSkinChainRigWithRotationOption):
    """
    Base deform rig of the skin system, implementing a B-Bone chain without
    any automation on the control nodes.
    """

    chain_priority = None

    bbone_segments: int
    use_bbones: bool
    use_connect_mirror: Sequence[bool]
    use_connect_ends: Sequence[bool]
    use_scale: bool
    use_reparent_handles: bool

    num_orgs: int
    length: float

    def find_org_bones(self, bone: PoseBone) -> list[str]:
        return [bone.name] + connected_children_names(self.obj, bone.name)

    def initialize(self):
        super().initialize()

        self.bbone_segments = self.params.bbones
        self.use_bbones = self.bbone_segments > 1
        self.use_connect_mirror = self.params.skin_chain_connect_mirror
        self.use_connect_ends = self.params.skin_chain_connect_ends
        self.use_scale = any(self.params.skin_chain_use_scale)
        self.use_reparent_handles = self.params.skin_chain_use_reparent

        orgs = self.bones.org

        self.num_orgs = len(orgs)
        self.length = sum([self.get_bone(b).length for b in orgs]) / len(orgs)

    ####################################################
    # OVERRIDES

    def get_control_node_rotation(self, node: ControlBoneNode) -> Quaternion:
        """Compute the chain-aligned control orientation."""
        orgs = self.bones.org

        # Average the adjoining org bone orientations
        bones = orgs[max(0, node.index-1):node.index+1]
        quaternions = [get_bone_quaternion(self.obj, name) for name in bones]
        result = sum(quaternions, Quaternion((0, 0, 0, 0))).normalized()

        # For end bones, align to the connected chain tangent
        if node.index in (0, self.num_orgs):
            chain = self.get_node_chain_with_mirror()
            node_prev = chain[node.index]
            node_next = chain[node.index+2]

            if node_prev and node_next:
                # Apply only swing to preserve roll; tgt roll thus doesn't matter
                tgt = matrix_from_axis_roll(node_next.point - node_prev.point, 0).to_quaternion()
                swing, _ = (result.inverted() @ tgt).to_swing_twist('Y')
                result = result @ swing

        return result

    def get_all_controls(self) -> list[str]:
        return [node.control_bone for node in self.control_nodes]

    ####################################################
    # BONES

    class MchBones(BaseSkinChainRigWithRotationOption.MchBones):
        handles: list[str]             # Final B-Bone handles.
        handles_pre: list[str]         # Mechanism bones that emulate Auto handle behavior.

    bones: BaseSkinChainRigWithRotationOption.ToplevelBones[
        list[str],
        'Rig.CtrlBones',
        'Rig.MchBones',
        list[str]
    ]

    ####################################################
    # CONTROL NODES

    control_nodes: list[ControlBoneNode]

    # List of control nodes extended with the two adjacent chained nodes below
    control_node_chain: Optional[list[ControlBoneNode | None]]

    # Connected chain continuation nodes, and corner setting values
    prev_node: Optional[ControlBoneNode]
    prev_corner: float
    next_node: Optional[ControlBoneNode]
    next_corner: float

    # Next chained rig if the end connects to the start of another chain
    next_chain_rig: Optional['Rig']

    @stage.initialize
    def init_control_nodes(self):
        orgs = self.bones.org

        self.control_nodes = nodes = [
            # Bone head nodes
            *map_list(self.make_control_node, count(0), orgs, repeat(False)),
            # Tail of the final bone
            self.make_control_node(len(orgs), orgs[-1], True),
        ]

        self.control_node_chain = None

        nodes[0].chain_end_neighbor = nodes[1]
        nodes[-1].chain_end_neighbor = nodes[-2]

    def make_control_node(self, i: int, org: str, is_end: bool) -> ControlBoneNode:
        bone = self.get_bone(org)
        name = make_derived_name(org, 'ctrl', '_end' if is_end else '')
        pos = bone.tail if is_end else bone.head

        if i == 0:
            chain_end = ControlNodeEnd.START
        elif is_end:
            chain_end = ControlNodeEnd.END
        else:
            chain_end = ControlNodeEnd.MIDDLE

        return ControlBoneNode(
            self, org, name, point=pos, size=self.length/3, index=i,
            allow_scale=self.use_scale, needs_reparent=self.use_reparent_handles,
            chain_end=chain_end,
        )

    def make_control_node_widget(self, node: ControlBoneNode):
        create_sphere_widget(self.obj, node.control_bone)

    ####################################################
    # B-Bone handle MCH

    # Generate two layers of handle bones, 'pre' for the auto handle mechanism,
    # and final handles combining that with user transformation. This flag may
    # be enabled by parent controller rigs when needed in order to be able to
    # inject more automatic handle positioning mechanisms.
    use_pre_handles = False

    def get_connected_node(self, node: ControlBoneNode
                           ) -> tuple[Optional[ControlBoneNode], Optional[ControlBoneNode], float]:
        """
        Find which other chain to connect this chain to at this node.

        Returns:
            (Connected counterpart node, Its chain neighbour, Average sharp angle setting value)
        """
        is_end = 1 if node.index != 0 else 0
        corner: float = self.params.skin_chain_connect_sharp_angle[is_end]

        # First try merging through mirror
        if self.use_connect_mirror[is_end]:
            mirror = node.get_best_mirror()

            if mirror and mirror.chain_end_neighbor and isinstance(mirror.rig, Rig):
                # Connect the same chain end
                s_is_end = 1 if mirror.index != 0 else 0

                if is_end == s_is_end and mirror.rig.use_connect_mirror[is_end]:
                    mirror_corner = mirror.rig.params.skin_chain_connect_sharp_angle[is_end]

                    return mirror, mirror.chain_end_neighbor, (corner + mirror_corner)/2

        # Then try connecting ends
        if self.use_connect_ends[is_end]:
            # Find chains that want to connect ends at this node group
            groups = ([], [])

            for sibling in node.get_merged_siblings():
                if isinstance(sibling.rig, Rig) and sibling.chain_end_neighbor:
                    s_is_end = 1 if sibling.index != 0 else 0

                    if sibling.rig.use_connect_ends[s_is_end]:
                        groups[s_is_end].append(sibling)

            # Only connect if the pairing is unambiguous
            if len(groups[0]) == 1 and len(groups[1]) == 1:
                assert node == groups[is_end][0]

                link = groups[1 - is_end][0]
                link_corner = link.rig.params.skin_chain_connect_sharp_angle[1 - is_end]

                return link, link.chain_end_neighbor, (corner + link_corner)/2

        return None, None, 0

    def get_node_chain_with_mirror(self):
        """Get node chain with connected node extensions at the ends."""
        if self.control_node_chain is not None:
            return self.control_node_chain

        nodes = self.control_nodes
        prev_link, self.prev_node, self.prev_corner = self.get_connected_node(nodes[0])
        next_link, self.next_node, self.next_corner = self.get_connected_node(nodes[-1])

        self.control_node_chain = [self.prev_node, *nodes, self.next_node]

        # Optimize connect next by sharing last handle mch
        if next_link and next_link.index == 0:
            assert isinstance(next_link.rig, Rig)
            self.next_chain_rig = next_link.rig
        else:
            self.next_chain_rig = None

        return self.control_node_chain

    def get_all_mch_handles(self) -> list[str]:
        """Returns the list of all handle bones, referencing the next chained rig if needed."""
        if self.next_chain_rig:
            return self.bones.mch.handles + [self.next_chain_rig.bones.mch.handles[0]]
        else:
            return self.bones.mch.handles

    def get_all_mch_handles_pre(self):
        """Returns the list of all pre-handle bones, referencing the next chained rig if needed."""
        if self.next_chain_rig:
            return self.bones.mch.handles_pre + [self.next_chain_rig.bones.mch.handles_pre[0]]
        else:
            return self.bones.mch.handles_pre

    @stage.generate_bones
    def make_mch_handle_bones(self):
        if self.use_bbones:
            mch = self.bones.mch
            chain = self.get_node_chain_with_mirror()

            # If the last handle mch will be shared, drop it from chain
            if self.next_chain_rig:
                chain = chain[0:-1]

            mch.handles = map_list(self.make_mch_handle_bone, count(0),
                                   chain, chain[1:], chain[2:])

            if self.use_pre_handles:
                mch.handles_pre = map_list(self.make_mch_pre_handle_bone, count(0), mch.handles)
            else:
                mch.handles_pre = mch.handles

    def make_mch_handle_bone(self, _i: int,
                             prev_node: Optional[ControlBoneNode],
                             node: ControlBoneNode,
                             next_node: Optional[ControlBoneNode]
                             ) -> str:
        name = self.copy_bone(node.org, make_derived_name(node.name, 'mch', '_handle'))

        handle_start = prev_node or node
        handle_end = next_node or node
        handle_axis = (handle_end.point - handle_start.point).normalized()

        bone = self.get_bone(name)
        bone.tail = bone.head + handle_axis * self.length * 3/4

        align_bone_roll(self.obj, name, node.org)
        return name

    def make_mch_pre_handle_bone(self, _i: int, handle: str) -> str:
        return self.copy_bone(handle, make_derived_name(handle, 'mch', '_pre'))

    @stage.parent_bones
    def parent_mch_handle_bones(self):
        if self.use_bbones:
            mch = self.bones.mch

            if self.use_pre_handles:
                for pre in mch.handles_pre:
                    self.set_bone_parent(pre, self.rig_parent_bone, inherit_scale='AVERAGE')

            for handle in mch.handles:
                self.set_bone_parent(handle, self.rig_parent_bone, inherit_scale='AVERAGE')

    @stage.rig_bones
    def rig_mch_handle_bones(self):
        if self.use_bbones:
            mch = self.bones.mch
            chain = self.get_node_chain_with_mirror()

            # Rig Auto-handle emulation (on pre handles)
            for args in zip(count(0), mch.handles_pre, chain, chain[1:], chain[2:]):
                self.rig_mch_handle_auto(*args)

            # Apply user transformation to the final handles
            for args in zip(count(0), mch.handles, chain, chain[1:], chain[2:], mch.handles_pre):
                self.rig_mch_handle_user(*args)

    def rig_mch_handle_auto(self, _i: int, mch: str,
                            prev_node: Optional[ControlBoneNode],
                            node: ControlBoneNode,
                            next_node: Optional[ControlBoneNode]):
        handle_start = prev_node or node
        handle_end = next_node or node

        # Emulate auto handle
        self.make_constraint(mch, 'COPY_LOCATION', handle_start.control_bone, name='locate_prev')
        self.make_constraint(mch, 'DAMPED_TRACK', handle_end.control_bone, name='track_next')

    def rig_mch_handle_user(self, _i: int, mch: str,
                            prev_node: Optional[ControlBoneNode],
                            node: ControlBoneNode,
                            next_node: Optional[ControlBoneNode],
                            pre: str):
        # Copy from the pre handle if used. Before Full is used to allow
        # drivers on local transform channels to still work.
        if pre != mch:
            self.make_constraint(
                mch, 'COPY_TRANSFORMS', pre, name='copy_pre',
                space='LOCAL', mix_mode='BEFORE_FULL',
            )

        # Apply user rotation and scale.
        # If the node belongs to a parent of this rig, there is a good chance this
        # may cause weird double transformation, so skip it in that case.
        if not rig_is_child(self, node.merged_master.rig, strict=True):
            input_bone = node.reparent_bone if self.use_reparent_handles else node.control_bone

            self.make_constraint(
                mch, 'COPY_TRANSFORMS', input_bone, name='copy_user',
                target_space='LOCAL_OWNER_ORIENT', owner_space='LOCAL',
                mix_mode='BEFORE_FULL',
            )

        # Remove any shear created by the previous steps
        self.make_constraint(mch, 'LIMIT_ROTATION', name='remove_shear')

    ##############################
    # ORG chain

    @stage.parent_bones
    def parent_org_chain(self):
        orgs = self.bones.org
        self.set_bone_parent(orgs[0], self.rig_parent_bone, inherit_scale='AVERAGE')
        self.parent_bone_chain(orgs, use_connect=True, inherit_scale='AVERAGE')

    @stage.rig_bones
    def rig_org_chain(self):
        for args in zip(count(0), self.bones.org, self.control_nodes, self.control_nodes[1:]):
            self.rig_org_bone(*args)

    def rig_org_bone(self, i: int, org: str, node: ControlBoneNode, next_node: ControlBoneNode):
        if i == 0:
            self.make_constraint(org, 'COPY_LOCATION', node.control_bone)

        self.make_constraint(org, 'STRETCH_TO', next_node.control_bone, keep_axis='SWING_Y')

    ##############################
    # Deform chain

    @stage.generate_bones
    def make_deform_chain(self):
        self.bones.deform = map_list(self.make_deform_bone, count(0), self.bones.org)

    def make_deform_bone(self, _i: int, org: str):
        name = self.copy_bone(org, make_derived_name(org, 'def'), bbone=True)
        self.get_bone(name).bbone_segments = self.bbone_segments
        return name

    @stage.parent_bones
    def parent_deform_chain(self):
        deform = self.bones.deform

        self.set_bone_parent(deform[0], self.rig_parent_bone, inherit_scale='AVERAGE')
        self.parent_bone_chain(deform, use_connect=True, inherit_scale='AVERAGE')

        if self.use_bbones:
            handles = self.get_all_mch_handles()

            for name, start_handle, end_handle in zip(deform, handles, handles[1:]):
                bone = self.get_bone(name)
                bone.bbone_handle_type_start = 'TANGENT'
                bone.bbone_custom_handle_start = self.get_bone(start_handle)
                bone.bbone_handle_type_end = 'TANGENT'
                bone.bbone_custom_handle_end = self.get_bone(end_handle)

                if self.use_scale:
                    bone.bbone_handle_use_scale_start = self.params.skin_chain_use_scale[0:3]
                    bone.bbone_handle_use_scale_end = self.params.skin_chain_use_scale[0:3]

                    bone.bbone_handle_use_ease_start = self.params.skin_chain_use_scale[3]
                    bone.bbone_handle_use_ease_end = self.params.skin_chain_use_scale[3]

    @stage.rig_bones
    def rig_deform_chain(self):
        for args in zip(count(0), self.bones.deform, self.bones.org):
            self.rig_deform_bone(*args)

    def rig_deform_bone(self, i: int, deform: str, org: str):
        self.make_constraint(deform, 'COPY_TRANSFORMS', org)

        if self.use_bbones:
            if i == 0 and self.prev_corner > 1e-3:
                self.make_corner_driver(
                    deform, 'bbone_easein',
                    self.control_nodes[0], self.control_nodes[1],
                    self.prev_node, self.prev_corner
                )

            elif i == self.num_orgs-1 and self.next_corner > 1e-3:
                self.make_corner_driver(
                    deform, 'bbone_easeout',
                    self.control_nodes[-1], self.control_nodes[-2],
                    self.next_node, self.next_corner
                )

    def make_corner_driver(self, bbone: str, field: str,
                           corner_node: ControlBoneNode,
                           next_node1: ControlBoneNode, next_node2: ControlBoneNode,
                           angle_threshold: float):
        """
        Create a driver adjusting B-Bone Ease based on the angle between controls,
        gradually making the corner sharper when the angle drops below the threshold.
        """
        pbone = self.get_bone(bbone)

        a = (corner_node.point - next_node1.point).length
        b = (corner_node.point - next_node2.point).length
        c = (next_node1.point - next_node2.point).length

        var_map = {
            'a': driver_var_distance(
                self.obj, bone1=corner_node.control_bone, bone2=next_node1.control_bone),
            'b': driver_var_distance(
                self.obj, bone1=corner_node.control_bone, bone2=next_node2.control_bone),
            'c': driver_var_distance(
                self.obj, bone1=next_node1.control_bone, bone2=next_node2.control_bone),
        }

        # Compute and set the ease in rest pose
        init_val = -1+2*smoothstep(-1, 1, acos((a*a+b*b-c*c)/max(2*a*b, 1e-10)) / angle_threshold)

        setattr(pbone.bone, field, init_val)

        # Create the actual driver
        bias = -1 - init_val

        expr = f'{bias}+2*smoothstep(-1,1,acos((a*a+b*b-c*c)/max(2*a*b,1e-10))/{angle_threshold})'

        self.make_driver(pbone, field, expression=expr, variables=var_map)

    ####################################################
    # SETTINGS

    @classmethod
    def add_parameters(cls, params):
        params.bbones = bpy.props.IntProperty(
            name='B-Bone Segments',
            default=10,
            min=1,
            description='Number of B-Bone segments'
        )

        params.skin_chain_use_reparent = bpy.props.BoolProperty(
            name='Merge Parent Rotation And Scale',
            default=False,
            description='When controls are merged into ones owned by other chains, include '
                        'parent-induced rotation/scale difference into handle motion. Otherwise '
                        'only local motion of the control bone is used',
        )

        params.skin_chain_use_scale = bpy.props.BoolVectorProperty(
            size=4,
            name='Use Handle Scale',
            default=(False, False, False, False),
            description='Use control scaling to scale the B-Bone'
        )

        params.skin_chain_connect_mirror = bpy.props.BoolVectorProperty(
            size=2,
            name='Connect With Mirror',
            default=(True, True),
            description='Create a smooth B-Bone transition if an end of the chain meets its mirror'
        )

        params.skin_chain_connect_sharp_angle = bpy.props.FloatVectorProperty(
            size=2,
            name='Sharpen Corner',
            default=(0, 0),
            min=0,
            max=math.pi,
            description='Create a mechanism to sharpen a connected corner when the angle is '
                        'below this value',
            unit='ROTATION',
        )

        params.skin_chain_connect_ends = bpy.props.BoolVectorProperty(
            size=2,
            name='Connect Matching Ends',
            default=(False, False),
            description='Create a smooth B-Bone transition if an end of the chain meets another '
                        'chain going in the same direction'
        )

        super().add_parameters(params)

    @classmethod
    def parameters_ui(cls, layout, params):
        layout.prop(params, "bbones")

        col = layout.column()
        col.active = params.bbones > 1

        col.prop(params, "skin_chain_use_reparent")

        row = col.split(factor=0.3)
        row.label(text="Use Scale:")
        row = row.row(align=True)
        row.prop(params, "skin_chain_use_scale", index=0, text="X", toggle=True)
        row.prop(params, "skin_chain_use_scale", index=1, text="Y", toggle=True)
        row.prop(params, "skin_chain_use_scale", index=2, text="Z", toggle=True)
        row.prop(params, "skin_chain_use_scale", index=3, text="Ease", toggle=True)

        row = col.split(factor=0.3)
        row.label(text="Connect Mirror:")
        row = row.row(align=True)
        row.prop(params, "skin_chain_connect_mirror", index=0, text="Start", toggle=True)
        row.prop(params, "skin_chain_connect_mirror", index=1, text="End", toggle=True)

        row = col.split(factor=0.3)
        row.label(text="Connect Next:")
        row = row.row(align=True)
        row.prop(params, "skin_chain_connect_ends", index=0, text="Start", toggle=True)
        row.prop(params, "skin_chain_connect_ends", index=1, text="End", toggle=True)

        row = col.split(factor=0.3)
        row.label(text="Sharpen:")
        row = row.row(align=True)
        row.prop(params, "skin_chain_connect_sharp_angle", index=0, text="Start")
        row.prop(params, "skin_chain_connect_sharp_angle", index=1, text="End")

        super().parameters_ui(layout, params)


def create_sample(obj):
    from rigify.rigs.basic.copy_chain import create_sample as inner
    obj.pose.bones[inner(obj)["bone.01"]].rigify_type = 'skin.basic_chain'
