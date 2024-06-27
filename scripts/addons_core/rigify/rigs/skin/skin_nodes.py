# SPDX-FileCopyrightText: 2021-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import enum

from functools import partial
from typing import Optional

from mathutils import Vector, Quaternion, Matrix

from ...utils.layers import set_bone_layers
from ...utils.misc import ArmatureObject
from ...utils.naming import NameSides, make_derived_name, get_name_base_and_sides, change_name_side, Side, SideZ
from ...utils.bones import BoneUtilityMixin, set_bone_widget_transform
from ...utils.widgets_basic import create_cube_widget, create_sphere_widget
from ...utils.mechanism import MechanismUtilityMixin
from ...utils.rig import get_parent_rigs

from ...utils.node_merger import MainMergeNode, QueryMergeNode, BaseMergeNode

from .skin_parents import ControlBoneWeakParentLayer, ControlBoneParentMix, ControlBoneParentBase
from .skin_rigs import BaseSkinRig, BaseSkinChainRig


class ControlNodeLayer(enum.IntEnum):
    FREE = 0
    MIDDLE_PIVOT = 10
    TWEAK = 20


class ControlNodeIcon(enum.IntEnum):
    TWEAK = 0
    MIDDLE_PIVOT = 1
    FREE = 2
    CUSTOM = 3


class ControlNodeEnd(enum.IntEnum):
    START = -1
    MIDDLE = 0
    END = 1


# noinspection PyAbstractClass
class BaseSkinNode(BaseMergeNode, MechanismUtilityMixin, BoneUtilityMixin):
    """Base class for skin control and query nodes."""

    rig: BaseSkinRig
    obj: ArmatureObject
    name: str
    point: Vector

    merged_master: 'ControlBoneNode'
    control_node: 'ControlBoneNode'

    node_parent: ControlBoneParentBase
    node_parent_built = False

    def do_build_parent(self) -> ControlBoneParentBase:
        """Create and intern the parent mechanism generator."""
        assert self.rig.generator.stage == 'initialize'

        result = self.rig.build_own_control_node_parent(self)
        parents = self.rig.get_all_parent_skin_rigs()

        for rig in reversed(parents):
            result = rig.extend_control_node_parent(result, self)

        for rig in parents:
            result = rig.extend_control_node_parent_post(result, self)

        result = self.merged_master.intern_parent(self, result)
        result.is_parent_frozen = True
        return result

    def build_parent(self, use=True, reparent=False) -> ControlBoneParentBase:
        """
        Create and activate if needed the parent mechanism for this node.

        Args:
            use: Immediately mark the parent as in use, ensuring generation.
            reparent: Immediately request reparent bone generation.
        Returns:
            Newly created parent.
        """
        if not self.node_parent_built:
            self.node_parent = self.do_build_parent()
            self.node_parent_built = True

        if use:
            self.merged_master.register_use_parent(self.node_parent)

        if reparent:
            self.merged_master.request_reparent(self.node_parent)

        return self.node_parent

    @property
    def control_bone(self):
        """The generated control bone."""
        return self.merged_master.control_bone

    @property
    def reparent_bone(self):
        """The generated reparent bone for this node's parent mechanism."""
        return self.merged_master.get_reparent_bone(self.node_parent)


class ControlBoneNode(MainMergeNode, BaseSkinNode):
    """Node representing controls of skin chain rigs."""

    merge_domain = 'ControlNetNode'

    rig: BaseSkinChainRig
    merged_master: 'ControlBoneNode'

    size: float
    name_split: NameSides
    name_merged: Optional[str]
    name_merged_split: Optional[NameSides]
    rotation: Optional[Quaternion]

    # For use by the owner rig: index in chain
    index: Optional[int]
    # If this node is the end of a chain, points to the next one
    chain_end_neighbor: Optional['ControlBoneNode']

    mirror_siblings: dict[NameSides, 'ControlBoneNode']
    mirror_sides_x: set[Side]
    mirror_sides_z: set[SideZ]

    parent_subrig_cache: list[ControlBoneParentBase]
    parent_subrig_names: dict[int, str]

    reparent_requests: list[ControlBoneParentBase]
    used_parents: dict[int, ControlBoneParentBase] | None
    reparent_bones: dict[int, str]
    reparent_bones_fake: set[str]

    matrix: Matrix
    _control_bone: str

    has_weak_parent: bool
    node_parent_weak: ControlBoneParentBase
    use_weak_parent: bool
    weak_parent_bone: str

    def __init__(
        self, rig: BaseSkinChainRig, org: str, name: str, *,
        point: Optional[Vector] = None, size: Optional[float] = None,
        needs_parent=False, needs_reparent=False, allow_scale=False,
        chain_end=ControlNodeEnd.MIDDLE,
        layer=ControlNodeLayer.FREE,
        index: Optional[int] = None,
        icon=ControlNodeIcon.TWEAK,
    ):
        """
        Construct a node generating a visible control.

        Args:
            rig: Owning skin chain rig.
            org: ORG bone that is associated with the node.
            name: Name of the node.
            point: Location of the node; defaults to org head.
            size: Size of the control; defaults to org length.
            needs_parent: Create the parent mechanism even if not master.
            needs_reparent: If this node's own parent mechanism differs from master,
            generate a conversion bone.
            allow_scale: Unlock scale channels.
            chain_end: Logical location within the chain.
            layer: Control dependency layer within the chain.
            index: Index within the chain.
            icon: Widget to use for the control.
        """

        assert isinstance(rig, BaseSkinChainRig)

        super().__init__(rig, name, point or rig.get_bone(org).head)

        self.org = org

        self.name_split = get_name_base_and_sides(name)

        self.name_merged = None
        self.name_merged_split = None

        self.size = size or rig.get_bone(org).length
        self.layer = layer
        self.icon = icon
        self.rotation = None
        self.chain_end = chain_end

        self.node_needs_parent = needs_parent
        self.node_needs_reparent = needs_reparent

        self.hide_control = False
        self.allow_scale = allow_scale

        self.index = index
        self.chain_end_neighbor = None

    @property
    def control_node(self) -> 'ControlBoneNode':
        return self

    @property
    def control_bone(self):
        return self.merged_master._control_bone

    def get_merged_siblings(self) -> list['ControlBoneNode']:
        return super().get_merged_siblings()

    def can_merge_into(self, other: 'ControlBoneNode'):
        # Only merge up the layers (towards more mechanism)
        delta_prio = self.rig.chain_priority - other.rig.chain_priority
        return (
            delta_prio <= 0 and
            (self.layer <= other.layer or delta_prio < 0) and
            super().can_merge_into(other)
        )

    def get_merge_priority(self, other: 'ControlBoneNode'):
        # Prefer higher and closest layer
        if self.layer <= other.layer:
            return -abs(self.layer - other.layer)
        else:
            return -abs(self.layer - other.layer) - 100

    def is_better_cluster(self, other: 'ControlBoneNode'):
        """Check if the current bone is preferable as master when choosing of same sized groups."""

        # Prefer bones that have strictly more parents
        my_parents = list(reversed(get_parent_rigs(self.rig.rigify_parent)))
        other_parents = list(reversed(get_parent_rigs(other.rig.rigify_parent)))

        if len(my_parents) > len(other_parents) and my_parents[0:len(other_parents)] == other_parents:
            return True
        if len(other_parents) > len(my_parents) and other_parents[0:len(other_parents)] == my_parents:
            return False

        # Prefer side chains
        side_x_my, side_z_my = map(abs, self.name_split[1:])
        side_x_other, side_z_other = map(abs, other.name_split[1:])

        if ((side_x_my < side_x_other and side_z_my <= side_z_other) or
                (side_x_my <= side_x_other and side_z_my < side_z_other)):
            return False
        if ((side_x_my > side_x_other and side_z_my >= side_z_other) or
                (side_x_my >= side_x_other and side_z_my > side_z_other)):
            return True

        return False

    def merge_done(self):
        if self.is_master_node:
            self.parent_subrig_cache = []
            self.parent_subrig_names = {}
            self.reparent_requests = []
            self.used_parents = {}

        super().merge_done()

        self.find_mirror_siblings()

    def find_mirror_siblings(self):
        """Find merged nodes that have their names in mirror symmetry with this one."""

        self.mirror_siblings = {}
        self.mirror_sides_x = set()
        self.mirror_sides_z = set()

        for node in self.get_merged_siblings():
            if node.name_split.base == self.name_split.base:
                self.mirror_siblings[node.name_split] = node
                self.mirror_sides_x.add(node.name_split.side)
                self.mirror_sides_z.add(node.name_split.side_z)

        assert self.mirror_siblings[self.name_split] is self

        # Remove sides that merged with a mirror from the name
        side_x = Side.MIDDLE if len(self.mirror_sides_x) > 1 else self.name_split.side
        side_z = SideZ.MIDDLE if len(self.mirror_sides_z) > 1 else self.name_split.side_z

        self.name_merged = change_name_side(self.name, side=side_x, side_z=side_z)
        self.name_merged_split = NameSides(self.name_split.base, side_x, side_z)

    def get_best_mirror(self) -> Optional['ControlBoneNode']:
        """Find best mirror sibling for connecting via mirror."""

        base, side, side_z = self.name_split

        for flip in [(base, -side, -side_z), (base, -side, side_z), (base, side, -side_z)]:
            mirror = self.mirror_siblings.get(flip, None)
            if mirror and mirror is not self:
                return mirror

        return None

    def intern_parent(self, node: BaseSkinNode, parent: ControlBoneParentBase
                      ) -> ControlBoneParentBase:
        """
        De-duplicate the parent layer chain within this merge group.

        Args:
            node: Node that introduced this parent mechanism.
            parent: Parent mechanism to register.
        Returns:
            The input parent mechanism, or its already interned equivalent.
        """

        # Quick check for the same object
        if id(parent) in self.parent_subrig_names:
            return parent

        # Find if an identical parent is already in the cache
        cache = self.parent_subrig_cache

        for previous in cache:
            if previous == parent:
                previous.is_parent_frozen = True
                return previous

        # Add to cache and intern the layer parent if exists
        cache.append(parent)

        self.parent_subrig_names[id(parent)] = node.name

        # Recursively apply to any inner references
        parent.replace_nested(partial(self.intern_parent, node))

        return parent

    def register_use_parent(self, parent: ControlBoneParentBase):
        """Activate this parent mechanism generator."""
        self.used_parents[id(parent)] = parent

    def request_reparent(self, parent: ControlBoneParentBase):
        """Request a reparent bone to be generated for this parent mechanism."""
        requests = self.reparent_requests

        if parent not in requests:
            # If the actual reparent would be generated, weak parent will be needed.
            if self.has_weak_parent and not self.use_weak_parent:
                if parent is not self.node_parent:
                    self.use_weak_parent = True
                    self.register_use_parent(self.node_parent_weak)

            self.register_use_parent(parent)
            requests.append(parent)

    def get_reparent_bone(self, parent: ControlBoneParentBase) -> str:
        """Returns the generated reparent bone for this parent mechanism."""
        return self.reparent_bones[id(parent)]

    def get_rotation(self) -> Quaternion:
        """Returns the orientation quaternion provided for this node by parents."""
        if self.rotation is None:
            self.rotation = self.rig.get_final_control_node_rotation(self)

        return self.rotation

    def initialize(self):
        if self.is_master_node:
            sibling_list = self.get_merged_siblings()
            mirror_sibling_list = self.mirror_siblings.values()

            # Compute size
            best = max(sibling_list, key=lambda n: n.icon)
            best_mirror = best.mirror_siblings.values()

            self.size = sum(node.size for node in best_mirror) / len(best_mirror)

            # Compute orientation
            self.rotation = sum(
                (node.get_rotation() for node in mirror_sibling_list),
                Quaternion((0, 0, 0, 0))
            ).normalized()

            self.matrix = self.rotation.to_matrix().to_4x4()
            self.matrix.translation = self.point

            # Create parents and decide if mix would be needed
            weak_parent_list = [node.build_parent(use=False) for node in mirror_sibling_list]

            if all(parent == self.node_parent for parent in weak_parent_list):
                weak_parent_list = [self.node_parent]
                self.node_parent_weak = self.node_parent
            else:
                self.node_parent_weak = ControlBoneParentMix(self.rig, self, weak_parent_list)

            # Prepare parenting without weak layers
            parent_list = [ControlBoneWeakParentLayer.strip(p) for p in weak_parent_list]

            self.use_weak_parent = False
            self.has_weak_parent = any((p is not pw)
                                       for p, pw in zip(weak_parent_list, parent_list))

            if not self.has_weak_parent:
                self.node_parent = self.node_parent_weak
            elif len(parent_list) > 1:
                self.node_parent = ControlBoneParentMix(
                    self.rig, self, parent_list, suffix='_mix_base')
            else:
                self.node_parent = parent_list[0]

            # Mirror siblings share the mixed parent for reparent
            self.register_use_parent(self.node_parent)

            for node in mirror_sibling_list:
                node.node_parent = self.node_parent

        # All nodes
        if self.node_needs_parent or self.node_needs_reparent:
            self.build_parent(reparent=self.node_needs_reparent)

    def prepare_bones(self):
        # Activate parent components once all reparents are registered
        if self.is_master_node:
            for parent in self.used_parents.values():
                parent.enable_component()

            self.used_parents = None

    def make_bone(self, name: str, scale: float, *,
                  rig: Optional[BaseSkinRig] = None,
                  orientation: Optional[Quaternion] = None) -> str:
        """
        Creates a bone associated with this node, using the appropriate
        orientation, location and size.

        Args:
            name: Name for the new bone.
            scale: Scale factor for the bone relative to default size.
            rig: Optionally, the rig that should be registered as the owner the bone.
            orientation: Optional override for the orientation but not location.
        """
        name = (rig or self).copy_bone(self.org, name)

        if orientation is not None:
            matrix = orientation.to_matrix().to_4x4()
            matrix.translation = self.merged_master.point
        else:
            matrix = self.merged_master.matrix

        bone = self.get_bone(name)
        bone.matrix = matrix
        bone.length = self.merged_master.size * scale

        return name

    def find_master_name_node(self) -> 'ControlBoneNode':
        """Find which node to name the control bone from."""

        # Chain end nodes have sub-par names, so try to find another chain
        if self.chain_end == ControlNodeEnd.END:
            # Choose possible other nodes so that it doesn't lose mirror tags
            siblings = [
                node for node in self.get_merged_siblings()
                if self.mirror_sides_x.issubset(node.mirror_sides_x)
                and self.mirror_sides_z.issubset(node.mirror_sides_z)
            ]

            # Prefer chain start, then middle nodes
            candidates = [node for node in siblings if node.chain_end == ControlNodeEnd.START]

            if not candidates:
                candidates = [node for node in siblings if node.chain_end == ControlNodeEnd.MIDDLE]

            # Choose based on priority and name alphabetical order
            if candidates:
                return min(candidates, key=lambda c: (-c.rig.chain_priority, c.name_merged))

        return self

    def generate_bones(self):
        if self.is_master_node:
            # Make control bone
            self._control_bone = self.make_master_bone()

            # Make weak parent bone
            if self.use_weak_parent:
                self.weak_parent_bone = self.make_bone(
                    make_derived_name(self._control_bone, 'mch', '_weak_parent'), 1/2)

            # Make requested reparents
            self.reparent_bones = {id(self.node_parent): self._control_bone}
            self.reparent_bones_fake = set(self.reparent_bones.values())

            for parent in self.reparent_requests:
                if id(parent) not in self.reparent_bones:
                    parent_name = self.parent_subrig_names[id(parent)]
                    bone = self.make_bone(make_derived_name(parent_name, 'mch', '_reparent'), 1/3)
                    self.reparent_bones[id(parent)] = bone

    def make_master_bone(self) -> str:
        """Generate the master control bone for the node."""
        choice = self.find_master_name_node()
        name = choice.name_merged

        if self.hide_control:
            name = make_derived_name(name, 'mch')

        return choice.make_bone(name, 1)

    def parent_bones(self):
        if self.is_master_node:
            self.set_bone_parent(
                self._control_bone, self.node_parent.output_bone, inherit_scale='AVERAGE')

            if self.use_weak_parent:
                self.set_bone_parent(
                    self.weak_parent_bone, self.node_parent_weak.output_bone, inherit_scale='FULL')

            for parent in self.reparent_requests:
                bone = self.reparent_bones[id(parent)]
                if bone not in self.reparent_bones_fake:
                    self.set_bone_parent(bone, parent.output_bone, inherit_scale='AVERAGE')

    def configure_bones(self):
        if self.is_master_node:
            if not any(node.allow_scale for node in self.get_merged_siblings()):
                self.get_bone(self.control_bone).lock_scale = (True, True, True)

        layers = self.rig.get_control_node_layers(self)
        if layers:
            bone = self.get_bone(self.control_bone).bone
            set_bone_layers(bone, layers, combine=not self.is_master_node)

    def rig_bones(self):
        if self.is_master_node:
            # Invoke parent rig callbacks
            for rig in reversed(self.rig.get_all_parent_skin_rigs()):
                rig.extend_control_node_rig(self)

            # Rig reparent bones
            reparent_source = self.control_bone

            if self.use_weak_parent:
                reparent_source = self.weak_parent_bone

                self.make_constraint(reparent_source, 'COPY_TRANSFORMS',
                                     self.control_bone, space='LOCAL')

                set_bone_widget_transform(self.obj, self.control_bone, reparent_source)

            for parent in self.reparent_requests:
                bone = self.reparent_bones[id(parent)]
                if bone not in self.reparent_bones_fake:
                    self.make_constraint(bone, 'COPY_TRANSFORMS', reparent_source)

    def generate_widgets(self):
        if self.is_master_node:
            best = max(self.get_merged_siblings(), key=lambda n: n.icon)

            if best.icon == ControlNodeIcon.TWEAK:
                create_sphere_widget(self.obj, self.control_bone)
            elif best.icon in (ControlNodeIcon.MIDDLE_PIVOT, ControlNodeIcon.FREE):
                create_cube_widget(self.obj, self.control_bone)
            else:
                best.rig.make_control_node_widget(best)


class ControlQueryNode(QueryMergeNode, BaseSkinNode):
    """Node representing controls of skin chain rigs."""

    merge_domain = 'ControlNetNode'

    matched_nodes: list['ControlBoneNode']

    def __init__(self, rig: BaseSkinRig, org: str, *,
                 name: Optional[str] = None,
                 point: Optional[Vector] = None,
                 find_highest_layer=False):
        """
        Create a skin query node.

        Args:
            rig: Rig that owns this node.
            org: ORG bone associated with this node.
            name: Name for this node, defaults to org.
            point: Location of the node, defaults to org head.
            find_highest_layer: Choose the highest layer master instead of lowest.
        """
        assert isinstance(rig, BaseSkinRig)

        super().__init__(rig, name or org, point or rig.get_bone(org).head)

        self.org = org
        self.find_highest_layer = find_highest_layer

    def can_merge_into(self, other: ControlBoneNode):
        return True

    def get_merge_priority(self, other: ControlBoneNode) -> float:
        return int(other.layer if self.find_highest_layer else -other.layer)

    @property
    def merged_master(self) -> ControlBoneNode:
        return self.matched_nodes[0]

    @property
    def control_node(self) -> ControlBoneNode:
        return self.matched_nodes[0]
