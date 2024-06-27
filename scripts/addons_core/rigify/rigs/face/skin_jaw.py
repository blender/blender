# SPDX-FileCopyrightText: 2021-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

from typing import Optional

import bpy
import math

from itertools import repeat

from bpy.types import PoseBone
from mathutils import Vector, Matrix, Quaternion
from bl_math import clamp

from ...utils.bones import TypedBoneDict
from ...utils.naming import make_derived_name, Side, SideZ, get_name_side_z
from ...utils.misc import map_list, matrix_from_axis_pair, LazyRef, Lazy
from ...utils.widgets_basic import create_circle_widget

from ...base_rig import stage

from ..skin.skin_nodes import ControlBoneNode, BaseSkinNode
from ..skin.skin_parents import ControlBoneParentOrg, ControlBoneParentArmature
from ..skin.skin_rigs import BaseSkinRig

from ..skin.basic_chain import Rig as BasicChainRig

from ..widgets import create_jaw_widget


class Rig(BaseSkinRig):
    """
    Jaw rig that manages loops of four mouth chains each. The chains
    must connect together at their ends using L/R and T/B symmetry.
    """

    def find_org_bones(self, bone: PoseBone) -> str:
        return bone.name

    mouth_orientation: Quaternion

    chain_to_layer: Optional[dict[BasicChainRig, int]]
    num_layers: int

    child_chains: list[BasicChainRig]
    corners: dict[Side | SideZ, list[ControlBoneNode]]
    chains_by_side: dict[Side | SideZ, set[BasicChainRig]]

    mouth_center: Vector
    mouth_space: Matrix
    to_mouth_space: Matrix

    left_sign: int
    layer_width: list[float]

    def initialize(self):
        super().initialize()

        self.mouth_orientation = self.get_mouth_orientation()
        self.chain_to_layer = None

        self.init_child_chains()

    ####################################################
    # UTILITIES

    def get_mouth_orientation(self) -> Quaternion:
        jaw_axis = self.get_bone(self.base_bone).y_axis.copy()
        jaw_axis[2] = 0

        return matrix_from_axis_pair(jaw_axis, (0, 0, 1), 'z').to_quaternion()

    def is_corner_node(self, node: ControlBoneNode) -> Side | SideZ | None:
        # Corners are nodes where two T/B or L/R chains meet.
        siblings = [n for n in node.get_merged_siblings() if n.rig in self.child_chains]

        sides_x = set(n.name_split.side for n in siblings)
        sides_z = set(n.name_split.side_z for n in siblings)

        if {SideZ.BOTTOM, SideZ.TOP}.issubset(sides_z):
            if Side.LEFT in sides_x:
                return Side.LEFT
            else:
                return Side.RIGHT

        if {Side.LEFT, Side.RIGHT}.issubset(sides_x):
            if SideZ.TOP in sides_z:
                return SideZ.TOP
            else:
                return SideZ.BOTTOM

        return None

    ####################################################
    # BONES

    class CtrlBones(BaseSkinRig.CtrlBones):
        master: str                    # Main jaw open control.
        mouth: str                     # Main control for adjusting mouth position and scale.

    class MchBones(BaseSkinRig.MchBones):
        lock: str                      # Jaw master mirror for the locked mouth.

        top: list[str]                 # Jaw master mirrors for the loop top.
        bottom: list[str]              # Jaw master mirrors for the loop bottom.
        middle: list[str]              # Middle position between top[] and bottom[].

        mouth_parent: str              # Parent for ctrl.mouth, mouth_layers and *_in (= middle[0])
        mouth_layers: list[str]        # Apply fade out of ctrl.mouth motion for outer loops.

        # Combine mouth and jaw motions via Copy Custom to Local.
        top_out: list[str]
        bottom_out: list[str]
        middle_out: list[str]

    class DeformBones(TypedBoneDict):
        master: str                    # Deform mirror of ctrl.master.

    bones: BaseSkinRig.ToplevelBones[
        str,
        'Rig.CtrlBones',
        'Rig.MchBones',
        'Rig.DeformBones'
    ]

    ####################################################
    # CHILD CHAINS

    def init_child_chains(self):
        self.child_chains = [
            rig
            for rig in self.rigify_children
            if isinstance(rig, BasicChainRig) and get_name_side_z(rig.base_bone) != SideZ.MIDDLE
        ]

        self.corners = {Side.LEFT: [], Side.RIGHT: [], SideZ.TOP: [], SideZ.BOTTOM: []}

    def arrange_child_chains(self):
        """Sort child chains into their corresponding mouth loops."""
        if self.chain_to_layer is not None:
            return

        # Index child node corners
        for child in self.child_chains:
            for node in child.control_nodes:
                corner = self.is_corner_node(node)
                if corner:
                    if node.merged_master not in self.corners[corner]:
                        self.corners[corner].append(node.merged_master)

        self.num_layers = len(self.corners[SideZ.TOP])

        for k, v in self.corners.items():
            if len(v) == 0:
                self.raise_error("Could not find all mouth corners")
            if len(v) != self.num_layers:
                self.raise_error(
                    "Mouth corner counts differ: {} vs {}",
                    [n.name for n in v], [n.name for n in self.corners[SideZ.TOP]]
                )

        # Find inner top/bottom corners
        anchor = self.corners[SideZ.BOTTOM][0].point
        inner_top = min(self.corners[SideZ.TOP], key=lambda p: (p.point - anchor).length)

        anchor = inner_top.point
        inner_bottom = min(self.corners[SideZ.BOTTOM], key=lambda p: (p.point - anchor).length)

        # Compute the mouth space
        self.mouth_center = center = (inner_top.point + inner_bottom.point) / 2

        matrix = self.mouth_orientation.to_matrix().to_4x4()
        matrix.translation = center
        self.mouth_space = matrix
        self.to_mouth_space = matrix.inverted()

        # Build a mapping of child chain to layer (i.e. sort multiple mouth loops)
        self.chain_to_layer = {}
        self.chains_by_side = {}

        for k, v in list(self.corners.items()):
            ordered: list[ControlBoneNode] = sorted(v, key=lambda p: (p.point - center).length)

            self.corners[k] = ordered

            chain_set: set[BasicChainRig] = set()

            for i, node in enumerate(ordered):
                for sibling in node.get_merged_siblings():
                    if sibling.rig in self.child_chains:
                        assert isinstance(sibling.rig, BasicChainRig)

                        cur_layer = self.chain_to_layer.get(sibling.rig)

                        if cur_layer is not None and cur_layer != i:
                            self.raise_error(
                                "Conflicting mouth chain layer on {}: {} and {}",
                                sibling.rig.base_bone, i, cur_layer)

                        self.chain_to_layer[sibling.rig] = i
                        chain_set.add(sibling.rig)

            self.chains_by_side[k] = chain_set

        for child in self.child_chains:
            if child not in self.chain_to_layer:
                self.raise_error("Could not determine chain layer on {}", child.base_bone)

        if not self.chains_by_side[Side.LEFT].isdisjoint(self.chains_by_side[Side.RIGHT]):
            self.raise_error("Left/right conflict in mouth")
        if not self.chains_by_side[SideZ.TOP].isdisjoint(self.chains_by_side[SideZ.BOTTOM]):
            self.raise_error("Top/bottom conflict in mouth")

        # Find left/right direction
        pt = self.to_mouth_space @ self.corners[Side.LEFT][0].point

        self.left_sign = 1 if pt.x > 0 else -1

        for node in self.corners[Side.LEFT]:
            if (self.to_mouth_space @ node.point).x * self.left_sign <= 0:
                self.raise_error("Bad left corner location: {}", node.name)

        for node in self.corners[Side.RIGHT]:
            if (self.to_mouth_space @ node.point).x * self.left_sign >= 0:
                self.raise_error("Bad right corner location: {}", node.name)

        # Find layer loop widths
        self.layer_width = [
            (self.corners[Side.LEFT][i].point - self.corners[Side.RIGHT][i].point).length
            for i in range(self.num_layers)
        ]

    def position_mouth_bone(self, name: str, scale: float):
        self.arrange_child_chains()

        bone = self.get_bone(name)
        bone.matrix = self.mouth_space
        bone.length = self.layer_width[0] * scale

    ####################################################
    # CONTROL NODES

    def get_node_parent_bones(self, node: ControlBoneNode
                              ) -> list[tuple[Lazy[str], float] | Lazy[str]]:
        """Get parent bones and their armature weights for the given control node."""
        self.arrange_child_chains()

        assert isinstance(node.rig, BasicChainRig)

        # Choose correct layer bones
        layer = self.chain_to_layer[node.rig]

        top_mch = LazyRef(self.bones.mch, 'top_out', layer)
        bottom_mch = LazyRef(self.bones.mch, 'bottom_out', layer)
        middle_mch = LazyRef(self.bones.mch, 'middle_out', layer)

        # Corners have one input
        corner = self.is_corner_node(node)
        if corner:
            if corner == SideZ.TOP:
                return [top_mch]
            elif corner == SideZ.BOTTOM:
                return [bottom_mch]
            else:
                return [middle_mch]

        # Otherwise blend two
        if node.rig in self.chains_by_side[SideZ.TOP]:
            side_mch = top_mch
        else:
            side_mch = bottom_mch

        pt_x = (self.to_mouth_space @ node.point).x
        side = Side.LEFT if pt_x * self.left_sign >= 0 else Side.RIGHT

        corner_x = (self.to_mouth_space @ self.corners[side][layer].point).x
        factor = math.sqrt(1 - clamp(pt_x / corner_x) ** 2)

        return [(side_mch, factor), (middle_mch, 1-factor)]

    def get_parent_for_name(self, name: str, parent_bone: str) -> Lazy[str]:
        """Get single replacement parent for the given child bone."""
        if parent_bone == self.base_bone:
            side = get_name_side_z(name)
            if side == SideZ.TOP:
                return LazyRef(self.bones.mch, 'top', -1)
            if side == SideZ.BOTTOM:
                return LazyRef(self.bones.mch, 'bottom', -1)

        return parent_bone

    def get_child_chain_parent(self, rig: BaseSkinRig, parent_bone: str) -> str:
        return self.get_parent_for_name(rig.base_bone, parent_bone)

    def build_control_node_parent(self, node: BaseSkinNode, parent_bone: str):
        if node.rig in self.child_chains:
            assert isinstance(node, ControlBoneNode)

            return ControlBoneParentArmature(
                self, node,
                bones=self.get_node_parent_bones(node),
                orientation=self.mouth_orientation,
                copy_scale=LazyRef(self.bones.mch, 'mouth_parent'),
            )

        return ControlBoneParentOrg(self.get_parent_for_name(node.name, parent_bone))

    ####################################################
    # Master control

    @stage.generate_bones
    def make_master_control(self):
        org = self.bones.org
        name = self.copy_bone(org, make_derived_name(org, 'ctrl'), parent=True)
        self.bones.ctrl.master = name

    @stage.configure_bones
    def configure_master_control(self):
        self.copy_bone_properties(self.bones.org, self.bones.ctrl.master)

        self.get_bone(self.bones.ctrl.master).lock_scale = (True, True, True)

    @stage.generate_widgets
    def make_master_control_widget(self):
        ctrl = self.bones.ctrl.master
        create_jaw_widget(self.obj, ctrl)

    ####################################################
    # Mouth control

    @stage.generate_bones
    def make_mouth_control(self):
        org = self.bones.org
        name = self.copy_bone(org, make_derived_name(org, 'ctrl', '_mouth'))
        self.position_mouth_bone(name, 1)
        self.bones.ctrl.mouth = name

    @stage.parent_bones
    def parent_mouth_control(self):
        self.set_bone_parent(self.bones.ctrl.mouth, self.bones.mch.mouth_parent)

    @stage.configure_bones
    def configure_mouth_control(self):
        pass

    @stage.generate_widgets
    def make_mouth_control_widget(self):
        ctrl = self.bones.ctrl.mouth

        width = (self.corners[Side.LEFT][0].point - self.corners[Side.RIGHT][0].point).length
        height = (self.corners[SideZ.TOP][0].point - self.corners[SideZ.BOTTOM][0].point).length
        back = (self.corners[Side.LEFT][0].point + self.corners[Side.RIGHT][0].point) / 2
        front = (self.corners[SideZ.TOP][0].point + self.corners[SideZ.BOTTOM][0].point) / 2
        depth = (front - back).length

        create_circle_widget(
            self.obj, ctrl,
            radius=0.2 + 0.5 * (height / width), radius_x=0.7,
            head_tail=0.2, head_tail_x=0.2 - (depth / width)
        )

    ####################################################
    # Jaw Motion MCH

    @stage.generate_bones
    def make_mch_lock_bones(self):
        org = self.bones.org
        mch = self.bones.mch

        self.arrange_child_chains()

        mch.lock = self.copy_bone(
            org, make_derived_name(org, 'mch', '_lock'), scale=1/2, parent=True)

        mch.top = map_list(self.make_mch_top_bone, range(self.num_layers), repeat(org))
        mch.bottom = map_list(self.make_mch_bottom_bone, range(self.num_layers), repeat(org))
        mch.middle = map_list(self.make_mch_middle_bone, range(self.num_layers), repeat(org))

        mch.mouth_parent = mch.middle[0]

    def make_mch_top_bone(self, _i: int, org: str):
        return self.copy_bone(org, make_derived_name(org, 'mch', '_top'), scale=1/4, parent=True)

    def make_mch_bottom_bone(self, _i: int, org: str):
        return self.copy_bone(org, make_derived_name(org, 'mch', '_bottom'), scale=1/3, parent=True)

    def make_mch_middle_bone(self, _i: int, org: str):
        return self.copy_bone(org, make_derived_name(org, 'mch', '_middle'), scale=2/3, parent=True)

    @stage.parent_bones
    def parent_mch_lock_bones(self):
        mch = self.bones.mch
        ctrl = self.bones.ctrl

        for mid, top in zip(mch.middle, mch.top):
            self.set_bone_parent(mid, top)

        for bottom in mch.bottom[1:]:
            self.set_bone_parent(bottom, ctrl.master)

    @stage.configure_bones
    def configure_mch_lock_bones(self):
        ctrl = self.bones.ctrl

        panel = self.script.panel_with_selected_check(self, [ctrl.master, ctrl.mouth])

        self.make_property(ctrl.master, 'mouth_lock', 0.0, description='Mouth is locked closed')
        panel.custom_prop(ctrl.master, 'mouth_lock', text='Mouth Lock', slider=True)

    @stage.rig_bones
    def rig_mch_track_bones(self):
        mch = self.bones.mch
        ctrl = self.bones.ctrl

        # Lock position follows jaw master with configured influence
        self.make_constraint(
            mch.lock, 'COPY_TRANSFORMS', ctrl.master,
            influence=self.params.jaw_locked_influence,
        )

        # Innermost top bone follows lock position according to slider
        con = self.make_constraint(mch.top[0], 'COPY_TRANSFORMS', mch.lock)
        self.make_driver(con, 'influence', variables=[(ctrl.master, 'mouth_lock')])

        # Innermost bottom bone follows jaw master with configured influence, and then lock
        self.make_constraint(
            mch.bottom[0], 'COPY_TRANSFORMS', ctrl.master,
            influence=self.params.jaw_mouth_influence,
        )

        con = self.make_constraint(mch.bottom[0], 'COPY_TRANSFORMS', mch.lock)
        self.make_driver(con, 'influence', variables=[(ctrl.master, 'mouth_lock')])

        # Outer layer bones interpolate toward innermost based on influence decay
        fac = self.params.jaw_secondary_influence

        for i, name in enumerate(mch.top[1:]):
            self.make_constraint(name, 'COPY_TRANSFORMS', mch.top[0], influence=fac ** (1+i))

        for i, name in enumerate(mch.bottom[1:]):
            self.make_constraint(name, 'COPY_TRANSFORMS', mch.bottom[0], influence=fac ** (1+i))

        # Middle bones interpolate the middle between top and bottom
        for mid, bottom in zip(mch.middle, mch.bottom):
            self.make_constraint(mid, 'COPY_TRANSFORMS', bottom, influence=0.5)

    ####################################################
    # Mouth MCH

    @stage.generate_bones
    def make_mch_mouth_bones(self):
        mch = self.bones.mch

        mch.mouth_layers = map_list(self.make_mch_mouth_bone,
                                    range(1, self.num_layers), repeat('_mouth_layer'), repeat(0.6))

        mch.top_out = map_list(self.make_mch_mouth_inout_bone,
                               range(self.num_layers), repeat('_top_out'), repeat(0.4))
        mch.bottom_out = map_list(self.make_mch_mouth_inout_bone,
                                  range(self.num_layers), repeat('_bottom_out'), repeat(0.35))
        mch.middle_out = map_list(self.make_mch_mouth_inout_bone,
                                  range(self.num_layers), repeat('_middle_out'), repeat(0.3))

    def make_mch_mouth_bone(self, _i: int, suffix: str, size: float):
        name = self.copy_bone(self.bones.org, make_derived_name(self.bones.org, 'mch', suffix))
        self.position_mouth_bone(name, size)
        return name

    def make_mch_mouth_inout_bone(self, _i: int, suffix: str, size: float):
        return self.copy_bone(self.bones.org, make_derived_name(self.bones.org, 'mch', suffix), scale=size)

    @stage.parent_bones
    def parent_mch_mouth_bones(self):
        mch = self.bones.mch
        layers = [self.bones.ctrl.mouth, *mch.mouth_layers]

        for name in mch.mouth_layers:
            self.set_bone_parent(name, mch.mouth_parent)

        for name_list in [mch.top_out, mch.bottom_out, mch.middle_out]:
            for name, parent in zip(name_list, layers):
                self.set_bone_parent(name, parent)

    @stage.rig_bones
    def rig_mch_mouth_bones(self):
        mch = self.bones.mch
        ctrl = self.bones.ctrl.mouth

        # Mouth influence fade out
        for i, name in enumerate(mch.mouth_layers):
            self.rig_mch_mouth_layer_bone(i+1, name, ctrl)

        # Transfer and combine jaw motion with mouth
        all_jaw = mch.top + mch.bottom + mch.middle
        all_out = mch.top_out + mch.bottom_out + mch.middle_out

        for dest, src in zip(all_out, all_jaw):
            self.make_constraint(
                dest, 'COPY_TRANSFORMS', src,
                owner_space='LOCAL', target_space='CUSTOM',
                space_object=self.obj, space_subtarget=mch.mouth_parent,
            )

    def rig_mch_mouth_layer_bone(self, i: int, mch: str, ctrl: str):
        # Fade location and rotation based on influence decay
        inf = self.params.jaw_secondary_influence ** i

        self.make_constraint(mch, 'COPY_LOCATION', ctrl, influence=inf)
        self.make_constraint(mch, 'COPY_ROTATION', ctrl, influence=inf)

        # For scale, additionally take radius into account
        inf_scale = inf * self.layer_width[0] / self.layer_width[i]

        self.make_constraint(mch, 'COPY_SCALE', ctrl, influence=inf_scale)

    ####################################################
    # ORG bone

    @stage.parent_bones
    def parent_org_chain(self):
        self.set_bone_parent(self.bones.org, self.bones.ctrl.master, inherit_scale='FULL')

    ####################################################
    # Deform bones

    @stage.generate_bones
    def make_deform_bone(self):
        org = self.bones.org
        self.bones.deform.master = self.copy_bone(org, make_derived_name(org, 'def'))

    @stage.parent_bones
    def parent_deform_chain(self):
        deform = self.bones.deform
        self.set_bone_parent(deform.master, self.bones.org)

    ####################################################
    # SETTINGS

    @classmethod
    def add_parameters(cls, params):
        params.jaw_mouth_influence = bpy.props.FloatProperty(
            name="Bottom Lip Influence",
            default=0.5, min=0, max=1,
            description="Influence of the jaw on the bottom lip chains"
        )

        params.jaw_locked_influence = bpy.props.FloatProperty(
            name="Locked Influence",
            default=0.2, min=0, max=1,
            description="Influence of the jaw on the locked mouth"
        )

        params.jaw_secondary_influence = bpy.props.FloatProperty(
            name="Secondary Influence Falloff",
            default=0.5, min=0, max=1,
            description="Reduction factor for each level of secondary mouth loops"
        )

    @classmethod
    def parameters_ui(cls, layout, params):
        layout.prop(params, "jaw_mouth_influence", slider=True)
        layout.prop(params, "jaw_locked_influence", slider=True)
        layout.prop(params, "jaw_secondary_influence", slider=True)


def create_sample(obj):
    # generated by rigify.utils.write_metarig
    bpy.ops.object.mode_set(mode='EDIT')
    arm = obj.data

    bones = {}

    bone = arm.edit_bones.new('jaw')
    bone.head = 0.0000, 0.0000, 0.0000
    bone.tail = 0.0000, -0.0585, -0.0489
    bone.roll = 0.0000
    bone.use_connect = False
    bones['jaw'] = bone.name
    bone = arm.edit_bones.new('teeth.T')
    bone.head = 0.0000, -0.0589, 0.0080
    bone.tail = 0.0000, -0.0283, 0.0080
    bone.roll = 0.0000
    bone.use_connect = False
    bones['teeth.T'] = bone.name
    bone = arm.edit_bones.new('lip.T.L')
    bone.head = -0.0000, -0.0684, 0.0030
    bone.tail = 0.0105, -0.0655, 0.0033
    bone.roll = -0.0000
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['jaw']]
    bones['lip.T.L'] = bone.name
    bone = arm.edit_bones.new('lip.B.L')
    bone.head = -0.0000, -0.0655, -0.0078
    bone.tail = 0.0107, -0.0625, -0.0053
    bone.roll = -0.0551
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['jaw']]
    bones['lip.B.L'] = bone.name
    bone = arm.edit_bones.new('lip.T.R')
    bone.head = 0.0000, -0.0684, 0.0030
    bone.tail = -0.0105, -0.0655, 0.0033
    bone.roll = 0.0000
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['jaw']]
    bones['lip.T.R'] = bone.name
    bone = arm.edit_bones.new('lip.B.R')
    bone.head = 0.0000, -0.0655, -0.0078
    bone.tail = -0.0107, -0.0625, -0.0053
    bone.roll = 0.0551
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['jaw']]
    bones['lip.B.R'] = bone.name
    bone = arm.edit_bones.new('teeth.B')
    bone.head = 0.0000, -0.0543, -0.0136
    bone.tail = 0.0000, -0.0237, -0.0136
    bone.roll = 0.0000
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['jaw']]
    bones['teeth.B'] = bone.name
    bone = arm.edit_bones.new('lip1.T.L')
    bone.head = 0.0105, -0.0655, 0.0033
    bone.tail = 0.0193, -0.0586, 0.0007
    bone.roll = -0.0257
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lip.T.L']]
    bones['lip1.T.L'] = bone.name
    bone = arm.edit_bones.new('lip1.B.L')
    bone.head = 0.0107, -0.0625, -0.0053
    bone.tail = 0.0194, -0.0573, -0.0029
    bone.roll = 0.0716
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lip.B.L']]
    bones['lip1.B.L'] = bone.name
    bone = arm.edit_bones.new('lip1.T.R')
    bone.head = -0.0105, -0.0655, 0.0033
    bone.tail = -0.0193, -0.0586, 0.0007
    bone.roll = 0.0257
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lip.T.R']]
    bones['lip1.T.R'] = bone.name
    bone = arm.edit_bones.new('lip1.B.R')
    bone.head = -0.0107, -0.0625, -0.0053
    bone.tail = -0.0194, -0.0573, -0.0029
    bone.roll = -0.0716
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lip.B.R']]
    bones['lip1.B.R'] = bone.name
    bone = arm.edit_bones.new('lip2.T.L')
    bone.head = 0.0193, -0.0586, 0.0007
    bone.tail = 0.0236, -0.0539, -0.0014
    bone.roll = 0.0324
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lip1.T.L']]
    bones['lip2.T.L'] = bone.name
    bone = arm.edit_bones.new('lip2.B.L')
    bone.head = 0.0194, -0.0573, -0.0029
    bone.tail = 0.0236, -0.0539, -0.0014
    bone.roll = 0.0467
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lip1.B.L']]
    bones['lip2.B.L'] = bone.name
    bone = arm.edit_bones.new('lip2.T.R')
    bone.head = -0.0193, -0.0586, 0.0007
    bone.tail = -0.0236, -0.0539, -0.0014
    bone.roll = -0.0324
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lip1.T.R']]
    bones['lip2.T.R'] = bone.name
    bone = arm.edit_bones.new('lip2.B.R')
    bone.head = -0.0194, -0.0573, -0.0029
    bone.tail = -0.0236, -0.0539, -0.0014
    bone.roll = -0.0467
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lip1.B.R']]
    bones['lip2.B.R'] = bone.name

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones[bones['jaw']]
    pbone.rigify_type = 'face.skin_jaw'
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['teeth.T']]
    pbone.rigify_type = 'basic.super_copy'
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    try:
        pbone.rigify_parameters.make_deform = False
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.super_copy_widget_type = "teeth"
    except AttributeError:
        pass
    pbone = obj.pose.bones[bones['lip.T.L']]
    pbone.rigify_type = 'skin.stretchy_chain'
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    try:
        pbone.rigify_parameters.bbones = 3
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.skin_chain_falloff_spherical = [True, False, True]
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.skin_chain_falloff = [0.5, 1.0, -0.5]
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.skin_chain_connect_mirror = [True, False]
    except AttributeError:
        pass
    pbone = obj.pose.bones[bones['lip.B.L']]
    pbone.rigify_type = 'skin.stretchy_chain'
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    try:
        pbone.rigify_parameters.bbones = 3
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.skin_chain_falloff_spherical = [True, False, True]
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.skin_chain_falloff = [0.5, 1.0, -0.5]
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.skin_chain_connect_mirror = [True, False]
    except AttributeError:
        pass
    pbone = obj.pose.bones[bones['lip.T.R']]
    pbone.rigify_type = 'skin.stretchy_chain'
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    try:
        pbone.rigify_parameters.bbones = 3
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.skin_chain_falloff_spherical = [True, False, True]
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.skin_chain_falloff = [0.5, 1.0, -0.5]
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.skin_chain_connect_mirror = [True, False]
    except AttributeError:
        pass
    pbone = obj.pose.bones[bones['lip.B.R']]
    pbone.rigify_type = 'skin.stretchy_chain'
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    try:
        pbone.rigify_parameters.bbones = 3
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.skin_chain_falloff_spherical = [True, False, True]
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.skin_chain_falloff = [0.5, 1.0, -0.5]
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.skin_chain_connect_mirror = [True, False]
    except AttributeError:
        pass
    pbone = obj.pose.bones[bones['teeth.B']]
    pbone.rigify_type = 'basic.super_copy'
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    try:
        pbone.rigify_parameters.super_copy_widget_type = "teeth"
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.make_deform = False
    except AttributeError:
        pass
    pbone = obj.pose.bones[bones['lip1.T.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lip1.B.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lip1.T.R']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lip1.B.R']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lip2.T.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lip2.B.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lip2.T.R']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lip2.B.R']]
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
        bone.bbone_x = bone.bbone_z = bone.length * 0.05
        arm.edit_bones.active = bone
        if bcoll := arm.collections.active:
            bcoll.assign(bone)

    return bones
