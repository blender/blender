# SPDX-FileCopyrightText: 2019-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy

from bpy.types import Constraint, ArmatureConstraint, UILayout

from ...utils.naming import choose_derived_bone, is_control_bone
from ...utils.mechanism import copy_custom_properties_with_ui, move_all_constraints
from ...utils.widgets import layout_widget_dropdown, create_registered_widget

from ...base_rig import BaseRig, BaseRigMixin

from itertools import repeat


'''
Due to T80764, bone name handling for 'limbs.raw_copy' was hard-coded in generate.py

class Rig(SubstitutionRig):
    """ A raw copy rig, preserving the metarig bone as is, without the ORG prefix. """

    def substitute(self):
        # Strip the ORG prefix during the rig instantiation phase
        new_name = strip_org(self.base_bone)
        new_name = self.generator.rename_org_bone(self.base_bone, new_name)

        return [ self.instantiate_rig(InstanceRig, new_name) ]
'''


class RelinkConstraintsMixin(BaseRigMixin):
    """ Utilities for constraint relinking. """

    def relink_bone_constraints(self, bone_name: str):
        if self.params.relink_constraints:
            for con in self.get_bone(bone_name).constraints:
                self.relink_single_constraint(con)

    relink_unmarked_constraints = False

    def relink_single_constraint(self, con: Constraint):
        if self.params.relink_constraints:
            parts = con.name.split('@')

            if len(parts) > 1:
                self.relink_constraint(con, parts[1:])
            elif self.relink_unmarked_constraints:
                self.relink_constraint(con, [''])

    def relink_move_constraints(self, from_bone: str, to_bone: str, *, prefix=''):
        if self.params.relink_constraints:
            move_all_constraints(self.obj, from_bone, to_bone, prefix=prefix)

    def relink_bone_parent(self, bone_name: str):
        if self.params.relink_constraints:
            self.generator.disable_auto_parent(bone_name)

            parent_spec = self.params.parent_bone
            if parent_spec:
                old_parent = self.get_bone_parent(bone_name)
                new_parent = self.find_relink_target(parent_spec, old_parent or '') or None
                self.set_bone_parent(bone_name, new_parent)
                return new_parent

    def relink_constraint(self, con: Constraint, specs: list[str]):
        if isinstance(con, ArmatureConstraint):
            if len(specs) == 1:
                specs = repeat(specs[0])
            elif len(specs) != len(con.targets):
                self.raise_error("Constraint {} actually has {} targets",
                                 con.name, len(con.targets))

            for tgt, spec in zip(con.targets, specs):
                if tgt.target == self.obj:
                    tgt.subtarget = self.find_relink_target(spec, tgt.subtarget)

        elif hasattr(con, 'subtarget'):
            if len(specs) > 1:
                self.raise_error("Only the Armature constraint can have multiple '@' targets: {}",
                                 con.name)

            if getattr(con, 'target', None) == self.obj:
                con.subtarget = self.find_relink_target(specs[0], con.subtarget)

    def find_relink_target(self, spec: str, old_target: str):
        if spec == '':
            return old_target
        elif spec in {'CTRL', 'DEF', 'MCH'}:
            result = choose_derived_bone(self.generator, old_target, spec.lower())
            if not result:
                result = choose_derived_bone(self.generator, old_target,
                                             spec.lower(), by_owner=False)
            if not result:
                self.raise_error("Cannot find derived {} bone of bone '{}' for relinking",
                                 spec, old_target)
            return result
        else:
            if spec not in self.obj.pose.bones:
                self.raise_error("Cannot find bone '{}' for relinking", spec)
            return spec

    @classmethod
    def add_relink_constraints_params(cls, params):
        params.relink_constraints = bpy.props.BoolProperty(
            name="Relink Constraints",
            default=False,
            description="For constraints with names formed like 'base@bonename', use the part "
                        "after '@' as the new subtarget after all bones are created. Use '@CTRL', "
                        "'@DEF' or '@MCH' to simply replace the prefix"
        )

        params.parent_bone = bpy.props.StringProperty(
            name="Parent",
            default="",
            description="Replace the parent with a different bone after all bones are created. "
                        "Using simply CTRL, DEF or MCH will replace the prefix instead"
        )

    @classmethod
    def add_relink_constraints_ui(cls, layout: UILayout, params):
        r = layout.row()
        r.prop(params, "relink_constraints")

        if params.relink_constraints:
            r = layout.row()
            r.prop(params, "parent_bone")

            layout.label(text="Constraint names have special meanings.", icon='ERROR')


class Rig(BaseRig, RelinkConstraintsMixin):
    bones: BaseRig.ToplevelBones[str, BaseRig.CtrlBones, BaseRig.MchBones, str]

    relink: bool

    def find_org_bones(self, pose_bone) -> str:
        return pose_bone.name

    def initialize(self):
        self.relink = self.params.relink_constraints

    def parent_bones(self):
        self.relink_bone_parent(self.bones.org)

    def configure_bones(self):
        org = self.bones.org
        if is_control_bone(org):
            copy_custom_properties_with_ui(self, org, org, ui_controls=[org])

    def rig_bones(self):
        self.relink_bone_constraints(self.bones.org)

    def generate_widgets(self):
        org = self.bones.org
        widget = self.params.optional_widget_type
        if widget and is_control_bone(org):
            create_registered_widget(self.obj, org, widget)

    @classmethod
    def add_parameters(cls, params):
        cls.add_relink_constraints_params(params)

        params.optional_widget_type = bpy.props.StringProperty(
            name="Widget Type",
            default='',
            description="Choose the type of the widget to create"
        )

    @classmethod
    def parameters_ui(cls, layout, params):
        col = layout.column()
        col.label(text='This rig type does not add the ORG prefix.')
        col.label(text='Manually add ORG, MCH or DEF as needed.')

        cls.add_relink_constraints_ui(layout, params)

        pbone = bpy.context.active_pose_bone

        if pbone and is_control_bone(pbone.name):
            layout_widget_dropdown(layout, params, "optional_widget_type")


# add_parameters = InstanceRig.add_parameters
# parameters_ui = InstanceRig.parameters_ui


def create_sample(obj):
    """ Create a sample metarig for this rig type.
    """
    # generated by rigify.utils.write_metarig
    bpy.ops.object.mode_set(mode='EDIT')
    arm = obj.data

    bones = {}

    bone = arm.edit_bones.new('DEF-bone')
    bone.head[:] = 0.0000, 0.0000, 0.0000
    bone.tail[:] = 0.0000, 0.0000, 0.2000
    bone.roll = 0.0000
    bone.use_connect = False
    bones['DEF-bone'] = bone.name

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones[bones['DEF-bone']]
    pbone.rigify_type = 'basic.raw_copy'
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
