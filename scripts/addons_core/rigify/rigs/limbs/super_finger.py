# SPDX-FileCopyrightText: 2014-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
import json

from typing import Optional, Sequence
from itertools import count

from ...rig_ui_template import PanelLayout
from ...utils.bones import put_bone, flip_bone, align_chain_x_axis, set_bone_widget_transform
from ...utils.naming import make_derived_name
from ...utils.widgets import create_widget
from ...utils.widgets_basic import create_circle_widget, create_sphere_widget
from ...utils.misc import map_list
from ...utils.layers import ControlLayersOption
from ...utils.switch_parent import SwitchParentBuilder
from ...utils.animation import add_generic_snap, add_fk_ik_snap_buttons

from ...base_rig import stage

from ..chain_rigs import SimpleChainRig


class Rig(SimpleChainRig):
    """A finger rig with master control."""

    make_ik: bool

    def initialize(self):
        super().initialize()

        self.bbone_segments = self.params.bbones
        self.make_ik = self.params.make_extra_ik_control

    def prepare_bones(self):
        if self.params.primary_rotation_axis == 'automatic':
            align_chain_x_axis(self.obj, self.bones.org)

    def parent_bones(self):
        self.rig_parent_bone = self.get_bone_parent(self.bones.org[0])

    ##############################
    # BONES

    class CtrlBones(SimpleChainRig.CtrlBones):
        master: str                    # Master control
        ik: str                        # IK control (@make_ik)

    class MchBones(SimpleChainRig.MchBones):
        stretch: list[str]             # Stretch system
        bend: list[str]                # Bend system

    bones: SimpleChainRig.ToplevelBones[
        list[str],
        'Rig.CtrlBones',
        'Rig.MchBones',
        list[str]
    ]

    ##############################
    # Master Control

    @stage.generate_bones
    def make_master_control(self):
        orgs = self.bones.org
        name = self.copy_bone(orgs[0], make_derived_name(orgs[0], 'ctrl', '_master'), parent=True)
        self.bones.ctrl.master = name

        first_bone = self.get_bone(orgs[0])
        last_bone = self.get_bone(orgs[-1])
        self.get_bone(name).length += (last_bone.tail - first_bone.head).length * 1.25

    @stage.configure_bones
    def configure_master_control(self):
        master = self.bones.ctrl.master

        self.copy_bone_properties(self.bones.org[0], master, props=False, widget=False)

        bone = self.get_bone(master)
        bone.lock_scale = True, False, True

    @stage.generate_widgets
    def make_master_control_widget(self):
        master_name = self.bones.ctrl.master

        w = create_widget(self.obj, master_name)
        if w is not None:
            mesh = w.data
            verts = [(0, 0, 0), (0, 1, 0), (0.05, 1, 0), (0.05, 1.1, 0), (-0.05, 1.1, 0), (-0.05, 1, 0)]
            if 'Z' in self.params.primary_rotation_axis:
                # Flip x/z coordinates
                temp = []
                for v in verts:
                    temp += [(v[2], v[1], v[0])]
                verts = temp
            edges = [(0, 1), (1, 2), (2, 3), (3, 4), (4, 5), (5, 1)]
            mesh.from_pydata(verts, edges, [])
            mesh.update()

    ##############################
    # Control chain

    @stage.generate_bones
    def make_control_chain(self):
        orgs = self.bones.org
        self.bones.ctrl.fk = map_list(self.make_control_bone, count(0), orgs)
        self.bones.ctrl.fk += [self.make_tip_control_bone(orgs[-1], orgs[0])]

    def make_control_bone(self, i: int, org: str):
        return self.copy_bone(org, make_derived_name(org, 'ctrl'), inherit_scale=True)

    def make_tip_control_bone(self, org: str, name_org: str):
        name = self.copy_bone(org, make_derived_name(name_org, 'ctrl'), parent=False)

        flip_bone(self.obj, name)
        self.get_bone(name).length /= 2

        return name

    @stage.parent_bones
    def parent_control_chain(self):
        ctrls = self.bones.ctrl.fk
        for args in zip(ctrls, self.bones.mch.bend + ctrls[-2:]):
            self.set_bone_parent(*args)

    @stage.configure_bones
    def configure_control_chain(self):
        for args in zip(count(0), self.bones.ctrl.fk, [*self.bones.org, None]):
            self.configure_control_bone(*args)

        ControlLayersOption.TWEAK.assign(self.params, self.obj, self.bones.ctrl.fk)

    def configure_control_bone(self, i: int, ctrl: str, org: Optional[str]):
        if org:
            self.copy_bone_properties(org, ctrl)
        else:
            bone = self.get_bone(ctrl)
            bone.lock_rotation_w = True
            bone.lock_rotation = (True, True, True)
            bone.lock_scale = (True, True, True)

    def make_control_widget(self, i: int, ctrl: str):
        if ctrl == self.bones.ctrl.fk[-1]:
            # Tip control
            create_circle_widget(self.obj, ctrl, radius=0.3, head_tail=0.0)
        else:
            set_bone_widget_transform(self.obj, ctrl, self.bones.org[i])

            create_circle_widget(self.obj, ctrl, radius=0.3, head_tail=0.5)

    ##############################
    # IK Control

    @stage.generate_bones
    def make_ik_control(self):
        if self.make_ik:
            self.bones.ctrl.ik = self.make_ik_control_bone(self.bones.org)

            self.build_ik_parent_switch(SwitchParentBuilder(self.generator))

    def make_ik_control_bone(self, orgs: list[str]):
        name = self.copy_bone(orgs[-1], make_derived_name(orgs[0], 'ctrl', '_ik'), scale=0.7)
        put_bone(self.obj, name, self.get_bone(orgs[-1]).tail)
        return name

    def build_ik_parent_switch(self, pbuilder: SwitchParentBuilder):
        ctrl = self.bones.ctrl

        pbuilder.build_child(
            self, ctrl.ik, prop_bone=ctrl.ik,
            select_tags=['held_object', 'limb_ik', {'child', 'limb_end'}], only_selected=True,
            prop_id='IK_parent', prop_name='IK Parent', controls=[ctrl.ik],
            no_fix_rotation=True, no_fix_scale=True,
        )

    @stage.parent_bones
    def parent_ik_control(self):
        if self.make_ik:
            bone = self.get_bone(self.bones.ctrl.ik)
            bone.use_local_location = self.params.ik_local_location

    @stage.configure_bones
    def configure_ik_control(self):
        if self.make_ik:
            bone = self.get_bone(self.bones.ctrl.ik)
            bone.lock_rotation = True, True, True
            bone.lock_rotation_w = True
            bone.lock_scale = True, True, True

            ControlLayersOption.EXTRA_IK.assign_rig(self, [self.bones.ctrl.ik])

    @stage.configure_bones
    def configure_ik_control_properties(self):
        if self.make_ik:
            ctrl = self.bones.ctrl
            rig_name = ctrl.fk[0]

            panel = self.script.panel_with_selected_check(self, self.bones.ctrl.flatten())

            self.make_property(
                ctrl.ik, 'FK_IK', 0.0,
                description="Enable simple IK correction on top of FK posing"
            )
            panel.custom_prop(ctrl.ik, 'FK_IK', text="Finger IK ({})".format(rig_name), slider=True)

            axis = self.params.primary_rotation_axis

            add_finger_snap_fk_to_ik(
                panel, master=ctrl.master, fk_bones=ctrl.fk,
                ik_bones=self.bones.org, ik_control=ctrl.ik,
                ik_constraint_bone=self.bones.org[-1],
                axis=self.axis_options[axis]['id'],
                rig_name=rig_name, compact=True,
            )

            add_generic_snap(
                panel, output_bones=[ctrl.ik], input_bones=ctrl.fk[-1:],
                input_ctrl_bones=[ctrl.master, *ctrl.fk],
                label='IK->FK', rig_name=rig_name, tooltip='IK to FK',
                compact=True, locks=(False, True, True),
            )

    @stage.generate_widgets
    def make_ik_control_widget(self):
        if self.make_ik:
            create_sphere_widget(self.obj, self.bones.ctrl.ik)

    ##############################
    # MCH bend chain

    @stage.generate_bones
    def make_mch_bend_chain(self):
        self.bones.mch.bend = map_list(self.make_mch_bend_bone, self.bones.org)

    def make_mch_bend_bone(self, org: str):
        return self.copy_bone(org, make_derived_name(org, 'mch', '_drv'), inherit_scale=True, scale=0.3)

    @stage.parent_bones
    def parent_mch_bend_chain(self):
        ctrls = self.bones.ctrl.fk
        for args in zip(self.bones.mch.bend, [self.rig_parent_bone] + ctrls):
            self.set_bone_parent(*args)

    # Match axis to expression
    axis_options = {
        "automatic": {"axis": 0, "id": '+X',
                      "expr": '(1-sy)*pi'},
        "X": {"axis": 0, "id": '+X',
              "expr": '(1-sy)*pi'},
        "-X": {"axis": 0, "id": '-X',
               "expr": '-((1-sy)*pi)'},
        "Y": {"axis": 1, "id": '+Y',
              "expr": '(1-sy)*pi'},
        "-Y": {"axis": 1, "id": '-Y',
               "expr": '-((1-sy)*pi)'},
        "Z": {"axis": 2, "id": '+Z',
              "expr": '(1-sy)*pi'},
        "-Z": {"axis": 2, "id": '-Z',
               "expr": '-((1-sy)*pi)'}
    }

    @stage.rig_bones
    def rig_mch_bend_chain(self):
        for args in zip(count(0), self.bones.mch.bend):
            self.rig_mch_bend_bone(*args)

    def rig_mch_bend_bone(self, i: int, mch: str):
        master = self.bones.ctrl.master
        if i == 0:
            self.make_constraint(mch, 'COPY_LOCATION', master)
            self.make_constraint(mch, 'COPY_ROTATION', master, space='LOCAL')
        else:
            axis = self.params.primary_rotation_axis
            options = self.axis_options[axis]

            bone = self.get_bone(mch)
            bone.rotation_mode = 'YZX'

            self.make_driver(
                bone, 'rotation_euler', index=options['axis'],
                expression=options['expr'],
                variables={'sy': (master, '.scale.y')}
            )

    ##############################
    # MCH stretch chain

    @stage.generate_bones
    def make_mch_stretch_chain(self):
        self.bones.mch.stretch = map_list(self.make_mch_stretch_bone, self.bones.org)

    def make_mch_stretch_bone(self, org: str):
        return self.copy_bone(org, make_derived_name(org, 'mch'), parent=False)

    @stage.parent_bones
    def parent_mch_stretch_chain(self):
        ctrls = self.bones.ctrl.fk
        for args in zip(self.bones.mch.stretch, [self.rig_parent_bone] + ctrls[1:]):
            self.set_bone_parent(*args)

    @stage.rig_bones
    def rig_mch_stretch_chain(self):
        ctrls = self.bones.ctrl.fk
        for args in zip(count(0), self.bones.mch.stretch, ctrls, ctrls[1:]):
            self.rig_mch_stretch_bone(*args)

    def rig_mch_stretch_bone(self, i: int, mch: str, ctrl: str, ctrl_next: str):
        if i == 0:
            self.make_constraint(mch, 'COPY_LOCATION', ctrl)
            self.make_constraint(mch, 'COPY_SCALE', ctrl)

        self.make_constraint(mch, 'STRETCH_TO', ctrl_next, volume='NO_VOLUME', keep_axis='SWING_Y')

    ##############################
    # ORG chain

    @stage.rig_bones
    def rig_org_chain(self):
        for args in zip(count(0), self.bones.org, self.bones.mch.stretch):
            self.rig_org_bone(*args)

        if self.make_ik:
            self.rig_org_ik(self.bones.org, self.bones.ctrl.ik)

    def rig_org_ik(self, orgs: list[str], ik_ctrl: str):
        axis = self.params.primary_rotation_axis
        options = self.axis_options[axis]

        # Lock IK axis on child bones, using stiffness to preserve
        # original rotation rather than zeroing it out.
        stiffness = [1.0, 1.0, 1.0]
        stiffness[options['axis']] = 0.0

        for org in orgs[1:]:
            bone = self.get_bone(org)
            bone.ik_stiffness_x, bone.ik_stiffness_y, bone.ik_stiffness_z = stiffness

        # Add the constraint
        con = self.make_constraint(
            orgs[-1], 'IK', ik_ctrl, name='FingerIK',
            chain_count=len(orgs), use_stretch=False,
        )

        self.make_driver(con, "influence", variables=[(ik_ctrl, 'FK_IK')])

    ##############################
    # Deform chain

    @stage.configure_bones
    def configure_master_properties(self):
        master = self.bones.ctrl.master

        if self.bbone_segments > 1:
            self.make_property(master, 'finger_curve', 0.0, description="Rubber hose finger cartoon effect")

            # Create UI
            panel = self.script.panel_with_selected_check(self, self.bones.ctrl.flatten())
            panel.custom_prop(master, 'finger_curve', text="Curvature", slider=True)

    def rig_deform_bone(self, i: int, deform: str, org: str):
        master = self.bones.ctrl.master
        bone = self.get_bone(deform)

        self.make_constraint(deform, 'COPY_TRANSFORMS', org)

        if self.bbone_segments > 1:
            self.make_driver(bone.bone, 'bbone_easein', variables=[(master, 'finger_curve')])
            self.make_driver(bone.bone, 'bbone_easeout', variables=[(master, 'finger_curve')])

    ###############
    # OPTIONS

    @classmethod
    def add_parameters(cls, params):
        """ Add the parameters of this rig type to the
            RigifyParameters PropertyGroup
        """
        items = [('automatic', 'Automatic', ''),
                 ('X', 'X manual', ''), ('Y', 'Y manual', ''), ('Z', 'Z manual', ''),
                 ('-X', '-X manual', ''), ('-Y', '-Y manual', ''), ('-Z', '-Z manual', '')]

        params.primary_rotation_axis = bpy.props.EnumProperty(
            items=items, name="Primary Rotation Axis", default='automatic')

        params.bbones = bpy.props.IntProperty(
            name='B-Bone Segments',
            default=10,
            min=1,
            description='Number of B-Bone segments'
        )

        params.make_extra_ik_control = bpy.props.BoolProperty(
            name="Extra IK Control",
            default=False,
            description="Create an optional IK control"
        )

        params.ik_local_location = bpy.props.BoolProperty(
            name='IK Local Location',
            default=True,
            description="Specifies the value of the Local Location option for IK controls, "
                        "which decides if the location channels are aligned to the local control "
                        "orientation or world",
        )

        ControlLayersOption.TWEAK.add_parameters(params)
        ControlLayersOption.EXTRA_IK.add_parameters(params)

    @classmethod
    def parameters_ui(cls, layout, params):
        """ Create the ui for the rig parameters.
        """
        r = layout.row()
        r.label(text="Bend rotation axis:")
        r.prop(params, "primary_rotation_axis", text="")

        layout.prop(params, 'bbones')
        layout.prop(params, 'make_extra_ik_control', text='IK Control')

        if params.make_extra_ik_control:
            layout.prop(params, 'ik_local_location')

        ControlLayersOption.TWEAK.parameters_ui(layout, params)

        if params.make_extra_ik_control:
            ControlLayersOption.EXTRA_IK.parameters_ui(layout, params)


#############################
# Finger FK to IK operator ##
#############################

SCRIPT_REGISTER_OP_SNAP_FK_IK = ['POSE_OT_rigify_finger_fk2ik', 'POSE_OT_rigify_finger_fk2ik_bake']

SCRIPT_UTILITIES_OP_SNAP_FK_IK = ['''
########################
## Limb Snap IK to FK ##
########################

class RigifyFingerFk2IkBase:
    ik_control:      StringProperty(name="IK Control")
    ik_chain:        StringProperty(name="IK output chain")
    constraint_bone: StringProperty(name="Bone With the IK Constraint")
    fk_master:       StringProperty(name="FK Master Control")
    fk_chain:        StringProperty(name="FK Bone Chain")
    axis:            StringProperty(name="Main Rotation Axis", default="+X")

    def init_execute(self, context):
        self.ik_chain_list = json.loads(self.ik_chain)
        self.fk_chain_list = json.loads(self.fk_chain)

    # Extracting the IK state - requires forcing IK on temporarily
    def find_constraint_drivers(self, obj):
        self.driver_fcurves = {}
        self.ik_constraint = None

        if self.constraint_bone:
            bone = obj.pose.bones[self.constraint_bone]
            self.ik_constraint = con = bone.constraints['FingerIK']
            self.driver_fcurves = DriverCurveTable(obj).get_prop_curves(con, 'influence')

    def before_save_state(self, context, obj):
        self.find_constraint_drivers(obj)

        if self.ik_constraint:
            for fcu in self.driver_fcurves.values():
                fcu.mute = True

            self.ik_constraint.influence = 1

            context.view_layer.update()

    def get_fk_axis_angles(self, obj):
        options = self.axis_options[self.axis]
        angles = []

        for bone in self.fk_chain_list[1:-1]:
            matrix = obj.pose.bones[bone].matrix_basis
            eulers = matrix.to_euler(options['order'])
            angles.append(eulers[options['axis']])

        return angles

    def get_ik_original_matrix(self, obj):
        bone = obj.pose.bones[self.ik_chain_list[0]]

        if len(bone.constraints) == 1 and bone.constraints[0].type == 'COPY_TRANSFORMS':
            target = bone.constraints[0].subtarget
            return obj.pose.bones[target].matrix

    def save_frame_state(self, context, obj):
        matrices = get_chain_transform_matrices(obj, self.ik_chain_list)
        fk_matrices = get_chain_transform_matrices(obj, self.fk_chain_list)
        angles = self.get_fk_axis_angles(obj)
        ik_original = self.get_ik_original_matrix(obj)
        return (matrices, fk_matrices, angles, ik_original)

    def after_save_state(self, context, obj):
        if self.ik_constraint:
            for fcu in self.driver_fcurves.values():
                fcu.mute = False

            context.view_layer.update()

    # Applying the state
    axis_options = {
        "+X": {"axis": 0, "sign": 1, "order": 'ZYX'},
        "-X": {"axis": 0, "sign": -1, "order": 'ZYX'},
        "+Y": {"axis": 1, "sign": 1, "order": 'ZXY'},
        "-Y": {"axis": 1, "sign": -1, "order": 'ZXY'},
        "+Z": {"axis": 2, "sign": 1, "order": 'XYZ'},
        "-Z": {"axis": 2, "sign": -1, "order": 'XYZ'},
    }

    def apply_frame_state(self, context, obj, state):
        matrices, fk_matrices, old_angles, ik_original = state

        fk_master = obj.pose.bones[self.fk_master]
        fk_chain = [ obj.pose.bones[k] for k in self.fk_chain_list ]

        # Set the master control position and rotation.
        master_mat = matrices[0]

        if ik_original:
            master_mat = master_mat @ ik_original.inverted() @ fk_matrices[0]

        master_mat = obj.convert_space(
            pose_bone=fk_chain[0].parent, matrix=master_mat,
            from_space='POSE', to_space='LOCAL'
        )
        master_mat = obj.convert_space(
            pose_bone=fk_master, matrix=master_mat,
            from_space='LOCAL', to_space='POSE'
        )
        master_mat.translation = matrices[0].translation

        set_transform_from_matrix(obj, self.fk_master, master_mat)

        fk_master.scale = (1, 1, 1)

        if self.keyflags is not None:
            keyframe_transform_properties(obj, self.fk_master, self.keyflags)

        context.view_layer.update()

        # Apply the detail controls
        set_chain_transforms_from_matrices(
            context, obj, self.fk_chain_list[:-1], matrices, keyflags=self.keyflags,
        )

        set_transform_from_matrix(
            obj, self.fk_chain_list[-1], Matrix.Identity(4), space='LOCAL', keyflags=self.keyflags
        )

        # Compute the master scale from average control angle, biased by original
        options = self.axis_options[self.axis]

        angles = self.get_fk_axis_angles(obj)
        avg_angle = sum(a - b for a, b in zip(angles, old_angles)) / len(angles)

        fk_master.scale[1] = 1 - avg_angle * options['sign'] / pi

        if self.keyflags is not None:
            keyframe_transform_properties(obj, self.fk_master, self.keyflags)

        context.view_layer.update()

        # Re-apply the rest of the detail controls
        set_chain_transforms_from_matrices(
            context, obj, self.fk_chain_list[1:-1], matrices[1:], keyflags=self.keyflags,
        )

class POSE_OT_rigify_finger_fk2ik(RigifyFingerFk2IkBase, RigifySingleUpdateMixin, bpy.types.Operator):
    bl_idname = "pose.rigify_finger_fk2ik_" + rig_id
    bl_label = "Snap FK->IK"
    bl_description = "Snap the FK chain to IK result"

class POSE_OT_rigify_finger_fk2ik_bake(RigifyFingerFk2IkBase, RigifyBakeKeyframesMixin, bpy.types.Operator):
    bl_idname = "pose.rigify_finger_fk2ik_bake_" + rig_id
    bl_label = "Apply Snap FK->IK To Keyframes"
    bl_description = "Snap the FK chain keyframes to IK result"

    def execute_scan_curves(self, context, obj):
        fk_bones = [self.fk_master, *self.fk_chain_list]
        self.bake_add_bone_frames(fk_bones + [self.ik_control], TRANSFORM_PROPS_ALL)
        return self.bake_get_all_bone_curves(fk_bones, TRANSFORM_PROPS_ALL)
''']


def add_finger_snap_fk_to_ik(
        panel: 'PanelLayout', *, master: Optional[str] = None,
        fk_bones: Sequence[str] = (), ik_bones: Sequence[str] = (),
        ik_control: Optional[str] = None,
        ik_constraint_bone: Optional[str] = None,
        axis='+X', rig_name='', compact: Optional[bool] = None):
    panel.use_bake_settings()
    panel.script.add_utilities(SCRIPT_UTILITIES_OP_SNAP_FK_IK)
    panel.script.register_classes(SCRIPT_REGISTER_OP_SNAP_FK_IK)

    op_props = {
        'fk_master': master,
        'fk_chain': json.dumps(fk_bones),
        'ik_chain': json.dumps(ik_bones),
        'ik_control': ik_control,
        'constraint_bone': ik_constraint_bone,
        'axis': axis,
    }

    add_fk_ik_snap_buttons(
        panel, 'pose.rigify_finger_fk2ik_{rig_id}', 'pose.rigify_finger_fk2ik_bake_{rig_id}',
        label='FK->IK', rig_name=rig_name, properties=op_props,
        clear_bones=[master, *fk_bones], compact=compact,
    )


def create_sample(obj):
    # generated by rigify.utils.write_metarig
    bpy.ops.object.mode_set(mode='EDIT')
    arm = obj.data

    bones = {}

    bone = arm.edit_bones.new('palm.04.L')
    bone.head[:] = 0.0043, -0.0030, -0.0026
    bone.tail[:] = 0.0642, 0.0037, -0.0469
    bone.roll = -2.5155
    bone.use_connect = False
    bones['palm.04.L'] = bone.name
    bone = arm.edit_bones.new('f_pinky.01.L')
    bone.head[:] = 0.0642, 0.0037, -0.0469
    bone.tail[:] = 0.0703, 0.0039, -0.0741
    bone.roll = -1.9749
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['palm.04.L']]
    bones['f_pinky.01.L'] = bone.name
    bone = arm.edit_bones.new('f_pinky.02.L')
    bone.head[:] = 0.0703, 0.0039, -0.0741
    bone.tail[:] = 0.0732, 0.0044, -0.0965
    bone.roll = -1.9059
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['f_pinky.01.L']]
    bones['f_pinky.02.L'] = bone.name
    bone = arm.edit_bones.new('f_pinky.03.L')
    bone.head[:] = 0.0732, 0.0044, -0.0965
    bone.tail[:] = 0.0725, 0.0046, -0.1115
    bone.roll = -1.7639
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['f_pinky.02.L']]
    bones['f_pinky.03.L'] = bone.name

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones[bones['palm.04.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'YXZ'
    pbone = obj.pose.bones[bones['f_pinky.01.L']]
    pbone.rigify_type = 'limbs.super_finger'
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    try:
        pbone.rigify_parameters.ik_local_location = False
    except AttributeError:
        pass
    pbone = obj.pose.bones[bones['f_pinky.02.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['f_pinky.03.L']]
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
