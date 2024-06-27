# SPDX-FileCopyrightText: 2010-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy

from ...base_rig import BaseRig

from ...utils.naming import make_derived_name
from ...utils.bones import set_bone_widget_transform
from ...utils.widgets import layout_widget_dropdown, create_registered_widget
from ...utils.widgets_basic import create_pivot_widget
from ...utils.switch_parent import SwitchParentBuilder


class Rig(BaseRig):
    """ A rig providing a rotation pivot control that can be moved. """

    class CtrlBones(BaseRig.CtrlBones):
        master: str
        pivot: str

    class MchBones(BaseRig.MchBones):
        pass

    bones: BaseRig.ToplevelBones[
        str,
        'Rig.CtrlBones',
        'Rig.MchBones',
        str
    ]

    make_control: bool
    make_pivot: bool
    make_deform: bool

    def find_org_bones(self, pose_bone) -> str:
        return pose_bone.name

    def initialize(self):
        self.make_control = self.params.make_extra_control
        self.make_pivot = self.params.make_control or not self.make_control
        self.make_deform = self.params.make_extra_deform

    def generate_bones(self):
        org = self.bones.org

        if self.make_control:
            self.bones.ctrl.master = name = self.copy_bone(org, make_derived_name(org, 'ctrl'), parent=True)

            if self.make_pivot:
                self.bones.ctrl.pivot = self.copy_bone(org, make_derived_name(org, 'ctrl', '_pivot'))

            if self.params.make_parent_switch:
                self.build_parent_switch(name)

            if self.params.register_parent:
                self.register_parent(name, self.get_parent_tags())

        else:
            self.bones.ctrl.pivot = self.copy_bone(org, make_derived_name(org, 'ctrl'), parent=True)

        if self.make_deform:
            self.bones.deform = self.copy_bone(org, make_derived_name(org, 'def'), bbone=True)

    def build_parent_switch(self, master_name: str):
        pbuilder = SwitchParentBuilder(self.generator)

        org_parent = self.get_bone_parent(self.bones.org)
        parents = [org_parent] if org_parent else []

        pbuilder.build_child(
            self, master_name,
            context_rig=self.rigify_parent, allow_self=True,
            prop_name="Parent ({})".format(master_name),
            extra_parents=parents, select_parent=org_parent,
            controls=lambda: self.bones.ctrl.flatten()
        )

    def get_parent_tags(self):
        tags = {t.strip() for t in self.params.register_parent_tags.split(',')}

        if self.params.make_parent_switch:
            tags.add('child')

        tags.discard('')
        return tags

    def register_parent(self, master_name: str, tags: set[str]):
        pbuilder = SwitchParentBuilder(self.generator)

        inject = self.rigify_parent if 'injected' in tags else None

        pbuilder.register_parent(
            self, self.bones.org, name=master_name,
            inject_into=inject, tags=tags
        )

    def parent_bones(self):
        ctrl = self.bones.ctrl

        if self.make_pivot:
            if self.make_control:
                self.set_bone_parent(ctrl.pivot, ctrl.master, use_connect=False)

            self.set_bone_parent(self.bones.org, ctrl.pivot, use_connect=False)

        else:
            self.set_bone_parent(self.bones.org, ctrl.master, use_connect=False)

        if self.make_deform:
            self.set_bone_parent(self.bones.deform, self.bones.org, use_connect=False)

    def configure_bones(self):
        org = self.bones.org
        ctrl = self.bones.ctrl
        main_ctl = ctrl.master if self.make_control else ctrl.pivot

        self.copy_bone_properties(org, main_ctl, ui_controls=True)

    def rig_bones(self):
        if self.make_pivot:
            self.make_constraint(
                self.bones.org, 'COPY_LOCATION', self.bones.ctrl.pivot,
                space='LOCAL', invert_xyz=(True,)*3
            )

    def generate_widgets(self):
        if self.make_pivot:
            create_pivot_widget(self.obj, self.bones.ctrl.pivot, square=True, axis_size=2.0)

        if self.make_control:
            set_bone_widget_transform(self.obj, self.bones.ctrl.master, self.bones.org)

            create_registered_widget(self.obj, self.bones.ctrl.master,
                                     self.params.pivot_master_widget_type or 'cube')

    @classmethod
    def add_parameters(cls, params):
        params.make_control = bpy.props.BoolProperty(
            name="Control",
            default=True,
            description="Create a control bone for the copy"
        )

        params.pivot_master_widget_type = bpy.props.StringProperty(
            name="Widget Type",
            default='cube',
            description="Choose the type of the widget to create"
        )

        params.make_parent_switch = bpy.props.BoolProperty(
            name="Switchable Parent",
            default=False,
            description="Allow switching the parent of the master control"
        )

        params.register_parent = bpy.props.BoolProperty(
            name="Register Parent",
            default=False,
            description="Register the control as a switchable parent candidate"
        )

        params.register_parent_tags = bpy.props.StringProperty(
            name="Parent Tags",
            default="",
            description="Comma-separated tags to use for the registered parent"
        )

        params.make_extra_control = bpy.props.BoolProperty(
            name="Extra Control",
            default=False,
            description="Create an optional control"
        )

        params.make_extra_deform = bpy.props.BoolProperty(
            name="Extra Deform",
            default=False,
            description="Create an optional deform bone"
        )

    @classmethod
    def parameters_ui(cls, layout, params):
        r = layout.row()
        r.prop(params, "make_extra_control", text="Master Control")

        if params.make_extra_control:
            layout_widget_dropdown(layout, params, "pivot_master_widget_type")

            layout.prop(params, "make_parent_switch")
            layout.prop(params, "register_parent")

            r = layout.row()
            r.active = params.register_parent
            r.prop(params, "register_parent_tags", text="Tags")

            layout.prop(params, "make_control", text="Pivot Control")

        r = layout.row()
        r.prop(params, "make_extra_deform", text="Deform Bone")


def create_sample(obj):
    """ Create a sample metarig for this rig type.
    """
    # generated by rigify.utils.write_metarig
    bpy.ops.object.mode_set(mode='EDIT')
    arm = obj.data

    bones = {}

    bone = arm.edit_bones.new('pivot')
    bone.head[:] = 0.0000, 0.0000, 0.0000
    bone.tail[:] = 0.0000, 0.5000, 0.0000
    bone.roll = 0.0000
    bone.use_connect = False
    bones['pivot'] = bone.name

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones[bones['pivot']]
    pbone.rigify_type = 'basic.pivot'
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
