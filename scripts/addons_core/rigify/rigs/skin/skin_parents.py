# SPDX-FileCopyrightText: 2021-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

from itertools import count
from string import Template
from typing import TYPE_CHECKING, Optional, NamedTuple, Any, Sequence, Callable
from dataclasses import dataclass, field

from mathutils import Quaternion

from ...utils.naming import make_derived_name
from ...utils.misc import force_lazy, Lazy, OptionalLazy

from ...base_rig import LazyRigComponent

if TYPE_CHECKING:
    from .skin_nodes import ControlBoneNode, BaseSkinNode
    from .skin_rigs import BaseSkinRig


class ControlBoneParentBase:
    """
    Base class for components that generate parent mechanisms for skin controls.
    The generated parent bone is accessible through the output_bone field or property.
    """

    # This generator's output bone cannot be modified by generators layered on top.
    # Otherwise, they may optimize bone count by adding more constraints in place.
    # (This generally signals the bone is shared between multiple users.)
    is_parent_frozen = True

    # The output bone field or property
    output_bone: str

    def __eq__(self, other):
        raise NotImplementedError()

    def replace_nested(self,
                       callback: Callable[['ControlBoneParentBase'], 'ControlBoneParentBase']):
        """Replace all nested parent objects with the result of applying the callback."""
        pass

    def enable_component(self):
        pass


# noinspection PyAbstractClass
class ControlBoneParentImplBase(LazyRigComponent, ControlBoneParentBase):
    """Base class for a control node parent generator that actually generates bones."""

    # Run this component after the @stage methods of the owner node and its slave nodes
    rigify_sub_object_run_late = True

    is_parent_frozen = False

    def __init__(self, rig: 'BaseSkinRig', node: 'BaseSkinNode'):
        super().__init__(node)

        # Rig that provides this parent mechanism.
        self.rig = rig
        # Control node that the mechanism is provided for
        self.node = node

    @property
    def control_node(self) -> 'ControlBoneNode':
        return self.node.control_node


class ControlBoneParentOrg(ControlBoneParentBase):
    """Control node parent generator wrapping a single ORG bone."""

    def __init__(self, org: Lazy[str]):
        self._output_bone = org

    @property
    def output_bone(self) -> str:
        return force_lazy(self._output_bone)

    def __eq__(self, other):
        return isinstance(other, ControlBoneParentOrg) and self._output_bone == other._output_bone


class ControlBoneParentArmature(ControlBoneParentImplBase):
    """Control node parent generator using the Armature constraint to parent the bone."""

    targets: list[str | tuple | dict]

    def __init__(self, rig: 'BaseSkinRig', node: 'BaseSkinNode', *,
                 bones: Lazy[list[str | tuple | dict]],
                 orientation: OptionalLazy[Quaternion] = None,
                 copy_scale: OptionalLazy[str] = None,
                 copy_rotation: OptionalLazy[str] = None):
        super().__init__(rig, node)

        # List of Armature constraint target specs for make_constraint (lazy).
        self.bones = bones
        # Orientation quaternion for the bone (lazy)
        self.orientation = orientation
        # Bone to copy scale from (lazy)
        self.copy_scale = copy_scale
        # Bone to copy rotation from (lazy)
        self.copy_rotation = copy_rotation

        if copy_scale or copy_rotation:
            self.is_parent_frozen = True

    def __eq__(self, other):
        return (
            isinstance(other, ControlBoneParentArmature) and
            self.node.point == other.node.point and
            self.orientation == other.orientation and
            self.bones == other.bones and
            self.copy_scale == other.copy_scale and
            self.copy_rotation == other.copy_rotation
        )

    def generate_bones(self):
        self.output_bone = self.control_node.make_bone(
            make_derived_name(self.node.name, 'mch', '_arm'), 1/4, rig=self.rig)

        self.rig.generator.disable_auto_parent(self.output_bone)

        if self.orientation:
            matrix = force_lazy(self.orientation).to_matrix().to_4x4()
            matrix.translation = self.node.point
            self.get_bone(self.output_bone).matrix = matrix

    def parent_bones(self):
        self.targets = force_lazy(self.bones)

        assert len(self.targets) > 0

        # Single target can be simplified to parenting
        if len(self.targets) == 1:
            target = force_lazy(self.targets[0])
            if isinstance(target, tuple):
                target = target[0]

            self.set_bone_parent(
                self.output_bone, target,
                inherit_scale='NONE' if self.copy_scale else 'FIX_SHEAR'
            )

    def rig_bones(self):
        # Multiple targets use the Armature constraint
        if len(self.targets) > 1:
            self.make_constraint(
                self.output_bone, 'ARMATURE', targets=self.targets,
                use_deform_preserve_volume=True
            )

            self.make_constraint(self.output_bone, 'LIMIT_ROTATION')

        if self.copy_rotation:
            self.make_constraint(self.output_bone, 'COPY_ROTATION', self.copy_rotation)
        if self.copy_scale:
            self.make_constraint(self.output_bone, 'COPY_SCALE', self.copy_scale)


class ControlBoneParentMix(ControlBoneParentImplBase):
    """Combine multiple parent mechanisms using the Armature constraint."""

    parents: list[ControlBoneParentBase]
    parent_weights: list[float]

    def __init__(self, rig: 'BaseSkinRig', node: 'ControlBoneNode',
                 parents: list[tuple[ControlBoneParentBase, float] | ControlBoneParentBase], *,
                 suffix: Optional[str] = None):
        super().__init__(rig, node)

        self.parents = []
        self.parent_weights = []
        self.suffix = suffix

        self.add_parents(parents)

    def replace_nested(self, callback):
        self.parents = [callback(item) for item in self.parents]

    def add_parents(self, parents):
        for item in parents:
            if isinstance(item, tuple):
                parent, weight = item
            else:
                parent, weight = item, 1

            for i, cur in enumerate(self.parents):
                if parent == cur:
                    self.parent_weights[i] += weight
                    break
            else:
                self.parents.append(parent)
                self.parent_weights.append(weight)

    def enable_component(self):
        for parent in self.parents:
            parent.enable_component()

        super().enable_component()

    def __eq__(self, other):
        return (
            isinstance(other, ControlBoneParentMix) and
            self.parents == other.parents and
            self.parent_weights == other.parent_weights
        )

    def generate_bones(self):
        self.output_bone = self.control_node.make_bone(
            make_derived_name(self.node.name, 'mch', self.suffix or '_mix'), 1/2, rig=self.rig)

        self.rig.generator.disable_auto_parent(self.output_bone)

    def parent_bones(self):
        if len(self.parents) == 1:
            self.set_bone_parent(self.output_bone, self.parents[0].output_bone)

    def rig_bones(self):
        if len(self.parents) > 1:
            targets = [(p.output_bone, w) for p, w in zip(self.parents, self.parent_weights)]

            self.make_constraint(
                self.output_bone, 'ARMATURE', targets=targets,
                use_deform_preserve_volume=True
            )


# noinspection PyAbstractClass
class ControlBoneParentLayer(ControlBoneParentImplBase):
    """Base class for parent generators that build on top of another mechanism."""

    def __init__(self, rig: 'BaseSkinRig', node: 'BaseSkinNode', parent: ControlBoneParentBase):
        super().__init__(rig, node)
        self.parent = parent

    def replace_nested(self, callback):
        self.parent = callback(self.parent)

    def enable_component(self):
        self.parent.enable_component()
        super().enable_component()


# noinspection PyAbstractClass
class ControlBoneWeakParentLayer(ControlBoneParentLayer):
    """
    Base class for layered parent generator that is only used for the re-parent source.
    I.e. it doesn't affect the control for its owner rig, but only for other rigs
    that have controls merged into this one.
    """

    @staticmethod
    def strip(parent):
        while isinstance(parent, ControlBoneWeakParentLayer):
            parent = parent.parent

        return parent


class ControlBoneParentOffset(ControlBoneParentLayer):
    """
    Parent mechanism generator that offsets the control's location.

    Supports Copy Transforms (Local) constraints and location drivers.
    Multiple offsets can be accumulated in the same generator, which
    will automatically create as many bones as needed.
    """

    class DriverEntry(NamedTuple):
        expression: str
        variables: dict[str, Any]

    @dataclass
    class CopyLocalEntry:
        influence: float = 0
        drivers: list['ControlBoneParentOffset.DriverEntry'] = field(default_factory=list)
        lazy_entries: list[Lazy[float]] = field(default_factory=list)

    copy_local: dict[str, 'ControlBoneParentOffset.CopyLocalEntry']
    add_orientations: dict[Sequence[float], Quaternion]
    add_local: dict[Sequence[float], Sequence[list['ControlBoneParentOffset.DriverEntry']]]
    limit_distance: list[tuple[str, dict]]

    reuse_mch: bool
    mch_bones: list[str]

    @classmethod
    def wrap(cls, owner: 'BaseSkinRig', parent: ControlBoneParentBase, node: 'BaseSkinNode',
             *constructor_args):
        # noinspection PyArgumentList
        return cls(owner, node, parent, *constructor_args)

    def __init__(self, rig: 'BaseSkinRig', node: 'BaseSkinNode', parent: ControlBoneParentBase):
        super().__init__(rig, node, parent)
        self.copy_local = {}
        self.add_local = {}
        self.add_orientations = {}
        self.limit_distance = []

    def enable_component(self):
        # Automatically merge an unfrozen sequence of this generator instances
        while isinstance(self.parent, ControlBoneParentOffset) and not self.parent.is_parent_frozen:
            self.prepend_contents(self.parent)
            self.parent = self.parent.parent

        super().enable_component()

    def prepend_contents(self, other):
        """Merge all offsets stored in the other generator into the current one."""
        for key, val in other.copy_local.items():
            if key not in self.copy_local:
                self.copy_local[key] = val
            else:
                inf, expr, cbs = val
                inf0, expr0, cbs0 = self.copy_local[key]
                self.copy_local[key] = self.CopyLocalEntry(inf+inf0, expr+expr0, cbs+cbs0)

        for key, val in other.add_orientations.items():
            if key not in self.add_orientations:
                self.add_orientations[key] = val

        for key, val in other.add_local.items():
            if key not in self.add_local:
                self.add_local[key] = val
            else:
                ot0, ot1, ot2 = val
                my0, my1, my2 = self.add_local[key]
                self.add_local[key] = (ot0+my0, ot1+my1, ot2+my2)

        self.limit_distance = other.limit_distance + self.limit_distance

    def add_copy_local_location(self, target: Lazy[str], *, influence: Lazy[float] = 1,
                                influence_expr: Optional[str] = None,
                                influence_vars: Optional[dict[str, Any]] = None):
        """
        Add a Copy Location (Local, Owner Orientation) offset.
        The influence may be specified as a (lazy) constant, or a driver expression
        with variables (using the same $var syntax as add_location_driver).
        """
        if target not in self.copy_local:
            self.copy_local[target] = self.CopyLocalEntry()

        if influence_expr:
            self.copy_local[target].drivers.append(
                self.DriverEntry(influence_expr, influence_vars or {}))
        elif callable(influence):
            self.copy_local[target].lazy_entries.append(influence)
        else:
            self.copy_local[target].influence += influence

    def add_location_driver(self, orientation: Quaternion, index: int,
                            expression: str, variables: dict[str, Any]):
        """
        Add a driver offsetting along the specified axis in the given Quaternion orientation.
        The variables may have to be renamed due to conflicts between multiple add requests,
        so the expression should use the $var syntax of Template to reference them.
        """
        assert isinstance(variables, dict)

        key: tuple[float, ...] = tuple(round(x*10000) for x in orientation)

        if key not in self.add_local:
            self.add_orientations[key] = orientation
            self.add_local[key] = ([], [], [])

        self.add_local[key][index].append(self.DriverEntry(expression, variables))

    def add_limit_distance(self, target: str, *, ensure_order=False, **kwargs):
        """Add a limit distance constraint with the given make_constraint arguments."""
        self.limit_distance.append((target, kwargs))

        # Prevent merging from reordering this limit
        if ensure_order:
            self.is_parent_frozen = True

    def __eq__(self, other):
        return (
            isinstance(other, ControlBoneParentOffset) and
            self.parent == other.parent and
            self.copy_local == other.copy_local and
            self.add_local == other.add_local and
            self.limit_distance == other.limit_distance
        )

    @property
    def output_bone(self):
        return self.mch_bones[-1] if self.mch_bones else self.parent.output_bone

    def generate_bones(self):
        self.mch_bones = []
        self.reuse_mch = False

        if self.copy_local or self.add_local or self.limit_distance:
            mch_name = make_derived_name(self.node.name, 'mch', '_offset')

            if self.add_local:
                # Generate a bone for every distinct orientation used for the drivers
                for key in self.add_local:
                    self.mch_bones.append(self.control_node.make_bone(
                        mch_name, 1/4, rig=self.rig, orientation=self.add_orientations[key]))
            else:
                # Try piggybacking on the parent bone if allowed
                if not self.parent.is_parent_frozen:
                    bone = self.get_bone(self.parent.output_bone)
                    if (bone.head - self.node.point).length < 1e-5:
                        self.reuse_mch = True
                        self.mch_bones = [bone.name]
                        return

                self.mch_bones.append(self.control_node.make_bone(mch_name, 1/4, rig=self.rig))

    def parent_bones(self):
        if self.mch_bones:
            if not self.reuse_mch:
                self.rig.set_bone_parent(self.mch_bones[0], self.parent.output_bone)

            self.rig.parent_bone_chain(self.mch_bones, use_connect=False)

    def compile_driver(self, items: list['ControlBoneParentOffset.DriverEntry']):
        variables = {}
        expressions = []

        # Loop through all expressions and combine the variable maps.
        for expr, var_set in items:
            template = Template(expr)
            var_map = {}

            # Check that all variables are present
            try:
                template.substitute({k: '' for k in var_set})
            except Exception as e:
                self.rig.raise_error('Invalid driver expression: {}\nError: {}', expr, e)

            # Merge variables
            for name, desc in var_set.items():
                # Check if the variable is used.
                try:
                    template.substitute({k: '' for k in var_set if k != name})
                    continue
                except KeyError:
                    pass

                # Descriptors may not be hashable, so linear search
                for var_name, var_desc in variables.items():
                    if var_desc == desc:
                        var_map[name] = var_name
                        break
                else:
                    # Find a unique name for the new variable and add to map
                    new_name = name
                    if new_name in variables:
                        for i in count(1):
                            new_name = '%s_%d' % (name, i)
                            if new_name not in variables:
                                break

                    variables[new_name] = desc
                    var_map[name] = new_name

            # Substitute the new names into the expression
            expressions.append(template.substitute(var_map))

        # Add all expressions together
        if len(expressions) > 1:
            final_expr = '+'.join('('+expr+')' for expr in expressions)
        else:
            final_expr = expressions[0]

        return final_expr, variables

    def rig_bones(self):
        # Emit the Copy Location constraints
        if self.copy_local:
            mch = self.mch_bones[0]
            for target, entry in self.copy_local.items():
                influence = entry.influence
                influence += sum(map(force_lazy, entry.lazy_entries))

                con = self.make_constraint(
                    mch, 'COPY_LOCATION', target, use_offset=True,
                    target_space='LOCAL_OWNER_ORIENT', owner_space='LOCAL', influence=influence,
                )

                drivers = entry.drivers

                if drivers:
                    if influence > 0:
                        drivers.append(self.DriverEntry(str(influence), {}))

                    expr, variables = self.compile_driver(drivers)
                    self.make_driver(con, 'influence', expression=expr, variables=variables)

        # Add the direct offset drivers
        if self.add_local:
            for mch, (key, specs) in zip(self.mch_bones, self.add_local.items()):
                for index, vals in enumerate(specs):
                    if vals:
                        expr, variables = self.compile_driver(vals)
                        self.make_driver(mch, 'location', index=index,
                                         expression=expr, variables=variables)

        # Add the limit distance constraints
        for target, kwargs in self.limit_distance:
            self.make_constraint(self.mch_bones[-1], 'LIMIT_DISTANCE', target, **kwargs)
