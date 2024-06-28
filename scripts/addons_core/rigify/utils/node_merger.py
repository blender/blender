# SPDX-FileCopyrightText: 2021-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import collections

from typing import Any, Sequence, Optional
from mathutils import Vector
from mathutils.kdtree import KDTree

from .errors import MetarigError
from .misc import ArmatureObject
from ..base_rig import BaseRig, GenerateCallbackHost
from ..base_generate import GeneratorPlugin


class NodeMerger(GeneratorPlugin):
    """
    Utility that allows rigs to interact based on common points in space.

    Rigs can register node objects representing locations during the
    initialize stage, and at the end the plugin sorts them into buckets
    based on proximity. For each such bucket a group object is created
    and allowed to further process the nodes.

    Nodes chosen by the groups as being 'final' become sub-objects of
    the plugin and receive stage callbacks.

    The domain parameter allows potentially having multiple completely
    separate layers of nodes with different purpose.
    """

    epsilon = 1e-5

    nodes: list['BaseMergeNode']
    final_nodes: list['BaseMergeNode']
    groups: list['MergeGroup']

    def __init__(self, generator, domain: Any):
        """
        Construct a new merger instance.

        @param domain: An arbitrary identifier to allow multiple independent merging domains.
        """
        super().__init__(generator)

        assert domain is not None
        assert generator.stage == 'initialize'

        self.domain = domain
        self.nodes = []
        self.final_nodes = []
        self.groups = []
        self.frozen = False

    def register_node(self, node: 'BaseMergeNode'):
        """
        Add a new node to generation, before merging is frozen.
        """
        assert not self.frozen
        node.generator_plugin = self
        self.nodes.append(node)

    def initialize(self):
        self.frozen = True

        nodes = self.nodes
        tree = KDTree(len(nodes))

        for i, node in enumerate(nodes):
            tree.insert(node.point, i)

        tree.balance()
        processed = set()
        final_nodes = []
        groups = []

        for i in range(len(nodes)):
            if i in processed:
                continue

            # Find points to merge
            pending = [i]
            merge_set = set(pending)

            while pending:
                added = set()
                for j in pending:
                    point = nodes[j].point
                    eps = max(1.0, point.length) * self.epsilon
                    for co, idx, dist in tree.find_range(point, eps):
                        added.add(idx)
                pending = added.difference(merge_set)
                merge_set.update(added)

            assert merge_set.isdisjoint(processed)

            processed.update(merge_set)

            # Group the points
            merge_list = [nodes[i] for i in merge_set]
            merge_list.sort(key=lambda x: x.name)

            group_class = merge_list[0].group_class

            for item in merge_list[1:]:
                cls = item.group_class

                if issubclass(cls, group_class):
                    group_class = cls
                elif not issubclass(group_class, cls):
                    raise MetarigError(
                        'Group class conflict: {} and {} from {} of {}'.format(
                            group_class, cls, item.name, item.rig.base_bone,
                        )
                    )

            group = group_class(merge_list)
            group.build(final_nodes)

            groups.append(group)

        self.final_nodes = self.rigify_sub_objects = final_nodes
        self.groups = groups


class MergeGroup(object):
    """
    Standard node group, merges nodes based on certain rules and priorities.

    1. Nodes are classified into main and query nodes; query nodes are not merged.
    2. Nodes owned by the same rig cannot merge with each other.
    3. Node can only merge into target if node.can_merge_into(target) is true.
    4. Among multiple candidates in one rig, node.get_merge_priority(target) is used.
    5. The largest clusters of nodes that can merge are picked until none are left.

    The master nodes of the chosen clusters, plus query nodes, become 'final'.
    """

    main_nodes: list['MainMergeNode']    # All main nodes in the group.
    query_nodes: list['QueryMergeNode']  # All query nodes in the group.
    final_nodes: list['MainMergeNode']   # All main nodes not merged into any other node.

    def __init__(self, nodes: list['BaseMergeNode']):
        self.nodes = nodes

        for node in nodes:
            assert isinstance(node, (MainMergeNode, QueryMergeNode))
            node.group = self

        self.main_nodes = [n for n in nodes if isinstance(n, MainMergeNode)]
        self.query_nodes = [n for n in nodes if isinstance(n, QueryMergeNode)]

    def build(self, final_nodes):
        main_nodes = self.main_nodes

        # Sort nodes into rig buckets - can't merge within the same rig
        rig_table = collections.defaultdict(list)

        for node in main_nodes:
            rig_table[node.rig].append(node)

        # Build a 'can merge' table
        merge_table = {n: set() for n in main_nodes}

        for node in main_nodes:
            for rig, tgt_nodes in rig_table.items():
                if rig is not node.rig:
                    nodes = [n for n in tgt_nodes if node.can_merge_into(n)]
                    if nodes:
                        best_node = max(nodes, key=node.get_merge_priority)
                        merge_table[best_node].add(node)

        # Output groups starting with largest
        self.final_nodes = []

        pending = set(main_nodes)

        while pending:
            # Find the largest group
            nodes = [n for n in main_nodes if n in pending]
            max_len = max(len(merge_table[n]) for n in nodes)

            nodes = [n for n in nodes if len(merge_table[n]) == max_len]

            # If a tie, try to resolve using comparison
            if len(nodes) > 1:
                weighted_nodes = [
                    (n, sum(
                        1 if (n.is_better_cluster(n2)
                              and not n2.is_better_cluster(n)) else 0
                        for n2 in nodes
                    ))
                    for n in nodes
                ]
                max_weight = max(wn[1] for wn in weighted_nodes)
                nodes = [wn[0] for wn in weighted_nodes if wn[1] == max_weight]

            # Final tiebreaker is the name
            best = min(nodes, key=lambda n: n.name)
            child_set = merge_table[best]

            # Link children
            best.point = sum((c.point for c in child_set),
                             best.point) / (len(child_set) + 1)

            for child in [n for n in main_nodes if n in child_set]:
                child.point = best.point
                best.merge_from(child)
                child.merge_into(best)

            final_nodes.append(best)
            self.final_nodes.append(best)

            best.merge_done()

            # Remove merged nodes from the table
            pending.remove(best)
            pending -= child_set

            for children in merge_table.values():
                children &= pending

        for node in self.query_nodes:
            node.merge_done()

        final_nodes += self.query_nodes


class BaseMergeNode(GenerateCallbackHost):
    """Base class of merge-able nodes."""

    rig: BaseRig
    obj: ArmatureObject
    name: str
    point: Vector

    merge_domain: Any = None
    merger = NodeMerger
    group_class = MergeGroup

    generator_plugin: NodeMerger
    group: MergeGroup

    def __init__(self, rig: BaseRig, name: str, point: Vector | Sequence[float], *,
                 domain: Any = None):
        self.rig = rig
        self.obj = rig.obj
        self.name = name
        self.point = Vector(point)

        merger = self.merger(rig.generator, domain or self.merge_domain)
        merger.register_node(self)

    def register_new_bone(self, new_name: str, old_name: Optional[str] = None):
        self.generator_plugin.register_new_bone(new_name, old_name)

    def can_merge_into(self, other: 'MainMergeNode') -> bool:
        """Checks if this main or query node can merge into the specified master node."""
        raise NotImplementedError

    def get_merge_priority(self, other: 'MainMergeNode') -> float:
        """Rank potential candidates to merge into."""
        return 0

    def merge_done(self):
        """Called after all merging operations are complete."""
        pass


class MainMergeNode(BaseMergeNode):
    """
    Base class of standard merge-able nodes. Each node can either be
    a master of its cluster or a merged child node. Children become
    sub-objects of their master to receive callbacks in defined order.
    """

    merged_master: 'MainMergeNode'          # Master of this merge cluster; may be self.
    merged_into: Optional['MainMergeNode']  # Master of this cluster if not self.
    merged: list['MainMergeNode']           # List of nodes merged into this one.

    def __init__(self, rig, name, point, *, domain=None):
        super().__init__(rig, name, point, domain=domain)

        self.merged_into = None
        self.merged = []

    def get_merged_siblings(self):
        """Retrieve the list of all nodes merged together with this one,
         starting with the master node."""
        master = self.merged_master
        return [master, *master.merged]

    def is_better_cluster(self, other: 'MainMergeNode'):
        """Compare with the other node to choose between cluster masters."""
        return False

    # noinspection PyMethodMayBeStatic
    def can_merge_from(self, _other: 'MainMergeNode'):
        """Checks if the other node can be merged into this one."""
        return True

    def can_merge_into(self, other: 'MainMergeNode'):
        """Checks if this node can merge into the specified master."""
        return other.can_merge_from(self)

    def merge_into(self, other: 'MainMergeNode'):
        """Called when it's decided to merge this node into a different master node."""
        self.merged_into = other

    def merge_from(self, other: 'MainMergeNode'):
        """Called when it's decided to merge a different node into this master node."""
        self.merged.append(other)

    @property
    def is_master_node(self):
        """Returns if this node is a master of a merge cluster."""
        return not self.merged_into

    def merge_done(self):
        self.merged_master = self.merged_into or self
        self.rigify_sub_objects = list(self.merged)

        for child in self.merged:
            child.merge_done()


# noinspection PyAbstractClass
class QueryMergeNode(BaseMergeNode):
    """Base class for special nodes used only to query which nodes are at a certain location."""

    is_master_node = False
    require_match = True

    matched_nodes: list['MainMergeNode']  # Master nodes this query matched with.

    def merge_done(self):
        self.matched_nodes = [
            n for n in self.group.final_nodes if self.can_merge_into(n)
        ]
        self.matched_nodes.sort(key=self.get_merge_priority, reverse=True)

        if self.require_match and not self.matched_nodes:
            self.rig.raise_error(
                'Could not match control node for {}', self.name)
