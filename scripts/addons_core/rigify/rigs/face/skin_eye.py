# SPDX-FileCopyrightText: 2021-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
import math
import mathutils

from typing import Optional

from bpy.types import PoseBone
from mathutils import Vector, Matrix

from ...rig_ui_template import PanelLayout
from ...utils.layers import set_bone_layers, union_layer_lists
from ...utils.naming import make_derived_name, mirror_name, change_name_side, Side, SideZ
from ...utils.bones import align_bone_z_axis, put_bone, TypedBoneDict
from ...utils.widgets import (widget_generator, generate_circle_geometry,
                              generate_circle_hull_geometry)
from ...utils.widgets_basic import create_circle_widget
from ...utils.switch_parent import SwitchParentBuilder
from ...utils.misc import matrix_from_axis_pair, LazyRef

from ...base_rig import stage, RigComponent

from ..skin.skin_nodes import ControlBoneNode, BaseSkinNode
from ..skin.skin_parents import ControlBoneParentOffset, ControlBoneParentBase
from ..skin.skin_rigs import BaseSkinRig

from ..skin.basic_chain import Rig as BasicChainRig


class Rig(BaseSkinRig):
    """
    Eye rig that manages two child eyelid chains. The chains must
    connect at their ends using T/B symmetry.
    """

    def find_org_bones(self, bone: PoseBone) -> str:
        return bone.name

    cluster_control = None

    center: Vector
    axis: Vector

    eye_corner_nodes: list[ControlBoneNode]
    eye_corner_matrix: Optional[Matrix]
    eye_corner_range: tuple[float, float]

    child_chains: list[BasicChainRig]

    def initialize(self):
        super().initialize()

        bone = self.get_bone(self.base_bone)
        self.center = bone.head
        self.axis = bone.vector

        self.eye_corner_nodes = []
        self.eye_corner_matrix = None

        # Create the cluster control (it will assign self.cluster_control)
        if not self.cluster_control:
            self.create_cluster_control()

        self.init_child_chains()

    def create_cluster_control(self):
        return EyeClusterControl(self)

    ####################################################
    # UTILITIES

    def is_eye_control_node(self, node: ControlBoneNode) -> bool:
        return node.rig in self.child_chains and node.is_master_node

    # noinspection PyMethodMayBeStatic
    def is_eye_corner_node(self, node: ControlBoneNode) -> bool:
        # Corners are nodes where the two T and B chains merge
        sides = set(n.name_split.side_z for n in node.get_merged_siblings())
        return {SideZ.BOTTOM, SideZ.TOP}.issubset(sides)

    def init_eye_corner_space(self):
        """Initialize the coordinate space of the eye based on two corners."""
        if self.eye_corner_matrix:
            return

        if len(self.eye_corner_nodes) != 2:
            self.raise_error('Expected 2 eye corners, but found {}', len(self.eye_corner_nodes))

        # Build a coordinate space with XY plane based on eye axis and two corners
        corner_axis = self.eye_corner_nodes[1].point - self.eye_corner_nodes[0].point

        matrix = matrix_from_axis_pair(self.axis, corner_axis, 'x').to_4x4()
        matrix.translation = self.center
        self.eye_corner_matrix = matrix.inverted()

        # Compute signed angles from space_axis to the eye corners
        angle_min, angle_max = sorted(map(self.get_eye_corner_angle, self.eye_corner_nodes))

        self.eye_corner_range = (angle_min, angle_max)

        if not (angle_min <= 0 <= angle_max):
            self.raise_error('Bad relative angles of eye corners: {}..{}',
                             math.degrees(angle_min), math.degrees(angle_max))

    def get_eye_corner_angle(self, node: ControlBoneNode) -> float:
        """Compute a signed Z rotation angle from the eye axis to the node."""
        pt = self.eye_corner_matrix @ node.point
        return math.atan2(pt.x, pt.y)

    def get_master_control_position(self) -> Vector:
        """Compute suitable position for the master control."""
        self.init_eye_corner_space()

        # Place the control between the two corners on the eye axis
        corner_points = [node.point for node in self.eye_corner_nodes]

        point, _ = mathutils.geometry.intersect_line_line(
            self.center, self.center + self.axis, corner_points[0], corner_points[1]
        )
        return point

    def get_lid_follow_influence(self, node: ControlBoneNode):
        """Compute the influence factor of the eye movement on this eyelid control node."""
        self.init_eye_corner_space()

        # Interpolate from axis to corners based on Z angle
        angle = self.get_eye_corner_angle(node)
        angle_min, angle_max = self.eye_corner_range

        if angle_min < angle < 0:
            return 1 - min(1.0, angle/angle_min) ** 2
        elif 0 < angle < angle_max:
            return 1 - min(1.0, angle/angle_max) ** 2
        else:
            return 0

    ####################################################
    # BONES

    class CtrlBones(BaseSkinRig.CtrlBones):
        master: str                    # Parent control for moving the whole eye.
        target: str                    # Individual target this eye aims for.

    class MchBones(BaseSkinRig.MchBones):
        master: str                    # Bone that rotates to track ctrl.target.
        track: str                     # Bone that translates to follow mch.master tail.

    class DeformBones(TypedBoneDict):
        master: str                    # Deform mirror of ctrl.master.
        eye: str                       # Deform bone that rotates with mch.master.
        iris: str                      # Iris deform bone at master tail that scales with ctrl.target

    bones: BaseSkinRig.ToplevelBones[
        str,
        'Rig.CtrlBones',
        'Rig.MchBones',
        'Rig.DeformBones'
    ]

    ####################################################
    # CHILD CHAINS

    def init_child_chains(self):
        self.child_chains = [rig for rig in self.rigify_children if isinstance(rig, BasicChainRig)]

        # Inject a component twisting handles to the eye radius
        for child in self.child_chains:
            self.patch_chain(child)

    def patch_chain(self, child: BasicChainRig):
        return EyelidChainPatch(child, self)

    ####################################################
    # CONTROL NODES

    def extend_control_node_parent(self, parent, node: BaseSkinNode):
        if not isinstance(node, ControlBoneNode):
            return parent

        if self.is_eye_control_node(node):
            if self.is_eye_corner_node(node):
                # Remember corners for later computations
                assert not self.eye_corner_matrix
                self.eye_corner_nodes.append(node)
            else:
                # Non-corners get extra motion applied to them
                return self.extend_mid_node_parent(parent, node)

        return parent

    def extend_mid_node_parent(self, parent: ControlBoneParentBase, node: ControlBoneNode):
        parent = ControlBoneParentOffset(self, node, parent)

        # Add movement of the eye to the eyelid controls
        parent.add_copy_local_location(
            LazyRef(self.bones.mch, 'track'),
            influence=LazyRef(self.get_lid_follow_influence, node)
        )

        # If Limit Distance on the control can be disabled, add another one to the mch
        if self.params.eyelid_detach_option:
            parent.add_limit_distance(
                self.bones.org,
                distance=(node.point - self.center).length,
                limit_mode='LIMITDIST_ONSURFACE', use_transform_limit=True,
                # Use custom space to accommodate scaling
                space='CUSTOM', space_object=self.obj, space_subtarget=self.bones.org,
                # Don't allow reordering this limit and subsequent offsets
                ensure_order=True,
            )

        return parent

    def extend_control_node_rig(self, node: ControlBoneNode):
        if self.is_eye_control_node(node):
            # Add Limit Distance to enforce following the surface of the eye to the control
            con = self.make_constraint(
                node.control_bone, 'LIMIT_DISTANCE', self.bones.org,
                distance=(node.point - self.center).length,
                limit_mode='LIMITDIST_ONSURFACE', use_transform_limit=True,
                # Use custom space to accommodate scaling
                space='CUSTOM', space_object=self.obj, space_subtarget=self.bones.org,
            )

            if self.params.eyelid_detach_option:
                self.make_driver(con, 'influence',
                                 variables=[(self.bones.ctrl.target, 'lid_attach')])

    ####################################################
    # SCRIPT

    @stage.configure_bones
    def configure_script_panels(self):
        ctrl = self.bones.ctrl

        controls = sum((chain.get_all_controls() for chain in self.child_chains), ctrl.flatten())
        panel = self.script.panel_with_selected_check(self, controls)

        self.add_custom_properties()
        self.add_ui_sliders(panel)

    def add_custom_properties(self):
        target = self.bones.ctrl.target

        if self.params.eyelid_follow_split:
            self.make_property(
                target, 'lid_follow', list(self.params.eyelid_follow_default),
                description='Eyelids follow eye movement (X and Z)'
            )
        else:
            self.make_property(target, 'lid_follow', 1.0,
                               description='Eyelids follow eye movement')

        if self.params.eyelid_detach_option:
            self.make_property(target, 'lid_attach', 1.0,
                               description='Eyelids follow eye surface')

    def add_ui_sliders(self, panel: PanelLayout, *, add_name=False):
        target = self.bones.ctrl.target

        name_tail = f' ({target})' if add_name else ''
        follow_text = f'Eyelids Follow{name_tail}'

        if self.params.eyelid_follow_split:
            row = panel.split(factor=0.66, align=True)
            row.custom_prop(target, 'lid_follow', index=0, text=follow_text, slider=True)
            row.custom_prop(target, 'lid_follow', index=1, text='', slider=True)
        else:
            panel.custom_prop(target, 'lid_follow', text=follow_text, slider=True)

        if self.params.eyelid_detach_option:
            panel.custom_prop(
                target, 'lid_attach', text=f'Eyelids Attached{name_tail}', slider=True)

    ####################################################
    # Master control

    @stage.generate_bones
    def make_master_control(self):
        org = self.bones.org
        name = self.copy_bone(org, make_derived_name(org, 'ctrl', '_master'), parent=True)
        put_bone(self.obj, name, self.get_master_control_position())
        self.bones.ctrl.master = name

    @stage.configure_bones
    def configure_master_control(self):
        self.copy_bone_properties(self.bones.org, self.bones.ctrl.master)

    @stage.generate_widgets
    def make_master_control_widget(self):
        ctrl = self.bones.ctrl.master
        create_circle_widget(self.obj, ctrl, radius=1, head_tail=0.25)

    ####################################################
    # Tracking MCH

    @stage.generate_bones
    def make_mch_track_bones(self):
        org = self.bones.org
        mch = self.bones.mch

        mch.master = self.copy_bone(org, make_derived_name(org, 'mch'))
        mch.track = self.copy_bone(org, make_derived_name(org, 'mch', '_track'), scale=1/4)

        put_bone(self.obj, mch.track, self.get_bone(org).tail)

    @stage.parent_bones
    def parent_mch_track_bones(self):
        mch = self.bones.mch
        ctrl = self.bones.ctrl
        self.set_bone_parent(mch.master, ctrl.master)
        self.set_bone_parent(mch.track, ctrl.master)

    @stage.rig_bones
    def rig_mch_track_bones(self):
        mch = self.bones.mch
        ctrl = self.bones.ctrl

        # Rotationally track the target bone in mch.master
        self.make_constraint(mch.master, 'DAMPED_TRACK', ctrl.target)

        # Translate to track the tail of mch.master in mch.track. Its local
        # location is then copied to the control nodes.
        # Two constraints are used to provide different X and Z influence values.
        con_x = self.make_constraint(
            mch.track, 'COPY_LOCATION', mch.master, head_tail=1, name='lid_follow_x',
            use_xyz=(True, False, False),
            space='CUSTOM', space_object=self.obj, space_subtarget=self.bones.org,
        )

        con_z = self.make_constraint(
            mch.track, 'COPY_LOCATION', mch.master, head_tail=1, name='lid_follow_z',
            use_xyz=(False, False, True),
            space='CUSTOM', space_object=self.obj, space_subtarget=self.bones.org,
        )

        # Apply follow slider influence(s)
        if self.params.eyelid_follow_split:
            self.make_driver(con_x, 'influence', variables=[(ctrl.target, 'lid_follow', 0)])
            self.make_driver(con_z, 'influence', variables=[(ctrl.target, 'lid_follow', 1)])
        else:
            factor = self.params.eyelid_follow_default

            self.make_driver(
                con_x, 'influence', expression=f'var*{factor[0]}',
                variables=[(ctrl.target, 'lid_follow')]
            )
            self.make_driver(
                con_z, 'influence', expression=f'var*{factor[1]}',
                variables=[(ctrl.target, 'lid_follow')]
            )

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
        deform = self.bones.deform
        deform.master = self.copy_bone(org, make_derived_name(org, 'def', '_master'), scale=3/2)

        if self.params.make_deform:
            deform.eye = self.copy_bone(org, make_derived_name(org, 'def'))
            deform.iris = self.copy_bone(org, make_derived_name(org, 'def', '_iris'), scale=1/2)
            put_bone(self.obj, deform.iris, self.get_bone(org).tail)

    @stage.parent_bones
    def parent_deform_chain(self):
        deform = self.bones.deform
        self.set_bone_parent(deform.master, self.bones.org)

        if self.params.make_deform:
            self.set_bone_parent(deform.eye, self.bones.mch.master)
            self.set_bone_parent(deform.iris, deform.eye)

    @stage.rig_bones
    def rig_deform_chain(self):
        if self.params.make_deform:
            # Copy XZ local scale from the eye target control
            self.make_constraint(
                self.bones.deform.iris, 'COPY_SCALE', self.bones.ctrl.target,
                owner_space='LOCAL', target_space='LOCAL_OWNER_ORIENT', use_y=False,
            )

    ####################################################
    # SETTINGS

    @classmethod
    def add_parameters(cls, params):
        params.make_deform = bpy.props.BoolProperty(
            name="Deform",
            default=True,
            description="Create a deform bone for the copy"
        )

        params.eyelid_detach_option = bpy.props.BoolProperty(
            name="Eyelid Detach Option",
            default=False,
            description="Create an option to detach eyelids from the eye surface"
        )

        params.eyelid_follow_split = bpy.props.BoolProperty(
            name="Split Eyelid Follow Slider",
            default=False,
            description="Create separate eyelid follow influence sliders for X and Z"
        )

        params.eyelid_follow_default = bpy.props.FloatVectorProperty(
            size=2,
            name="Eyelids Follow Default",
            default=(0.2, 0.7), min=0, max=1,
            description="Default setting for the Eyelids Follow sliders (X and Z)",
        )

    @classmethod
    def parameters_ui(cls, layout, params):
        col = layout.column()
        col.prop(params, "make_deform", text="Eyeball And Iris Deforms")
        col.prop(params, "eyelid_detach_option")

        col.prop(params, "eyelid_follow_split")

        row = col.row(align=True)
        row.prop(params, "eyelid_follow_default", index=0, text="Follow X", slider=True)
        row.prop(params, "eyelid_follow_default", index=1, text="Follow Z", slider=True)


class EyelidChainPatch(RigComponent):
    """Component injected into child chains to twist handles aiming Z axis at the eye center."""

    rigify_sub_object_run_late = True

    owner: BasicChainRig

    def __init__(self, owner: BasicChainRig, eye: Rig):
        super().__init__(owner)

        self.eye = eye
        self.owner.use_pre_handles = True

    def align_bone(self, name):
        """Align bone rest orientation to aim Z axis at the eye center."""
        align_bone_z_axis(self.obj, name, self.eye.center - self.get_bone(name).head)

    def prepare_bones(self):
        for org in self.owner.bones.org:
            self.align_bone(org)

    def generate_bones(self):
        if self.owner.use_bbones:
            mch = self.owner.bones.mch
            for pre in [*mch.handles_pre, *mch.handles]:
                self.align_bone(pre)

    def rig_bones(self):
        if self.owner.use_bbones:
            for pre, node in zip(self.owner.bones.mch.handles_pre, self.owner.control_nodes):
                self.make_constraint(pre, 'COPY_LOCATION', node.control_bone, name='locate_cur')
                self.make_constraint(
                    pre, 'LOCKED_TRACK', self.eye.bones.org, name='track_center',
                    track_axis='TRACK_Z', lock_axis='LOCK_Y',
                )


class EyeClusterControl(RigComponent):
    """Component generating a common control for an eye cluster."""

    owner: Rig

    rig_list: list[Rig]
    rig_count: int

    size: float
    matrix: Matrix           # Cluster plane matrix
    inv_matrix: Matrix       # World to cluster plane

    rig_points: dict[Rig, Vector]  # Eye projections in cluster plane space

    master_bone: str
    child_bones: list[str]

    def __init__(self, owner: Rig):
        super().__init__(owner)

        self.find_cluster_rigs()

    def find_cluster_rigs(self):
        """Find and register all other eyes that belong to this cluster."""
        owner = self.owner

        owner.cluster_control = self
        self.rig_list = [owner]

        # Collect all sibling eye rigs
        parent_rig = owner.rigify_parent
        if parent_rig:
            for rig in parent_rig.rigify_children:
                if isinstance(rig, Rig) and rig != owner:
                    rig.cluster_control = self
                    self.rig_list.append(rig)

        self.rig_count = len(self.rig_list)

    ####################################################
    # UTILITIES

    def find_cluster_position(self):
        """Compute the eye cluster control position and orientation."""

        # Average location and Y axis of all the eyes
        axis = Vector((0, 0, 0))
        center = Vector((0, 0, 0))
        length = 0

        for rig in self.rig_list:
            bone = self.get_bone(rig.base_bone)
            axis += bone.y_axis
            center += bone.head
            length += bone.length

        axis /= self.rig_count
        center /= self.rig_count
        length /= self.rig_count

        # Create the matrix from the average Y and world Z
        matrix = matrix_from_axis_pair((0, 0, 1), axis, 'z').to_4x4()
        matrix.translation = center + axis * length * 5

        self.size = length * 3 / 4
        self.matrix = matrix
        self.inv_matrix = matrix.inverted()

    def project_rig_control(self, rig: Rig):
        """Intersect the given eye Y axis with the cluster plane, returns (x,y,0)."""
        bone = self.get_bone(rig.base_bone)

        head = self.inv_matrix @ bone.head
        tail = self.inv_matrix @ bone.tail
        axis = tail - head

        return head + axis * (-head.z / axis.z)

    def get_common_rig_name(self):
        """Choose a name for the cluster control based on the members."""
        names = set(rig.base_bone for rig in self.rig_list)
        name = min(names)

        if mirror_name(name) in names:
            return change_name_side(name, side=Side.MIDDLE)

        return name

    def get_rig_control_matrix(self, rig: Rig):
        """Compute a matrix for an individual eye sub-control."""
        matrix = self.matrix.copy()
        matrix.translation = self.matrix @ self.rig_points[rig]
        return matrix

    def get_master_control_layers(self):
        """Combine layers of all eyes for the cluster control."""
        return union_layer_lists(list(self.get_bone(rig.base_bone).collections) for rig in self.rig_list)

    def get_all_rig_control_bones(self):
        """Make a list of all control bones of all clustered eyes."""
        return list(set(sum((rig.bones.ctrl.flatten() for rig in self.rig_list), [self.master_bone])))

    ####################################################
    # STAGES

    def initialize(self):
        self.find_cluster_position()
        self.rig_points = {rig: self.project_rig_control(rig) for rig in self.rig_list}

    def generate_bones(self):
        if self.rig_count > 1:
            self.master_bone = self.make_master_control()
            self.child_bones = []

            for rig in self.rig_list:
                rig.bones.ctrl.target = child = self.make_child_control(rig)
                self.child_bones.append(child)
        else:
            self.master_bone = self.make_child_control(self.rig_list[0])
            self.child_bones = [self.master_bone]
            self.owner.bones.ctrl.target = self.master_bone

        self.build_parent_switch()

    def make_master_control(self):
        name = self.new_bone(make_derived_name(self.get_common_rig_name(), 'ctrl', '_common'))
        bone = self.get_bone(name)
        bone.matrix = self.matrix
        bone.length = self.size
        set_bone_layers(bone, self.get_master_control_layers())
        return name

    def make_child_control(self, rig: Rig):
        name = rig.copy_bone(
            rig.base_bone, make_derived_name(rig.base_bone, 'ctrl'), length=self.size)
        self.get_bone(name).matrix = self.get_rig_control_matrix(rig)
        return name

    def build_parent_switch(self):
        pbuilder = SwitchParentBuilder(self.owner.generator)

        org_parent = self.owner.rig_parent_bone
        parents = [org_parent] if org_parent else []

        pbuilder.build_child(
            self.owner, self.master_bone,
            prop_name=f'Parent ({self.master_bone})',
            extra_parents=parents, select_parent=org_parent,
            controls=self.get_all_rig_control_bones
        )

    def parent_bones(self):
        if self.rig_count > 1:
            self.get_bone(self.master_bone).use_local_location = False

            for child in self.child_bones:
                self.set_bone_parent(child, self.master_bone)

    def configure_bones(self):
        for child in self.child_bones:
            bone = self.get_bone(child)
            bone.lock_rotation = (True, True, True)
            bone.lock_rotation_w = True

        # When the cluster master control is selected, show sliders for all eyes
        if self.rig_count > 1:
            panel = self.owner.script.panel_with_selected_check(self.owner, [self.master_bone])

            for rig in self.rig_list:
                rig.add_ui_sliders(panel, add_name=True)

    def generate_widgets(self):
        for child in self.child_bones:
            create_eye_widget(self.obj, child)

        if self.rig_count > 1:
            pt2d = [p.to_2d() / self.size for p in self.rig_points.values()]
            create_eye_cluster_widget(self.obj, self.master_bone, points=pt2d)


@widget_generator
def create_eye_widget(geom, *, size=1):
    generate_circle_geometry(geom, Vector((0, 0, 0)), size/2)


@widget_generator
def create_eye_cluster_widget(geom, *, size=1, points):
    hull_points = [points[i] for i in mathutils.geometry.convex_hull_2d(points)]

    generate_circle_hull_geometry(geom, hull_points, size*0.75, size*0.6)
    generate_circle_hull_geometry(geom, hull_points, size, size*0.85)


def create_sample(obj):
    # generated by rigify.utils.write_metarig
    bpy.ops.object.mode_set(mode='EDIT')
    arm = obj.data

    bones = {}

    bone = arm.edit_bones.new('eye.L')
    bone.head = 0.0000, 0.0000, 0.0000
    bone.tail = 0.0000, -0.0125, 0.0000
    bone.roll = 0.0000
    bone.use_connect = False
    bones['eye.L'] = bone.name
    bone = arm.edit_bones.new('lid1.T.L')
    bone.head = 0.0155, -0.0006, -0.0003
    bone.tail = 0.0114, -0.0099, 0.0029
    bone.roll = 2.9453
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['eye.L']]
    bones['lid1.T.L'] = bone.name
    bone = arm.edit_bones.new('lid1.B.L')
    bone.head = 0.0155, -0.0006, -0.0003
    bone.tail = 0.0112, -0.0095, -0.0039
    bone.roll = -0.0621
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['eye.L']]
    bones['lid1.B.L'] = bone.name
    bone = arm.edit_bones.new('lid2.T.L')
    bone.head = 0.0114, -0.0099, 0.0029
    bone.tail = 0.0034, -0.0149, 0.0040
    bone.roll = 2.1070
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lid1.T.L']]
    bones['lid2.T.L'] = bone.name
    bone = arm.edit_bones.new('lid2.B.L')
    bone.head = 0.0112, -0.0095, -0.0039
    bone.tail = 0.0029, -0.0140, -0.0057
    bone.roll = 0.8337
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lid1.B.L']]
    bones['lid2.B.L'] = bone.name
    bone = arm.edit_bones.new('lid3.T.L')
    bone.head = 0.0034, -0.0149, 0.0040
    bone.tail = -0.0046, -0.0157, 0.0026
    bone.roll = 1.7002
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lid2.T.L']]
    bones['lid3.T.L'] = bone.name
    bone = arm.edit_bones.new('lid3.B.L')
    bone.head = 0.0029, -0.0140, -0.0057
    bone.tail = -0.0041, -0.0145, -0.0057
    bone.roll = 1.0671
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lid2.B.L']]
    bones['lid3.B.L'] = bone.name
    bone = arm.edit_bones.new('lid4.T.L')
    bone.head = -0.0046, -0.0157, 0.0026
    bone.tail = -0.0123, -0.0140, -0.0049
    bone.roll = 1.0850
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lid3.T.L']]
    bones['lid4.T.L'] = bone.name
    bone = arm.edit_bones.new('lid4.B.L')
    bone.head = -0.0041, -0.0145, -0.0057
    bone.tail = -0.0123, -0.0140, -0.0049
    bone.roll = 1.1667
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['lid3.B.L']]
    bones['lid4.B.L'] = bone.name

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones[bones['eye.L']]
    pbone.rigify_type = 'face.skin_eye'
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lid1.T.L']]
    pbone.rigify_type = 'skin.stretchy_chain'
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    try:
        pbone.rigify_parameters.skin_chain_pivot_pos = 2
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.bbones = 5
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.skin_chain_connect_mirror = [False, False]
    except AttributeError:
        pass
    pbone = obj.pose.bones[bones['lid1.B.L']]
    pbone.rigify_type = 'skin.stretchy_chain'
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    try:
        pbone.rigify_parameters.skin_chain_pivot_pos = 2
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.bbones = 5
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.skin_chain_connect_mirror = [False, False]
    except AttributeError:
        pass
    pbone = obj.pose.bones[bones['lid2.T.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lid2.B.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lid3.T.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lid3.B.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lid4.T.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['lid4.B.L']]
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
