# SPDX-FileCopyrightText: 2016-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
import math
import json

from typing import Optional
from mathutils import Vector, Matrix

from ...utils.rig import is_rig_base_bone
from ...utils.bones import align_chain_x_axis, align_bone_x_axis, align_bone_z_axis
from ...utils.bones import put_bone, align_bone_orientation
from ...utils.naming import make_derived_name
from ...utils.misc import matrix_from_axis_roll, matrix_from_axis_pair
from ...utils.widgets import adjust_widget_transform_mesh
from ...utils.animation import add_fk_ik_snap_buttons
from ...utils.mechanism import driver_var_transform

from ..widgets import create_foot_widget, create_ball_socket_widget

from ...base_rig import stage
from ...rig_ui_template import PanelLayout

from .limb_rigs import BaseLimbRig, SCRIPT_UTILITIES_OP_SNAP_IK_FK


DEG_360 = math.pi * 2
ALL_TRUE = (True, True, True)


class Rig(BaseLimbRig):
    """Human leg rig."""

    min_valid_orgs = max_valid_orgs = 4

    pivot_type: str
    heel_euler_order: str
    use_ik_toe: bool
    use_toe_roll: bool

    ik_matrix: Matrix
    roll_matrix: Matrix

    def find_org_bones(self, bone):
        bones = super().find_org_bones(bone)

        for b in self.get_bone(bones.main[2]).bone.children:
            if not b.use_connect and not b.children and not is_rig_base_bone(self.obj, b.name):
                bones.heel = b.name
                break
        else:
            self.raise_error("Heel bone not found.")

        return bones

    def initialize(self):
        super().initialize()

        self.pivot_type = self.params.foot_pivot_type
        self.heel_euler_order = 'ZXY' if self.main_axis == 'x' else 'XZY'
        self.use_ik_toe = self.params.extra_ik_toe
        self.use_toe_roll = self.params.extra_toe_roll

        if self.use_ik_toe:
            self.fk_name_suffix_cutoff = 3
            self.fk_ik_layer_cutoff = 4

        assert self.pivot_type in {'ANKLE', 'TOE', 'ANKLE_TOE'}

    def prepare_bones(self):
        orgs = self.bones.org.main
        foot = self.get_bone(orgs[2])

        ik_y_axis = (0, 1, 0)
        foot_y_axis = -self.vector_without_z(foot.y_axis)
        foot_x = foot_y_axis.cross((0, 0, 1))

        if self.params.rotation_axis == 'automatic':
            align_chain_x_axis(self.obj, orgs[0:2])

            # Orient foot and toe
            align_bone_x_axis(self.obj, orgs[2], foot_x)
            align_bone_x_axis(self.obj, orgs[3], -foot_x)

            align_bone_x_axis(self.obj, self.bones.org.heel, Vector((0, 0, 1)))

        elif self.params.auto_align_extremity:
            if self.main_axis == 'x':
                align_bone_x_axis(self.obj, orgs[2], foot_x)
                align_bone_x_axis(self.obj, orgs[3], -foot_x)
            else:
                align_bone_z_axis(self.obj, orgs[2], foot_x)
                align_bone_z_axis(self.obj, orgs[3], -foot_x)

        else:
            ik_y_axis = foot_y_axis

        # Orientation of the IK main and roll control bones
        self.ik_matrix = matrix_from_axis_roll(ik_y_axis, 0)
        self.roll_matrix = matrix_from_axis_pair(ik_y_axis, foot_x, self.main_axis)

    ####################################################
    # BONES

    class OrgBones(BaseLimbRig.OrgBones):
        heel: str                      # Heel location marker bone

    class CtrlBones(BaseLimbRig.CtrlBones):
        ik_spin: str                   # Toe spin control
        heel: str                      # Foot roll control
        ik_toe: str                    # If enabled, toe control for IK chain.

    class MchBones(BaseLimbRig.MchBones):
        heel: list[str]                # Chain of bones implementing foot roll.
        ik_toe_parent: str             # If using split IK toe, parent of the IK toe control.

    bones: BaseLimbRig.ToplevelBones[
        'Rig.OrgBones',
        'Rig.CtrlBones',
        'Rig.MchBones',
        list[str]
    ]

    ####################################################
    # UI

    def add_global_buttons(self, panel, rig_name):
        super().add_global_buttons(panel, rig_name)

        ik_chain, tail_chain, fk_chain = self.get_ik_fk_position_chains()

        add_leg_snap_ik_to_fk(
            panel,
            master=self.bones.ctrl.master,
            fk_bones=fk_chain, ik_bones=ik_chain, tail_bones=tail_chain,
            ik_ctrl_bones=self.get_ik_control_chain(),
            ik_extra_ctrls=self.get_extra_ik_controls(),
            heel_control=self.bones.ctrl.heel,
            rig_name=rig_name
        )

    def add_ik_only_buttons(self, panel, rig_name):
        super().add_ik_only_buttons(panel, rig_name)

        if self.use_toe_roll:
            bone = self.bones.ctrl.heel

            self.make_property(
                bone, 'Toe_Roll', default=0.0,
                description='Pivot on the tip of the toe when rolling forward with the heel control'
            )

            panel.custom_prop(bone, 'Toe_Roll', text='Roll On Toe', slider=True)

    ####################################################
    # IK controls

    def get_tail_ik_controls(self):
        return [self.bones.ctrl.ik_toe] if self.use_ik_toe else []

    def get_extra_ik_controls(self):
        controls = super().get_extra_ik_controls() + [self.bones.ctrl.heel]
        if self.pivot_type == 'ANKLE_TOE':
            controls += [self.bones.ctrl.ik_spin]
        return controls

    def make_ik_control_bone(self, orgs):
        name = self.copy_bone(orgs[2], make_derived_name(orgs[2], 'ctrl', '_ik'))
        if self.pivot_type == 'TOE':
            put_bone(self.obj, name, self.get_bone(name).tail, matrix=self.ik_matrix)
        else:
            put_bone(self.obj, name, None, matrix=self.ik_matrix)
        return name

    def build_ik_pivot(self, ik_name, **args):
        heel_bone = self.get_bone(self.bones.org.heel)
        args = {
            'position': (heel_bone.head + heel_bone.tail)/2,
            **args
        }
        return super().build_ik_pivot(ik_name, **args)

    def register_switch_parents(self, pbuilder):
        super().register_switch_parents(pbuilder)

        pbuilder.register_parent(self, self.bones.org.main[2], exclude_self=True, tags={'limb_end'})

    def make_ik_ctrl_widget(self, ctrl):
        obj = create_foot_widget(self.obj, ctrl)

        if self.pivot_type != 'TOE':
            ctrl = self.get_bone(ctrl)
            org = self.get_bone(self.bones.org.main[2])
            offset = org.tail - (ctrl.custom_shape_transform or ctrl).head
            adjust_widget_transform_mesh(obj, Matrix.Translation(offset))

    ####################################################
    # IK pivot controls

    def get_ik_pivot_output(self):
        if self.pivot_type == 'ANKLE_TOE':
            return self.bones.ctrl.ik_spin
        else:
            return self.get_ik_control_output()

    @stage.generate_bones
    def make_ik_pivot_controls(self):
        if self.pivot_type == 'ANKLE_TOE':
            self.bones.ctrl.ik_spin = self.make_ik_spin_bone(self.bones.org.main)

    def make_ik_spin_bone(self, orgs: list[str]):
        name = self.copy_bone(orgs[2], make_derived_name(orgs[2], 'ctrl', '_spin_ik'))
        put_bone(self.obj, name, self.get_bone(orgs[3]).head, matrix=self.ik_matrix, scale=0.5)
        return name

    @stage.parent_bones
    def parent_ik_pivot_controls(self):
        if self.pivot_type == 'ANKLE_TOE':
            self.set_bone_parent(self.bones.ctrl.ik_spin, self.get_ik_control_output())

    @stage.generate_widgets
    def make_ik_spin_control_widget(self):
        if self.pivot_type == 'ANKLE_TOE':
            obj = create_ball_socket_widget(self.obj, self.bones.ctrl.ik_spin, size=0.75)
            rot_fix = Matrix.Rotation(math.pi/2, 4, self.main_axis.upper())
            adjust_widget_transform_mesh(obj, rot_fix, local=True)

    ####################################################
    # Heel control

    @stage.generate_bones
    def make_heel_control_bone(self):
        org = self.bones.org.main[2]
        name = self.copy_bone(org, make_derived_name(org, 'ctrl', '_heel_ik'))
        put_bone(self.obj, name, None, matrix=self.roll_matrix, scale=0.5)
        self.bones.ctrl.heel = name

    @stage.parent_bones
    def parent_heel_control_bone(self):
        self.set_bone_parent(self.bones.ctrl.heel, self.get_ik_pivot_output(), inherit_scale='AVERAGE')

    @stage.configure_bones
    def configure_heel_control_bone(self):
        bone = self.get_bone(self.bones.ctrl.heel)
        bone.lock_location = True, True, True
        bone.rotation_mode = self.heel_euler_order
        bone.lock_scale = True, True, True

    @stage.generate_widgets
    def generate_heel_control_widget(self):
        create_ball_socket_widget(self.obj, self.bones.ctrl.heel)

    ####################################################
    # IK toe control

    @stage.generate_bones
    def make_ik_toe_control(self):
        if self.use_ik_toe:
            toe = self.bones.org.main[3]
            self.bones.ctrl.ik_toe = self.make_ik_toe_control_bone(toe)
            self.bones.mch.ik_toe_parent = self.make_ik_toe_parent_mch_bone(toe)

    def make_ik_toe_control_bone(self, org: str):
        return self.copy_bone(org, make_derived_name(org, 'ctrl', '_ik'))

    def make_ik_toe_parent_mch_bone(self, org: str):
        return self.copy_bone(org, make_derived_name(org, 'mch', '_ik_parent'), scale=1/3)

    @stage.parent_bones
    def parent_ik_toe_control(self):
        if self.use_ik_toe:
            mch = self.bones.mch
            align_bone_orientation(self.obj, mch.ik_toe_parent, self.get_mch_heel_toe_output())

            self.set_bone_parent(mch.ik_toe_parent, mch.ik_target, use_connect=True)
            self.set_bone_parent(self.bones.ctrl.ik_toe, mch.ik_toe_parent)

    @stage.configure_bones
    def configure_ik_toe_control(self):
        if self.use_ik_toe:
            self.copy_bone_properties(self.bones.org.main[3], self.bones.ctrl.ik_toe, props=False)

    @stage.rig_bones
    def rig_ik_toe_control(self):
        if self.use_ik_toe:
            self.make_constraint(self.bones.mch.ik_toe_parent, 'COPY_TRANSFORMS', self.get_mch_heel_toe_output())

    @stage.generate_widgets
    def make_ik_toe_control_widget(self):
        if self.use_ik_toe:
            self.make_fk_control_widget(3, self.bones.ctrl.ik_toe)

    ####################################################
    # Heel roll MCH

    def get_mch_heel_toe_output(self):
        return self.bones.mch.heel[-3]

    @stage.generate_bones
    def make_roll_mch_chain(self):
        orgs = self.bones.org.main
        self.bones.mch.heel = self.make_roll_mch_bones(orgs[2], orgs[3], self.bones.org.heel)

    def make_roll_mch_bones(self, foot: str, toe: str, heel: str):
        heel_bone = self.get_bone(heel)

        heel_middle = (heel_bone.head + heel_bone.tail) / 2

        result = self.copy_bone(foot, make_derived_name(foot, 'mch', '_roll'), scale=0.25)

        roll1 = self.copy_bone(toe, make_derived_name(heel, 'mch', '_roll1'), scale=0.3)
        roll2 = self.copy_bone(toe, make_derived_name(heel, 'mch', '_roll2'), scale=0.3)
        rock1 = self.copy_bone(heel, make_derived_name(heel, 'mch', '_rock1'))
        rock2 = self.copy_bone(heel, make_derived_name(heel, 'mch', '_rock2'))

        put_bone(self.obj, roll1, None, matrix=self.roll_matrix)
        put_bone(self.obj, roll2, heel_middle, matrix=self.roll_matrix)
        put_bone(self.obj, rock1, heel_bone.tail, matrix=self.roll_matrix, scale=0.5)
        put_bone(self.obj, rock2, heel_bone.head, matrix=self.roll_matrix, scale=0.5)

        if self.use_toe_roll:
            roll3 = self.copy_bone(toe, make_derived_name(heel, 'mch', '_roll3'), scale=0.3)

            toe_pos = Vector(self.get_bone(toe).tail)
            toe_pos.z = self.get_bone(roll2).head.z

            put_bone(self.obj, roll3, toe_pos, matrix=self.roll_matrix)

            return [rock2, rock1, roll2, roll3, roll1, result]

        else:
            return [rock2, rock1, roll2, roll1, result]

    @stage.parent_bones
    def parent_roll_mch_chain(self):
        chain = self.bones.mch.heel
        self.set_bone_parent(chain[0], self.get_ik_pivot_output())
        self.parent_bone_chain(chain)

    @stage.rig_bones
    def rig_roll_mch_chain(self):
        self.rig_roll_mch_bones(self.bones.mch.heel, self.bones.ctrl.heel, self.bones.org.heel)

    def rig_roll_mch_bones(self, chain: list[str], heel: str, org_heel: str):
        if self.use_toe_roll:
            rock2, rock1, roll2, roll3, roll1, result = chain

            # Interpolate rotation in Euler space via drivers to simplify Snap With Roll
            self.make_driver(
                roll3, 'rotation_euler', index=0,
                expression='max(0,x*i)' if self.main_axis == 'x' else 'x*i',
                variables={
                    'x': driver_var_transform(
                        self.obj, heel, type='ROT_X', space='LOCAL',
                        rotation_mode=self.heel_euler_order,
                    ),
                    'i': (heel, 'Toe_Roll'),
                }
            )

            self.make_driver(
                roll3, 'rotation_euler', index=2,
                expression='max(0,z*i)' if self.main_axis == 'z' else 'z*i',
                variables={
                    'z': driver_var_transform(
                        self.obj, heel, type='ROT_Z', space='LOCAL',
                        rotation_mode=self.heel_euler_order,
                    ),
                    'i': (heel, 'Toe_Roll'),
                }
            )

        else:
            rock2, rock1, roll2, roll1, result = chain

        # This order is required for correct working of the constraints
        for bone in chain:
            self.get_bone(bone).rotation_mode = self.heel_euler_order

        self.make_constraint(roll1, 'COPY_ROTATION', heel, space='POSE')

        if self.main_axis == 'x':
            self.make_constraint(roll2, 'COPY_ROTATION', heel, space='LOCAL', use_xyz=(True, False, False))
            self.make_constraint(roll2, 'LIMIT_ROTATION', min_x=-DEG_360, space='LOCAL')
        else:
            self.make_constraint(roll2, 'COPY_ROTATION', heel, space='LOCAL', use_xyz=(False, False, True))
            self.make_constraint(roll2, 'LIMIT_ROTATION', min_z=-DEG_360, space='LOCAL')

        direction = self.get_main_axis(self.get_bone(heel)).dot(self.get_bone(org_heel).vector)

        if direction < 0:
            rock2, rock1 = rock1, rock2

        self.make_constraint(
            rock1, 'COPY_ROTATION', heel, space='LOCAL',
            use_xyz=(False, True, False),
        )
        self.make_constraint(
            rock2, 'COPY_ROTATION', heel, space='LOCAL',
            use_xyz=(False, True, False),
        )

        self.make_constraint(rock1, 'LIMIT_ROTATION', max_y=DEG_360, space='LOCAL')
        self.make_constraint(rock2, 'LIMIT_ROTATION', min_y=-DEG_360, space='LOCAL')

    ####################################################
    # FK parents MCH chain

    def parent_fk_parent_bone(self, i, parent_mch, prev_ctrl, org, prev_org):
        if i == 3:
            if not self.use_ik_toe:
                align_bone_orientation(self.obj, parent_mch, self.get_mch_heel_toe_output())

                self.set_bone_parent(parent_mch, prev_org, use_connect=True)
            else:
                self.set_bone_parent(parent_mch, prev_ctrl, use_connect=True, inherit_scale='ALIGNED')

        else:
            super().parent_fk_parent_bone(i, parent_mch, prev_ctrl, org, prev_org)

    def rig_fk_parent_bone(self, i, parent_mch, org):
        if i == 3:
            if not self.use_ik_toe:
                con = self.make_constraint(parent_mch, 'COPY_TRANSFORMS', self.get_mch_heel_toe_output())

                self.make_driver(con, 'influence', variables=[(self.prop_bone, 'IK_FK')], polynomial=[1.0, -1.0])

        else:
            super().rig_fk_parent_bone(i, parent_mch, org)

    ####################################################
    # IK system MCH

    def get_ik_input_bone(self):
        return self.bones.mch.heel[-1]

    @stage.parent_bones
    def parent_ik_mch_chain(self):
        super().parent_ik_mch_chain()

        self.set_bone_parent(self.bones.mch.ik_target, self.bones.mch.heel[-1])

    ####################################################
    # Settings

    @classmethod
    def add_parameters(cls, params):
        super().add_parameters(params)

        items = [
            ('ANKLE', 'Ankle',
             'The foots pivots at the ankle'),
            ('TOE', 'Toe',
             'The foot pivots around the base of the toe'),
            ('ANKLE_TOE', 'Ankle and Toe',
             'The foots pivots at the ankle, with extra toe pivot'),
        ]

        params.foot_pivot_type = bpy.props.EnumProperty(
            items=items,
            name="Foot Pivot",
            default='ANKLE_TOE'
        )

        params.extra_ik_toe = bpy.props.BoolProperty(
            name='Separate IK Toe',
            default=False,
            description="Generate a separate IK toe control for better IK/FK snapping"
        )

        params.extra_toe_roll = bpy.props.BoolProperty(
            name='Toe Tip Roll',
            default=False,
            description="Generate a slider to pivot forward heel roll on the tip rather than the base of the toe"
        )

    @classmethod
    def parameters_ui(cls, layout, params, end='Foot'):
        layout.prop(params, 'foot_pivot_type')
        layout.prop(params, 'extra_ik_toe')
        layout.prop(params, 'extra_toe_roll')

        super().parameters_ui(layout, params, end)


##########################
# Leg IK to FK operator ##
##########################

SCRIPT_REGISTER_OP_LEG_SNAP_IK_FK = [
    'POSE_OT_rigify_leg_roll_ik2fk', 'POSE_OT_rigify_leg_roll_ik2fk_bake']

SCRIPT_UTILITIES_OP_LEG_SNAP_IK_FK = SCRIPT_UTILITIES_OP_SNAP_IK_FK + ['''
#######################
## Leg Snap IK to FK ##
#######################

class RigifyLegRollIk2FkBase(RigifyLimbIk2FkBase):
    heel_control: StringProperty(name="Heel")
    use_roll:     bpy.props.BoolVectorProperty(
        name="Use Roll", size=3, default=(True, True, False),
        description="Specifies which rotation axes of the heel roll control to use"
    )

    MODES = {
        'ZXY': ((0, 2), (1, 0, 2)),
        'XZY': ((2, 0), (2, 0, 1)),
    }

    def save_frame_state(self, context, obj):
        return get_chain_transform_matrices(obj, self.fk_bone_list + self.ctrl_bone_list[-1:])

    def assign_extra_controls(self, context, obj, all_matrices, ik_bones, ctrl_bones):
        for extra in self.extra_ctrl_list:
            set_transform_from_matrix(
                obj, extra, Matrix.Identity(4), space='LOCAL', keyflags=self.keyflags
            )

        if any(self.use_roll):
            foot_matrix = all_matrices[len(ik_bones) - 1]
            ctrl_matrix = all_matrices[len(self.fk_bone_list)]
            heel_bone = obj.pose.bones[self.heel_control]
            foot_bone = ctrl_bones[-1]

            # Relative rotation of heel from orientation of master IK control
            # to actual foot orientation.
            heel_rest = convert_pose_matrix_via_rest_delta(ctrl_matrix, foot_bone, heel_bone)
            heel_rot = convert_pose_matrix_via_rest_delta(foot_matrix, ik_bones[-1], heel_bone)

            # Decode the euler decomposition mode
            rot_mode = heel_bone.rotation_mode
            indices, use_map = self.MODES[rot_mode]
            use_roll = [self.use_roll[i] for i in use_map]
            roll, turn = indices

            # If the last rotation (yaw) is unused, move it to be first for better result
            if not use_roll[turn]:
                rot_mode = rot_mode[1:] + rot_mode[0:1]

            local_rot = (heel_rest.inverted() @ heel_rot).to_euler(rot_mode)

            heel_bone.rotation_euler = [
                (val if use else 0) for val, use in zip(local_rot, use_roll)
            ]

            if self.keyflags is not None:
                keyframe_transform_properties(
                    obj, bone_name, self.keyflags, no_loc=True, no_rot=no_rot, no_scale=True
                )

            if 'Toe_Roll' in heel_bone and self.tail_bone_list:
                toe_matrix = all_matrices[len(ik_bones)]
                toe_bone = obj.pose.bones[self.tail_bone_list[0]]

                # Compute relative rotation of heel determined by toe
                heel_rot_toe = convert_pose_matrix_via_rest_delta(toe_matrix, toe_bone, heel_bone)
                toe_rot = (heel_rest.inverted() @ heel_rot_toe).to_euler(rot_mode)

                # Determine how much of the already computed heel rotation seems to be applied
                heel_rot = list(heel_bone.rotation_euler)
                heel_rot[roll] = max(0.0, heel_rot[roll])

                # This relies on toe roll interpolation being done in Euler space
                ratios = [
                    toe_rot[i] / heel_rot[i] for i in (roll, turn)
                    if use_roll[i] and heel_rot[i] * toe_rot[i] > 0
                ]

                val = min(1.0, max(0.0, min(ratios) if ratios else 0.0))
                if val < 1e-5:
                    val = 0.0

                set_custom_property_value(
                    obj, heel_bone.name, 'Toe_Roll', val, keyflags=self.keyflags)

    def draw(self, context):
        row = self.layout.row(align=True)
        row.label(text="Use:")
        row.prop(self, 'use_roll', index=0, text="Rock", toggle=True)
        row.prop(self, 'use_roll', index=1, text="Roll", toggle=True)
        row.prop(self, 'use_roll', index=2, text="Yaw", toggle=True)

class POSE_OT_rigify_leg_roll_ik2fk(
        RigifyLegRollIk2FkBase, RigifySingleUpdateMixin, bpy.types.Operator):
    bl_options = {'REGISTER', 'UNDO', 'INTERNAL'}
    bl_idname = "pose.rigify_leg_roll_ik2fk_" + rig_id
    bl_label = "Snap IK->FK With Roll"
    bl_description = "Snap the IK chain to FK result, using foot roll to preserve the current IK "\
                     "control orientation as much as possible"

    def invoke(self, context, event):
        self.init_invoke(context)
        return self.execute(context)

class POSE_OT_rigify_leg_roll_ik2fk_bake(
        RigifyLegRollIk2FkBase, RigifyBakeKeyframesMixin, bpy.types.Operator):
    bl_idname = "pose.rigify_leg_roll_ik2fk_bake_" + rig_id
    bl_label = "Apply Snap IK->FK To Keyframes"
    bl_description = "Snap the IK chain keyframes to FK result, using foot roll to preserve the "\
                     "current IK control orientation as much as possible"

    def execute_scan_curves(self, context, obj):
        self.bake_add_bone_frames(self.fk_bone_list, TRANSFORM_PROPS_ALL)
        self.bake_add_bone_frames(self.ctrl_bone_list[-1:], TRANSFORM_PROPS_ROTATION)
        return self.bake_get_all_bone_curves(
            self.ctrl_bone_list + self.extra_ctrl_list, TRANSFORM_PROPS_ALL)
''']


def add_leg_snap_ik_to_fk(panel: PanelLayout, *, master: Optional[str] = None,
                          fk_bones=(), ik_bones=(), tail_bones=(),
                          ik_ctrl_bones=(), ik_extra_ctrls=(), heel_control, rig_name=''):
    panel.use_bake_settings()
    panel.script.add_utilities(SCRIPT_UTILITIES_OP_LEG_SNAP_IK_FK)
    panel.script.register_classes(SCRIPT_REGISTER_OP_LEG_SNAP_IK_FK)

    assert len(fk_bones) == len(ik_bones) + len(tail_bones)

    op_props = {
        'prop_bone': master,
        'fk_bones': json.dumps(fk_bones),
        'ik_bones': json.dumps(ik_bones),
        'ctrl_bones': json.dumps(ik_ctrl_bones),
        'tail_bones': json.dumps(tail_bones),
        'extra_ctrls': json.dumps(ik_extra_ctrls),
        'heel_control': heel_control,
    }

    add_fk_ik_snap_buttons(
        panel, 'pose.rigify_leg_roll_ik2fk_{rig_id}', 'pose.rigify_leg_roll_ik2fk_bake_{rig_id}',
        label='IK->FK With Roll', rig_name=rig_name, properties=op_props,
    )


def create_sample(obj):
    # generated by rigify.utils.write_metarig
    bpy.ops.object.mode_set(mode='EDIT')
    arm = obj.data

    bones = {}

    bone = arm.edit_bones.new('thigh.L')
    bone.head[:] = 0.0980, 0.0124, 1.0720
    bone.tail[:] = 0.0980, -0.0286, 0.5372
    bone.roll = 0.0000
    bone.use_connect = False
    bones['thigh.L'] = bone.name
    bone = arm.edit_bones.new('shin.L')
    bone.head[:] = 0.0980, -0.0286, 0.5372
    bone.tail[:] = 0.0980, 0.0162, 0.0852
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['thigh.L']]
    bones['shin.L'] = bone.name
    bone = arm.edit_bones.new('foot.L')
    bone.head[:] = 0.0980, 0.0162, 0.0852
    bone.tail[:] = 0.0980, -0.0934, 0.0167
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['shin.L']]
    bones['foot.L'] = bone.name
    bone = arm.edit_bones.new('toe.L')
    bone.head[:] = 0.0980, -0.0934, 0.0167
    bone.tail[:] = 0.0980, -0.1606, 0.0167
    bone.roll = -0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['foot.L']]
    bones['toe.L'] = bone.name
    bone = arm.edit_bones.new('heel.02.L')
    bone.head[:] = 0.0600, 0.0459, 0.0000
    bone.tail[:] = 0.1400, 0.0459, 0.0000
    bone.roll = 0.0000
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['foot.L']]
    bones['heel.02.L'] = bone.name

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones[bones['thigh.L']]
    pbone.rigify_type = 'limbs.leg'
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    try:
        pbone.rigify_parameters.separate_ik_layers = True
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.limb_type = "leg"
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.extra_ik_toe = True
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.ik_local_location = False
    except AttributeError:
        pass
    pbone = obj.pose.bones[bones['shin.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['foot.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['toe.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['heel.02.L']]
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
        arm.edit_bones.active = bone
        if bcoll := arm.collections.active:
            bcoll.assign(bone)

    return bones
