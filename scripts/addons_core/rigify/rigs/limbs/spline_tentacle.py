# SPDX-FileCopyrightText: 2014-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.app.translations import pgettext_iface as iface_

import re
import itertools
import bisect
import math
import json

from ...utils.naming import strip_org, make_derived_name
from ...utils.bones import set_bone_widget_transform
from ...utils.mechanism import make_driver, make_constraint, driver_var_transform
from ...utils.widgets import create_widget
from ...utils.widgets_basic import create_circle_widget, create_sphere_widget
from ...utils.layers import ControlLayersOption
from ...utils.misc import map_list, TypedObject
from ...utils.animation import add_generic_snap_fk_to_ik
from ...utils.switch_parent import SwitchParentBuilder

from ...base_rig import stage
from ...rig_ui_template import PanelLayout, UTILITIES_FUNC_COMMON_IK_FK

from ...rigs.chain_rigs import SimpleChainRig
from ...rigs.widgets import create_gear_widget

from typing import NamedTuple, Sequence
from itertools import count


class Rig(SimpleChainRig):
    ##############################
    # Initialization

    stretch_control_mode: str | None = None  # Override in a subclass to disable the Tip Control option

    name_base: str
    name_sep: str
    name_suffix: str

    spline_name: str
    spline_obj: TypedObject[bpy.types.Curve] | None

    use_stretch: bool
    use_tip: bool
    use_fk: bool
    use_radius: bool
    max_curve_radius: float

    org_lengths: list[float]
    org_tot_lengths: list[float]
    chain_length: float
    avg_length: float

    class PosSpec(NamedTuple):
        org_index: int
        head_tail: float
        name: str

    num_main_controls: int
    main_control_pos_list: list['Rig.PosSpec']
    start_control_pos_list: list['Rig.PosSpec']
    end_control_pos_list: list['Rig.PosSpec']

    def initialize(self):
        super().initialize()

        org_chain = self.bones.org
        org_bones = [self.get_bone(org) for org in org_chain]

        # Compute master bone name: inherit .LR suffix, but strip trailing digits
        name_parts = re.match(r'^(.*?)(?:([._-])\d+)?((?:[._-][LlRr])?)(?:\.\d+)?$',
                              strip_org(org_chain[0]))
        name_base, name_sep, name_suffix = name_parts.groups()

        self.name_base = name_base
        self.name_sep = name_sep if name_sep else '-'
        self.name_suffix = name_suffix

        # Create a spline object (replacing the old one if it exists)
        self.spline_obj = self.generator.artifacts.create_new(self, 'CURVE', 'spline')

        # Options
        if self.stretch_control_mode is None:
            self.stretch_control_mode = self.params.sik_stretch_control

        self.use_stretch = (self.stretch_control_mode == 'MANUAL_STRETCH')
        self.use_tip = (self.stretch_control_mode == 'DIRECT_TIP')
        self.use_fk = self.params.sik_fk_controls

        # Compute org chain lengths and control distribution
        if self.use_tip:
            org_bones.pop()

        self.org_lengths = [bone.length for bone in org_bones]
        self.org_tot_lengths = list(itertools.accumulate(self.org_lengths))
        self.chain_length = self.org_tot_lengths[-1]
        self.avg_length = self.chain_length / len(org_bones)

        end_idx = len(org_bones) - 1

        # Find which bones hold main controls
        self.num_main_controls = self.params.sik_mid_controls + 2
        main_control_step = self.chain_length / (self.num_main_controls - 1)

        self.main_control_pos_list = [
            self.find_bone_by_length(
                i * main_control_step, name=self.get_main_control_name(i))
            for i in range(self.num_main_controls)
        ]

        # Likewise for extra start and end controls
        num_start_controls = self.params.sik_start_controls
        start_range = main_control_step / self.org_lengths[0]
        start_control_step = start_range * 0.001 / max(1, num_start_controls)

        self.start_control_pos_list = [
            self.PosSpec(
                0, (i + 1) * start_control_step, self.make_name('start%02d' % (idx + 1)))
            for idx, i in enumerate(reversed(range(num_start_controls)))
        ]

        num_end_controls = self.params.sik_end_controls + (1 if self.use_tip else 0)
        end_range = main_control_step / self.org_lengths[-1]
        end_control_step = end_range * 0.001 / max(1, num_end_controls)

        self.end_control_pos_list = [
            self.PosSpec(
                end_idx, 1.0 - (i + 1) * end_control_step, self.make_name('end%02d' % (idx + 1)))
            for idx, i in enumerate(reversed(range(num_end_controls)))
        ]

        # Adjust control bindings if using manual tip control
        if self.use_tip:
            # tip = self.main_control_pos_list[-1]
            self.main_control_pos_list[-1] = self.PosSpec(end_idx + 1, 0, strip_org(org_chain[-1]))

            # tip_extra = self.end_control_pos_list[0]
            self.end_control_pos_list[0] = self.PosSpec(
                end_idx, max(0.0, 1 - end_range * 0.25), self.make_name('end'))

        # Radius scaling
        self.use_radius = self.params.sik_radius_scaling
        self.max_curve_radius = self.params.sik_max_radius if self.use_radius else 1.0

    ##############################
    # Utilities

    def find_bone_by_length(self, pos: float, name: str):
        tot_lengths = self.org_tot_lengths
        idx = bisect.bisect_left(tot_lengths, pos)
        idx = min(idx, len(tot_lengths) - 1)
        prev = tot_lengths[idx - 1] if idx > 0 else 0
        return self.PosSpec(idx, min(1.0, (pos - prev) / (tot_lengths[idx] - prev)), name)

    def make_name(self, mid_part: str):
        """Make a name for a bone not tied to a specific org bone"""
        return self.name_base + self.name_sep + mid_part + self.name_suffix

    def get_main_control_name(self, i: int):
        if i == 0:
            base = 'start'
        elif i == self.num_main_controls - 1:
            base = 'end'
        else:
            base = 'mid%02d' % i

        return self.make_name(base)

    def make_bone_by_spec(self, pos_spec: 'Rig.PosSpec', name: str, scale: float):
        """Make a bone positioned along the chain."""
        org_name = self.bones.org[pos_spec[0]]
        new_name = self.copy_bone(org_name, name, parent=False)

        org_bone = self.get_bone(org_name)
        new_bone = self.get_bone(new_name)
        new_bone.translate(pos_spec[1] * (org_bone.tail - org_bone.head))
        new_bone.length = self.avg_length * scale

        return new_name

    ENABLE_CONTROL_PROPERTY = [None, 'start_controls', 'end_controls']

    def rig_enable_control_driver(self, owner, prop: str, subtype: int, index: int, disable=False):
        if subtype != 0:
            if self.use_tip and subtype == 2:
                if index == 0:
                    return
                index -= 1

            master = self.bones.ctrl.master
            var_prop = self.ENABLE_CONTROL_PROPERTY[subtype]

            make_driver(
                owner, prop,
                expression='active %s %d' % ('<=' if disable else '>', index),
                variables={'active': (self.obj, master, var_prop)}
            )

    ##############################
    # BONES

    class CtrlBones(SimpleChainRig.CtrlBones):
        master: str                    # Root control for moving and scaling the whole rig.
        main: list[str]                # List of main spline controls (always visible and active).
        start: list[str]               # List of extra spline controls attached to the
        end: list[str]                 # tip main ones (can disable).
        end_twist: str                 # Twist control at the end of the tentacle.

    class MchBones(SimpleChainRig.MchBones):
        start_parent: str              # Intermediate bones for parenting extra controls
        end_parent: str                # - discards scale of the main.
        start_hooks: list[str]         # Proxy bones for extra control hooks.
        end_hooks: list[str]
        ik: list[str]                  # Spline IK chain, extracting the shape of the curve.
        ik_final: list[str]            # Final IK result with tip_fix.
        end_stretch: str               # Bone used in distributing the end twist control scaling.
        tip_fix_parent: str            # Bones used to match tip control rotation and scale.
        tip_fix: str

    bones: SimpleChainRig.ToplevelBones[
        list[str],
        'Rig.CtrlBones',
        'Rig.MchBones',
        list[str]
    ]

    ##############################
    # Master control bone

    @stage.generate_bones
    def make_master_control(self):
        self.bones.ctrl.master = self.copy_bone(
            self.bones.org[0], self.make_name('master'), parent=True,
            length=self.avg_length * 1.5
        )

        self.register_parents()

    def register_parents(self):
        builder = SwitchParentBuilder(self.generator)

        builder.register_parent(self, self.bones.ctrl.master)

        if self.use_tip:
            builder.register_parent(self, self.bones.org[-1], exclude_self=True)

    @stage.configure_bones
    def configure_master_control(self):
        master = self.bones.ctrl.master
        ctrls = self.bones.ctrl.flatten()
        rig_name = self.name_base + self.name_suffix
        panel = self.script.panel_with_selected_check(self, ctrls)

        # Properties for enabling extra controls
        if self.params.sik_start_controls > 0:
            self.make_property(
                master, 'start_controls', 0,
                min=0, max=self.params.sik_start_controls,
                description="Enabled extra start controls for " + rig_name
            )

            self.add_start_controls_buttons(panel, master, rig_name)

        if self.params.sik_end_controls > 0:
            self.make_property(
                master, 'end_controls', 0,
                min=0, max=self.params.sik_end_controls,
                description="Enabled extra end controls for " + rig_name
            )

            self.add_end_controls_buttons(panel, master, rig_name)

        # End twist correction for directly controllable tip
        if self.use_tip:
            max_val = len(self.bones.org) * math.pi

            self.make_property(
                master, 'end_twist', 0.0, min=-max_val, max=max_val,
                subtype='ANGLE', precision=0, step=1000.0,
                description="Rough end twist estimate. The rig auto-corrects it to the actual tip orientation "
                            "within 180 degrees of the specified value"
            )

            self.add_direct_tip_buttons(panel, master, rig_name)

        # IK/FK switch
        if self.use_fk:
            self.make_property(master, 'IK_FK', 0.0, description='IK/FK switch for ' + rig_name)

            self.add_fk_snap_buttons(panel, master, rig_name)

    def add_start_controls_buttons(self, panel: 'PanelLayout', master: str, _rig_name: str):
        row = panel.row(align=True)
        row.custom_prop(master, 'start_controls', text="Start Controls")

        ctrl_bones = self.bones.ctrl.start
        hook_bones = self.bones.mch.start_hooks

        add_toggle_control_button(row, prop_bone=master, prop_name='start_controls',
                                  ctrl_bones=ctrl_bones, hook_bones=hook_bones, enable=True)
        add_toggle_control_button(row, prop_bone=master, prop_name='start_controls',
                                  ctrl_bones=ctrl_bones, hook_bones=hook_bones, enable=False)

    def add_end_controls_buttons(self, panel: 'PanelLayout', master: str, _rig_name: str):
        row = panel.row(align=True)
        row.custom_prop(master, 'end_controls', text="End Controls")

        ctrl_bones = self.bones.ctrl.end
        hook_bones = self.bones.mch.end_hooks

        if self.use_tip:
            ctrl_bones = ctrl_bones[1:]
            hook_bones = hook_bones[1:]

        add_toggle_control_button(row, prop_bone=master, prop_name='end_controls',
                                  ctrl_bones=ctrl_bones, hook_bones=hook_bones, enable=True)
        add_toggle_control_button(row, prop_bone=master, prop_name='end_controls',
                                  ctrl_bones=ctrl_bones, hook_bones=hook_bones, enable=False)

    # noinspection PyMethodMayBeStatic
    def add_direct_tip_buttons(self, panel: 'PanelLayout', master: str, _rig_name: str):
        panel.custom_prop(master, 'end_twist', text="End Twist Estimate")

    def add_fk_snap_buttons(self, panel: 'PanelLayout', master: str, rig_name: str):
        panel.custom_prop(master, 'IK_FK', text="IK - FK", slider=True)

        ik_controls = [item[0] for item in self.all_controls]
        if not self.use_tip:
            ik_controls += [self.bones.ctrl.end_twist]

        add_generic_snap_fk_to_ik(
            panel,
            fk_bones=self.bones.ctrl.fk, ik_bones=self.get_ik_final(),
            ik_ctrl_bones=ik_controls,
            undo_copy_scale=True,
            rig_name=rig_name
        )

        add_spline_snap_ik_to_fk(
            panel,
            fk_bones=self.bones.ctrl.fk, ik_bones=self.get_ik_final(),
            ik_ctrl_bones=ik_controls,
            use_tip=self.use_tip,
            use_stretch=self.use_stretch,
            rig_name=rig_name
        )

    @stage.generate_widgets
    def make_master_control_widget(self):
        master_name = self.bones.ctrl.master
        create_gear_widget(self.obj, master_name, radius=0.5)

    ##############################
    # Twist controls

    @stage.generate_bones
    def make_twist_control_bones(self):
        if not self.use_tip:
            self.bones.ctrl.end_twist = self.make_twist_control_bone('end-twist', 1.15)

    def make_twist_control_bone(self, name, size):
        return self.copy_bone(self.bones.org[0], self.make_name(name),
                              length=self.avg_length * size)

    @stage.parent_bones
    def parent_twist_control_bones(self):
        if not self.use_tip:
            self.set_bone_parent(self.bones.ctrl.end_twist, self.bones.ctrl.master, inherit_scale='ALIGNED')

    @stage.configure_bones
    def configure_twist_control_bones(self):
        if not self.use_tip:
            self.configure_twist_control_bone(self.bones.ctrl.end_twist)

    def configure_twist_control_bone(self, name):
        bone = self.get_bone(name)
        bone.rotation_mode = 'XYZ'
        bone.lock_location = (True, True, True)
        bone.lock_rotation = (True, False, True)
        if not self.use_stretch:
            bone.lock_scale = (True, True, True)

    @stage.rig_bones
    def rig_twist_control_bones(self):
        if not self.use_tip:
            # Copy the location of the end bone to provide more convenient tool behavior.
            self.make_constraint(self.bones.ctrl.end_twist, 'COPY_LOCATION', self.bones.org[-1])

    @stage.generate_widgets
    def make_twist_control_widgets(self):
        if not self.use_tip:
            self.make_twist_control_widget(self.bones.ctrl.end_twist, self.bones.org[-1], 0.85)

    def make_twist_control_widget(self, ctrl, org, size=1.0, head_tail=0.5):
        set_bone_widget_transform(self.obj, ctrl, org, target_size=True)

        create_twist_widget(self.obj, ctrl, size=size, head_tail=head_tail)

    ##############################
    # Twist controls MCH

    @stage.generate_bones
    def make_mch_twist_control_bones(self):
        if self.use_stretch:
            self.bones.mch.end_stretch = self.make_mch_end_stretch_bone('end-twist.stretch', 1.15)

    def make_mch_end_stretch_bone(self, name_base, size):
        name = make_derived_name(self.make_name(name_base), 'mch')
        return self.copy_bone(self.bones.org[0], name, length=self.avg_length * size * 0.5)

    @stage.parent_bones
    def parent_mch_twist_control_bones(self):
        if self.use_stretch:
            self.set_bone_parent(self.bones.mch.end_stretch, self.bones.ctrl.master, inherit_scale='AVERAGE')

    @stage.rig_bones
    def rig_mch_twist_control_bones(self):
        if self.use_stretch:
            self.rig_mch_end_stretch_bone(self.bones.mch.end_stretch, self.bones.ctrl.end_twist)

    def rig_mch_end_stretch_bone(self, mch, ctrl):
        # Break the dependency cycle caused by COPY_LOCATION above by copying raw properties.
        self.make_driver(mch, 'scale', index=0, variables=[(ctrl, '.scale.x')])
        self.make_driver(mch, 'scale', index=1, variables=[(ctrl, '.scale.y')])
        self.make_driver(mch, 'scale', index=2, variables=[(ctrl, '.scale.z')])

        self.make_constraint(mch, 'MAINTAIN_VOLUME', mode='UNIFORM', owner_space='LOCAL')

    ##############################
    # Spline controls

    class ControlEntry(NamedTuple):
        bone: str
        subtype: int
        index: int

    tip_controls_table: list[str | None]
    all_controls: list['Rig.ControlEntry']

    @stage.generate_bones
    def make_main_control_chain(self):
        self.bones.ctrl.main = map_list(self.make_main_control_bone, self.main_control_pos_list)
        self.bones.ctrl.start = map_list(self.make_extra_control_bone, self.start_control_pos_list)
        self.bones.ctrl.end = map_list(self.make_extra_control_bone, self.end_control_pos_list)

        self.make_all_controls_list()
        self.make_controls_switch_parent()

    def make_all_controls_list(self):
        main_controls = [self.ControlEntry(bone, 0, i)
                         for i, bone in enumerate(self.bones.ctrl.main)]
        start_controls = [self.ControlEntry(bone, 1, i)
                          for i, bone in enumerate(self.bones.ctrl.start)]
        end_controls = [self.ControlEntry(bone, 2, i)
                        for i, bone in enumerate(self.bones.ctrl.end)]

        self.tip_controls_table = [None, self.bones.ctrl.main[0], self.bones.ctrl.main[-1]]
        self.all_controls = [main_controls[0], *reversed(start_controls),
                             *main_controls[1:-1],
                             *end_controls, main_controls[-1]]

    def make_controls_switch_parent(self):
        builder = SwitchParentBuilder(self.generator)

        def extra():
            return [
                (self.bones.mch.start_parent, self.bones.ctrl.main[0]),
                (self.bones.mch.end_parent, self.bones.ctrl.main[-1])
            ]

        select_table = [
            lambda: self.bones.ctrl.master,
            lambda: self.bones.mch.start_parent,
            lambda: self.bones.mch.end_parent
        ]

        for (bone, subtype, index) in self.all_controls[1:-1]:
            builder.build_child(
                self, bone, extra_parents=extra,
                select_parent=select_table[subtype],
                no_fix_rotation=True, no_fix_scale=True
            )

        builder.build_child(self, self.bones.ctrl.main[-1], no_fix_scale=not self.use_tip)

    def make_main_control_bone(self, pos_spec):
        return self.make_bone_by_spec(pos_spec, pos_spec[2], 1.1)

    def make_extra_control_bone(self, pos_spec):
        return self.make_bone_by_spec(pos_spec, pos_spec[2], 0.9)

    @stage.parent_bones
    def parent_main_control_chain(self):
        self.set_bone_parent(self.bones.ctrl.main[0], self.bones.ctrl.master, inherit_scale='ALIGNED')

    @stage.configure_bones
    def configure_main_control_chain(self):
        for info in self.all_controls:
            self.configure_main_control_bone(*info)

    def configure_main_control_bone(self, ctrl, subtype, index):
        bone = self.get_bone(ctrl)

        if subtype == 0 and index == 0:
            if self.params.sik_start_controls > 0:
                bone.rotation_mode = 'QUATERNION'
            else:
                bone.rotation_mode = 'XYZ'
                bone.lock_rotation = (True, False, True)
        elif (subtype == 0 and index == self.num_main_controls - 1
              and self.params.sik_end_controls > 0):
            bone.rotation_mode = 'QUATERNION'
        else:
            bone.lock_rotation_w = True
            bone.lock_rotation = (True, True, True)

        if subtype == 0 and index == 0:
            bone.lock_scale = (False, not self.use_stretch, False)
        elif not self.use_radius:
            bone.lock_scale = (True, True, True)

    @stage.rig_bones
    def rig_main_control_chain(self):
        for info in self.all_controls:
            self.rig_main_control_bone(*info)

    def rig_main_control_bone(self, ctrl, subtype, index):
        if self.use_stretch and subtype == 0 and index == 0:
            self.make_constraint(ctrl, 'MAINTAIN_VOLUME', mode='UNIFORM', owner_space='LOCAL')

        self.rig_enable_control_driver(
            self.get_bone(ctrl), 'hide', subtype, index, disable=True)

    @stage.generate_widgets
    def make_main_control_widgets(self):
        for info in self.all_controls:
            self.make_main_control_widget(*info)

    def make_main_control_widget(self, ctrl, subtype, index):
        if subtype == 0 and index == 0:
            if len(self.start_control_pos_list) > 0:
                create_twist_widget(self.obj, ctrl, size=1, head_tail=0.25)
            else:
                self.make_twist_control_widget(ctrl, self.bones.org[0], head_tail=0.25)
        elif self.use_tip and subtype == 0 and index == self.num_main_controls - 1:
            create_circle_widget(self.obj, ctrl, radius=0.5, head_tail=0.25)
        else:
            create_sphere_widget(self.obj, ctrl)

    ##############################
    # FK Control chain

    @stage.generate_bones
    def make_control_chain(self):
        if self.use_fk:
            super().make_control_chain()

    @stage.parent_bones
    def parent_control_chain(self):
        if self.use_fk:
            self.parent_bone_chain(self.bones.ctrl.fk, use_connect=True, inherit_scale='ALIGNED')
            self.set_bone_parent(self.bones.ctrl.fk[0], self.bones.ctrl.master, inherit_scale='ALIGNED')

    @stage.configure_bones
    def configure_control_chain(self):
        if self.use_fk:
            super().configure_control_chain()

            ControlLayersOption.FK.assign(self.params, self.obj, self.bones.ctrl.fk)

    @stage.generate_widgets
    def make_control_widgets(self):
        if self.use_fk:
            super().make_control_widgets()

    def make_control_widget(self, i, ctrl):
        create_circle_widget(self.obj, ctrl, radius=0.3, head_tail=0.5)

    ##############################
    # Spline tip parent MCH

    @stage.generate_bones
    def make_mch_extra_parent_bones(self):
        self.bones.mch.start_parent = \
            self.make_mch_extra_parent_bone(self.main_control_pos_list[0])
        self.bones.mch.end_parent = \
            self.make_mch_extra_parent_bone(self.main_control_pos_list[-1])

    def make_mch_extra_parent_bone(self, pos_spec):
        return self.make_bone_by_spec(
            pos_spec, make_derived_name(pos_spec[2], 'mch', '.psocket'), 0.40)

    @stage.parent_bones
    def parent_mch_extra_parent_bones(self):
        self.set_bone_parent(self.bones.mch.start_parent, self.bones.ctrl.master, inherit_scale='AVERAGE')
        self.set_bone_parent(self.bones.mch.end_parent, self.bones.ctrl.master, inherit_scale='AVERAGE')

    @stage.rig_bones
    def rig_mch_extra_parent_bones(self):
        self.rig_mch_extra_parent_bone(self.bones.mch.start_parent, self.bones.ctrl.main[0])
        self.rig_mch_extra_parent_bone(self.bones.mch.end_parent, self.bones.ctrl.main[-1])

    def rig_mch_extra_parent_bone(self, bone, ctrl):
        self.make_constraint(bone, 'COPY_LOCATION', ctrl)
        self.make_constraint(bone, 'COPY_ROTATION', ctrl)

    ##############################
    # Spline extra hook proxy MCH

    mch_hooks_table: list[None | str]

    @stage.generate_bones
    def make_mch_extra_hook_bones(self):
        self.bones.mch.start_hooks = map_list(
            self.make_mch_extra_hook_bone, self.start_control_pos_list)
        self.bones.mch.end_hooks = map_list(
            self.make_mch_extra_hook_bone, self.end_control_pos_list)

        self.mch_hooks_table = [None, self.bones.mch.start_hooks, self.bones.mch.end_hooks]

    def make_mch_extra_hook_bone(self, pos_spec):
        return self.make_bone_by_spec(
            pos_spec, make_derived_name(pos_spec[2], 'mch', '.hook'), 0.30)

    @stage.parent_bones
    def parent_mch_extra_hook_bones(self):
        for hook in self.bones.mch.start_hooks:
            self.set_bone_parent(hook, self.bones.mch.start_parent)

        for hook in self.bones.mch.end_hooks:
            self.set_bone_parent(hook, self.bones.mch.end_parent)

    @stage.rig_bones
    def rig_mch_extra_hook_bones(self):
        for (bone, subtype, index) in self.all_controls:
            hooks = self.mch_hooks_table[subtype]
            if hooks:
                self.rig_mch_extra_hook_bone(hooks[index], bone, subtype, index)

    def rig_mch_extra_hook_bone(self, hook, ctrl, subtype, index):
        tip_ctrl = self.tip_controls_table[subtype]

        con = self.make_constraint(hook, 'COPY_LOCATION', ctrl)
        self.rig_enable_control_driver(con, 'mute', subtype, index, disable=True)

        con = self.make_constraint(hook, 'COPY_SCALE', ctrl, space='LOCAL')
        self.rig_enable_control_driver(con, 'mute', subtype, index, disable=True)

        if subtype == 2 and not self.use_tip:
            con = self.make_constraint(hook, 'COPY_SCALE', tip_ctrl, space='LOCAL')
            self.rig_enable_control_driver(con, 'mute', subtype, index, disable=False)

    ##############################
    # Spline Object

    @stage.configure_bones
    def make_spline_object(self):
        spline_data = self.spline_obj.data
        spline_data.dimensions = '3D'

        self.make_spline_points(spline_data, self.all_controls)

        if self.use_radius:
            self.make_spline_keys(self.spline_obj, self.all_controls)

    def make_spline_points(self, spline_data, all_controls):
        spline = spline_data.splines.new('BEZIER')

        spline.bezier_points.add(len(all_controls) - 1)

        for i, (name, subtype, index) in enumerate(all_controls):
            point = spline.bezier_points[i]
            point.handle_left_type = point.handle_right_type = 'AUTO'
            point.co = point.handle_left = point.handle_right = self.get_bone(name).head

    def make_spline_keys(self, spline_obj, all_controls):
        spline_obj.shape_key_add(name='Basis', from_mix=False)

        controls = all_controls[1:-1] if self.use_tip else all_controls[1:]

        for i, (name, subtype, index) in enumerate(controls):
            key = spline_obj.shape_key_add(name=name, from_mix=False)
            key.value = 0.0
            key.data[i + 1].radius = 0.0

    @stage.rig_bones
    def rig_spline_object(self):
        for i, info in enumerate(self.all_controls):
            self.rig_spline_hook(i, *info)

        if self.use_radius:
            controls = self.all_controls[1:-1] if self.use_tip else self.all_controls[1:]

            for i, info in enumerate(controls):
                self.rig_spline_radius_shapekey(i, *info)

    def rig_spline_hook(self, i, ctrl, subtype, index):
        hooks = self.mch_hooks_table[subtype]
        bone = self.get_bone(ctrl)

        hook = self.spline_obj.modifiers.new(ctrl, 'HOOK')

        assert isinstance(hook, bpy.types.HookModifier)

        hook.object = self.obj
        hook.subtarget = hooks[index] if hooks else ctrl
        hook.center = bone.head
        hook.vertex_indices_set([i * 3, i * 3 + 1, i * 3 + 2])

    def rig_spline_radius_shapekey(self, i, ctrl, subtype, index):
        key = self.spline_obj.data.shape_keys.key_blocks[i + 1]

        assert key.name == ctrl

        hooks = self.mch_hooks_table[subtype]
        target = hooks[index] if hooks else ctrl

        expr = '1 - var'
        scale_var = [driver_var_transform(self.obj, target, type='SCALE_AVG', space='LOCAL')]

        make_driver(key, 'value', expression=expr, variables=scale_var)
        key.slider_min = 1 - self.max_curve_radius

    ##############################
    # Spline IK Chain MCH

    @stage.generate_bones
    def make_mch_ik_chain(self):
        orgs = self.bones.org[0:-1] if self.use_tip else self.bones.org
        self.bones.mch.ik = map_list(self.make_mch_ik_bone, orgs)

    def make_mch_ik_bone(self, org):
        return self.copy_bone(org, make_derived_name(org, 'mch', '.ik'))

    @stage.parent_bones
    def parent_mch_ik_chain(self):
        self.parent_bone_chain(self.bones.mch.ik, use_connect=True, inherit_scale='NONE')
        self.set_bone_parent(self.bones.mch.ik[0], self.bones.ctrl.main[0], inherit_scale='NONE')

    @stage.rig_bones
    def rig_mch_ik_chain(self):
        for i, args in enumerate(zip(self.bones.mch.ik)):
            self.rig_mch_ik_bone(i, *args)

        self.rig_mch_ik_constraint(self.bones.mch.ik[-1])

    def rig_mch_ik_bone(self, i, mch):
        self.get_bone(mch).rotation_mode = 'XYZ'

        num_ik = len(self.bones.org)

        # Apply end twist rotation
        rot_fac = 1.0 / num_ik
        if self.use_tip:
            rot_var = [(self.bones.ctrl.master, 'end_twist')]
        else:
            rot_var = [(self.bones.ctrl.end_twist, '.rotation_euler.y')]

        self.make_driver(
            mch, 'rotation_euler', index=1, expression='var * %f' % rot_fac, variables=rot_var)

        # Copy the common scale
        self.make_constraint(mch, 'COPY_SCALE', self.bones.ctrl.main[0])

        if self.use_stretch:
            self.make_constraint(
                mch, 'COPY_SCALE', self.bones.mch.end_stretch,
                use_offset=True, space='LOCAL',
                power=(i + 1) / num_ik
            )

    def rig_mch_ik_constraint(self, mch):
        ik_bone = self.get_bone(mch)

        make_constraint(
            ik_bone, 'SPLINE_IK', self.spline_obj,
            chain_count=len(self.bones.mch.ik),
            use_curve_radius=self.use_radius,
            y_scale_mode='BONE_ORIGINAL' if self.use_stretch else 'FIT_CURVE',
            xz_scale_mode='VOLUME_PRESERVE',
            use_original_scale=True,
        )

    ##############################
    # Tip matching MCH

    @stage.generate_bones
    def make_mch_tip_fix(self):
        if self.use_tip:
            org = self.bones.org[-1]
            self.bones.mch.tip_fix_parent = self.copy_bone(
                org, make_derived_name(org, 'mch', '.fix.parent'), scale=0.8)
            self.bones.mch.tip_fix = self.copy_bone(
                org, make_derived_name(org, 'mch', '.fix'), scale=0.7)

    @stage.parent_bones
    def parent_mch_tip_fix(self):
        if self.use_tip:
            self.set_bone_parent(self.bones.mch.tip_fix_parent, self.bones.mch.ik[-1], inherit_scale='NONE')
            self.set_bone_parent(self.bones.mch.tip_fix, self.bones.mch.tip_fix_parent, inherit_scale='ALIGNED')

    @stage.rig_bones
    def rig_mch_tip_fix(self):
        if self.use_tip:
            ctrl = self.bones.ctrl.main[-1]
            parent = self.bones.mch.tip_fix_parent
            fix = self.bones.mch.tip_fix

            # Rig the baseline bone as the end of the IK chain (scale, twist)
            self.rig_mch_ik_bone(len(self.bones.mch.ik), parent)

            # Align the baseline to the tip control direction
            self.make_constraint(parent, 'DAMPED_TRACK', ctrl, head_tail=1.0)

            # Deduce the scale and twist correction by subtracting baseline
            # from tip control transform via parenting and local space.
            self.make_constraint(fix, 'COPY_TRANSFORMS', ctrl)

    ###################################
    # Final IK Chain MCH (tip matched)

    @stage.generate_bones
    def make_mch_ik_final_chain(self):
        if self.use_tip:
            self.bones.mch.ik_final = map_list(self.make_mch_ik_final_bone, self.bones.org[0:-1])

    def make_mch_ik_final_bone(self, org):
        return self.copy_bone(org, make_derived_name(org, 'mch', '.ik.final'))

    def get_ik_final(self):
        if self.use_tip:
            return [*self.bones.mch.ik_final, self.bones.ctrl.main[-1]]
        else:
            return self.bones.mch.ik

    @stage.parent_bones
    def parent_mch_ik_final_chain(self):
        if self.use_tip:
            for final, ik in zip(self.bones.mch.ik_final, self.bones.mch.ik):
                self.set_bone_parent(final, ik, inherit_scale='ALIGNED')

    @stage.rig_bones
    def rig_mch_ik_final_chain(self):
        if self.use_tip:
            for args in zip(count(0), self.bones.mch.ik_final):
                self.rig_mch_ik_final_bone(*args)

    def rig_mch_ik_final_bone(self, i, mch):
        fix = self.bones.mch.tip_fix
        factor = (i + 1) / len(self.bones.org)

        self.make_constraint(
            mch, 'COPY_ROTATION', fix, space='LOCAL',
            use_x=False, use_z=False, influence=factor
        )
        self.make_constraint(
            mch, 'COPY_SCALE', fix, space='LOCAL',
            use_y=False, power=factor
        )

    ##############################
    # ORG chain

    @stage.parent_bones
    def parent_org_chain(self):
        self.set_bone_parent(self.bones.org[0], self.bones.ctrl.master, inherit_scale='ALIGNED')

    @stage.rig_bones
    def rig_org_chain(self):
        for args in zip(count(0), self.bones.org, self.get_ik_final()):
            self.rig_org_bone(*args)

    def rig_org_bone(self, i, org, ik):
        self.make_constraint(org, 'COPY_TRANSFORMS', ik)

        if self.use_fk:
            con = self.make_constraint(org, 'COPY_TRANSFORMS', self.bones.ctrl.fk[i])

            self.make_driver(con, 'influence', variables=[(self.bones.ctrl.master, 'IK_FK')])

    ##############################
    # UI

    @classmethod
    def add_parameters(cls, params):
        """ Register the rig parameters. """

        params.sik_start_controls = bpy.props.IntProperty(
            name="Extra Start Controls", min=0, default=1,
            description="Number of extra spline control points attached to the start control"
        )
        params.sik_mid_controls = bpy.props.IntProperty(
            name="Middle Controls", min=1, default=1,
            description="Number of spline control points in the middle"
        )
        params.sik_end_controls = bpy.props.IntProperty(
            name="Extra End Controls", min=0, default=1,
            description="Number of extra spline control points attached to the end control"
        )

        params.sik_stretch_control = bpy.props.EnumProperty(
            name="Tip Control",
            description="How the stretching of the tentacle is controlled",
            items=[('FIT_CURVE', 'Stretch To Fit', 'The tentacle stretches to fit the curve'),
                   ('DIRECT_TIP', 'Direct Tip Control',
                    'The last bone of the chain is directly controlled, like the hand in an IK '
                    'arm, and the middle stretches to reach it'),
                   ('MANUAL_STRETCH', 'Manual Squash & Stretch',
                    'The tentacle scaling is manually controlled via twist controls.')]
        )

        params.sik_radius_scaling = bpy.props.BoolProperty(
            name="Radius Scaling", default=True,
            description="Allow scaling the spline control bones to affect the thickness via "
                        "curve radius"
        )
        params.sik_max_radius = bpy.props.FloatProperty(
            name="Maximum Radius", min=1, default=10,
            description="Maximum supported scale factor for the spline control bones"
        )

        params.sik_fk_controls = bpy.props.BoolProperty(
            name="FK Controls", default=True,
            description="Generate an FK control chain for the tentacle"
        )

        ControlLayersOption.FK.add_parameters(params)

    @classmethod
    def parameters_ui(cls, layout, params):
        """ Create the ui for the rig parameters. """

        layout.label(icon='INFO', text='A straight line rest shape works best.')

        layout.prop(params, 'sik_start_controls')
        layout.prop(params, 'sik_mid_controls')
        layout.prop(params, 'sik_end_controls')

        if cls.stretch_control_mode is None:
            layout.prop(params, 'sik_stretch_control', text='')

        layout.prop(params, 'sik_radius_scaling')

        col = layout.column()
        col.active = params.sik_radius_scaling
        col.prop(params, 'sik_max_radius')

        layout.prop(params, 'sik_fk_controls')

        col = layout.column()
        col.active = params.sik_fk_controls
        ControlLayersOption.FK.parameters_ui(col, params)


###########################
# Limb IK to FK operator ##
###########################

SCRIPT_REGISTER_OP_SNAP_IK_FK = ['POSE_OT_rigify_spline_tentacle_ik2fk']

SCRIPT_UTILITIES_OP_SNAP_IK_FK = UTILITIES_FUNC_COMMON_IK_FK + ['''
########################
## Limb Snap IK to FK ##
########################

class RigifySplineTentacleIk2FkBase:
    fk_bones:     StringProperty(name="FK Bone Chain")
    ik_bones:     StringProperty(name="IK Result Bone Chain")
    ctrl_bones:   StringProperty(name="IK Controls")
    use_tip:      bpy.props.BoolProperty(name="Direct Tip Control")
    use_stretch:  bpy.props.BoolProperty(name="Manual Stretch")

    def init_execute(self, context):
        self.fk_bone_list = json.loads(self.fk_bones)
        self.ik_bone_list = json.loads(self.ik_bones)
        self.ctrl_bone_list = json.loads(self.ctrl_bones)
        if not self.use_tip:
            self.twist_control_bone = self.ctrl_bone_list.pop()

    def save_frame_state(self, context, obj):
        matrices = get_chain_transform_matrices(obj, self.fk_bone_list)
        if not self.use_tip:
            last_tail = matrices[-1].copy()
            last_tail.translation = obj.pose.bones[self.fk_bone_list[-1]].tail
            matrices.append(last_tail)
        return matrices

    def apply_frame_state(self, context, obj, all_matrices):
        ik_bones = [obj.pose.bones[k] for k in self.ik_bone_list]
        ctrl_bones = [obj.pose.bones[k] for k in self.ctrl_bone_list]

        # Reset transformation of controls
        for name in self.ctrl_bone_list + ([] if self.use_tip else [self.twist_control_bone]):
            set_transform_from_matrix(obj, name, Matrix.Identity(4), space='LOCAL', keyflags=self.keyflags)

        # Position the first and last controls and update to ensure switchable parent is ready
        set_transform_from_matrix(
            obj, self.ctrl_bone_list[0], all_matrices[0], keyflags=self.keyflags, no_scale=True, no_rot=True)

        set_transform_from_matrix(
            obj, self.ctrl_bone_list[-1], all_matrices[-1], keyflags=self.keyflags, no_scale=not self.use_tip)

        context.view_layer.update()

        # Find currently enabled controls
        visible_ctrls = [
            bone for bone in ctrl_bones[1:-1]
            if not (bone.hide and obj.animation_data.drivers.find(bone.path_from_id("hide")))
        ]
        ctrl_count = len(visible_ctrls) + (0 if self.use_tip else 1)
        max_pos = 1 - 0.25 / ctrl_count
        ctrl_points = [
            (min((i+1) / ctrl_count, max_pos), ctrl) for i, ctrl in enumerate(visible_ctrls)
        ]

        # Measure the fk polyline
        points = [m.translation for m in all_matrices]
        lengths = [(n - p).length for p, n in zip(points, points[1:])]
        tot_length = sum(lengths)

        # Snap visible controls evenly to the polyline
        total = 0

        for seg_len, p, n in zip(lengths, points, points[1:]):
            prev_total = total
            total += seg_len / tot_length
            while ctrl_points and ctrl_points[0][0] <= total:
                fac, ctrl = ctrl_points.pop(0)
                fac = (fac - prev_total) / (total - prev_total)
                assert 0 <= fac <= 1
                pos = p * (1 - fac) + n * fac
                set_transform_from_matrix(
                    obj, ctrl.name, Matrix.Translation(pos), keyflags=self.keyflags, no_scale=True, no_rot=True)

        # Initial approximation of twist and scale for the base control
        context.view_layer.update()

        base_error_rotation = ik_bones[0].matrix.to_quaternion().inverted() @ all_matrices[0].to_quaternion()
        base_twist = base_error_rotation.to_swing_twist('Y')[1]
        base_scale = [b / a for a, b in zip(ik_bones[0].matrix.to_scale(), all_matrices[0].to_scale())]

        if self.use_stretch and any(con.type == 'MAINTAIN_VOLUME' and not con.mute
                                    for con in ctrl_bones[0].constraints):
            # Compensate for the maintain volume constraint
            base_scale[0] *= pow(base_scale[1], 1.5)
            base_scale[2] *= pow(base_scale[1], 1.5)

        if self.use_tip:
            # Compensate for the effect of targeting the tip bone orientation
            chain_len = len(all_matrices)
            tip_factor = chain_len / (chain_len - 1)
            base_twist = base_twist * tip_factor
            base_scale = [pow(s, tip_factor) for s in base_scale]

        set_transform_from_matrix(
            obj, self.ctrl_bone_list[0], Matrix.LocRotScale(None, Euler((0, base_twist, 0)), base_scale),
            space='LOCAL', keyflags=self.keyflags, no_loc=True
        )

        # Approximation for the tip twist control, and correction for the base control
        if not self.use_tip:
            context.view_layer.update()

            tip_error_rotation = ik_bones[-1].matrix.to_quaternion().inverted() @ all_matrices[-1].to_quaternion()
            tip_twist = tip_error_rotation.to_swing_twist('Y')[1]
            tip_scale = [b / a for a, b in zip(ik_bones[-1].matrix.to_scale(), all_matrices[-1].to_scale())]

            if self.use_stretch:
                # Compensate for the maintain volume constraint
                tip_scale[0] *= pow(tip_scale[1], 1.5)
                tip_scale[2] *= pow(tip_scale[1], 1.5)
            else:
                tip_scale = [1, 1, 1]

            # Compensate for the fraction removed from the base below
            chain_len = len(all_matrices) - 1
            tip_factor = chain_len / (chain_len - 1)
            new_tip_twist = tip_twist * tip_factor
            new_tip_scale = [pow(s, tip_factor) for s in tip_scale]

            set_transform_from_matrix(
                obj, self.twist_control_bone, Matrix.LocRotScale(None, Euler((0, new_tip_twist, 0)), new_tip_scale),
                space='LOCAL', keyflags=self.keyflags, no_loc=True, no_scale=not self.use_stretch
            )

            # A fraction of the tip scale and twist is applied to the base bone too, so remove it
            new_base_twist = base_twist - tip_twist / (chain_len - 1)
            new_base_scale = [b / pow(t, 1 / (chain_len - 1)) for b, t in zip(base_scale, tip_scale)]

            set_transform_from_matrix(
                obj, self.ctrl_bone_list[0], Matrix.LocRotScale(None, Euler((0, new_base_twist, 0)), new_base_scale),
                space='LOCAL', keyflags=self.keyflags, no_loc=True
            )

class POSE_OT_rigify_spline_tentacle_ik2fk(RigifySplineTentacleIk2FkBase, RigifySingleUpdateMixin, bpy.types.Operator):
    bl_idname = "pose.rigify_spline_tentacle_ik2fk_" + rig_id
    bl_label = "Snap IK->FK"
    bl_description = "Approximately snap the IK chain to FK result. Note that this will never produce an exact match"
''']


def add_spline_snap_ik_to_fk(panel: 'PanelLayout', *,
                             fk_bones: Sequence[str],
                             ik_bones: Sequence[str],
                             ik_ctrl_bones: Sequence[str],
                             use_tip: bool, use_stretch: bool,
                             rig_name=''):
    panel.use_bake_settings()
    panel.script.add_utilities(SCRIPT_UTILITIES_OP_SNAP_IK_FK)
    panel.script.register_classes(SCRIPT_REGISTER_OP_SNAP_IK_FK)

    op_props = {
        'fk_bones': json.dumps(fk_bones),
        'ik_bones': json.dumps(ik_bones),
        'ctrl_bones': json.dumps(ik_ctrl_bones),
        'use_tip': use_tip,
        'use_stretch': use_stretch,
    }

    text = iface_("IK->FK ({:s})").format(rig_name)
    panel.operator(
        'pose.rigify_spline_tentacle_ik2fk_{rig_id}',
        text=text,
        translate=False,
        icon='SNAP_ON',
        properties=op_props
    )


SCRIPT_REGISTER_OP_TOGGLE_CONTROLS = ['POSE_OT_rigify_spline_tentacle_toggle_control']

SCRIPT_UTILITIES_OP_TOGGLE_CONTROLS = ['''
#####################################
## Toggle Spline Tentacle Controls ##
#####################################

class RigifySplineTentacleToggleControlBase:
    prop_bone:      StringProperty(name="Settings Bone")
    prop_name:      StringProperty(name="Switch Property")

    ctrl_bones:     StringProperty(name="Control Bones")
    hook_bones:     StringProperty(name="Hook Bones")

    @classmethod
    def poll(cls, context):
        return find_action(context.active_object) is not None

    def check_increment(self, obj, delta):
        bone = obj.pose.bones[self.prop_bone]
        ui_data = bone.id_properties_ui(self.prop_name).as_dict()
        return ui_data["min"] <= bone[self.prop_name] + delta <= ui_data["max"]

    def get_property(self, obj):
        return obj.pose.bones[self.prop_bone][self.prop_name]

    def set_property(self, obj, value):
        obj.pose.bones[self.prop_bone][self.prop_name] = value

    def keyframe_increment(self, context, obj, delta):
        assert isinstance(obj, bpy.types.Object), "Expected {} to be an object".format(obj)

        action = obj.animation_data.action
        action_slot = obj.animation_data.action_slot

        assert action, "Expected {} to have an Action assigned".format(obj.name)
        assert action_slot, "Expected {} to have an Action slot assigned".format(obj.name)

        bone = obj.pose.bones[self.prop_bone]
        prop_quoted = rna_idprop_quote_path(self.prop_name)

        # Find the F-Curve
        data_path = bone.path_from_id(prop_quoted)
        channelbag = anim_utils.action_ensure_channelbag_for_slot(action, action_slot)
        fcurve = channelbag.fcurves.ensure(data_path, group_name=self.prop_bone)

        # Ensure the current value is keyed at the start of the animation
        keyflags = get_keying_flags(context)
        frame = context.scene.frame_current

        if len(fcurve.keyframe_points) == 0:
            min_x = min(fcu.keyframe_points[0].co[0] for fcu in channelbag.fcurves if len(fcu.keyframe_points) > 0)
            min_frame = nla_tweak_to_scene(obj.animation_data, min_x)
            if min_frame < frame:
                bone.keyframe_insert(prop_quoted, frame=min_frame, options=keyflags)

        # Keyframe the new value
        cur_value = bone[self.prop_name]
        new_value = cur_value + delta

        bone[self.prop_name] = new_value
        bone.keyframe_insert(prop_quoted, frame=frame, options=keyflags)

        # Ensure constant interpolation
        for key in fcurve.keyframe_points:
            key.interpolation = 'CONSTANT'

        return fcurve, cur_value, new_value

    def get_hook_bone(self, obj, index):
        hook_bones = json.loads(self.hook_bones)
        return obj.pose.bones[hook_bones[index]]

    def get_control_bone(self, obj, index):
        ctrl_bones = json.loads(self.ctrl_bones)
        return obj.pose.bones[ctrl_bones[index]]

    def get_hook_position(self, obj, index):
        hook = self.get_hook_bone(obj, index)

        hook_matrix_pose = hook.matrix.copy()
        hook_matrix_local = obj.convert_space(
            pose_bone=hook, matrix=hook_matrix_pose, from_space='POSE', to_space='LOCAL')

        return hook_matrix_pose.translation, hook_matrix_local.to_scale()

    def set_control_position(self, context, obj, index, location, scale):
        ctrl = self.get_control_bone(obj, index)

        ctrl_matrix_local = obj.convert_space(
            pose_bone=ctrl, matrix=Matrix.Translation(location), from_space='POSE', to_space='LOCAL')

        ctrl.location = ctrl_matrix_local.translation
        ctrl.scale = scale

        keyframe_transform_properties(obj, ctrl.name, get_keying_flags(context), no_rot=True)

class POSE_OT_rigify_spline_tentacle_toggle_control(RigifySplineTentacleToggleControlBase, bpy.types.Operator):
    bl_idname = "pose.rigify_spline_tentacle_toggle_control_" + rig_id
    bl_label = "Toggle And Keyframe Extra Control"
    bl_options = {'UNDO', 'INTERNAL', 'REGISTER'}

    enable: bpy.props.BoolProperty(name="Enable a control")

    @classmethod
    def description(cls, context, props):
        return (("Enable" if props.enable else "Disable") +
                " one more extra control in the middle of an animation, appropriately keyframing its position"
                " and the count property to preserve animation continuity")

    def execute(self, context):
        obj = context.active_object
        delta = 1 if self.enable else -1

        if not self.check_increment(obj, delta):
            self.report({'ERROR'}, "There are no more controls to {'enable' if self.enable else 'disable'}")
            return {'CANCELED'}

        # Find the control bone
        index = self.get_property(obj) + min(0, delta)
        ctrl = self.get_control_bone(obj, index)

        # Select the master control if hiding the active bone
        select_bone = None

        if not self.enable and obj.data.bones.active == ctrl.bone:
            select_bone = obj.pose.bones[self.prop_bone].bone

        # Capture the hook transform when the control is disabled, and keyframe toggle
        if self.enable:
            loc, scale = self.get_hook_position(obj, index)

            self.keyframe_increment(context, obj, 1)
        else:
            self.keyframe_increment(context, obj, -1)

            obj.update_tag(refresh={'DATA'})
            context.view_layer.update()

            loc, scale = self.get_hook_position(obj, index)

        # Keyframe the passive hook transform onto the control
        self.set_control_position(context, obj, index, loc, scale)

        # Select the new control if enabling it
        if self.enable:
            select_bone = ctrl.bone

        if select_bone is not None:
            for bone in obj.data.bones:
                bone.select = bone == select_bone
            obj.data.bones.active = select_bone

        obj.update_tag(refresh={'DATA'})
        return {'FINISHED'}
''']


def add_toggle_control_button(panel: 'PanelLayout', *,
                              prop_bone: str,
                              prop_name: str,
                              ctrl_bones: Sequence[str],
                              hook_bones: Sequence[str],
                              enable=True,
                              text=''):
    panel.use_bake_settings()
    panel.script.add_utilities(SCRIPT_UTILITIES_OP_TOGGLE_CONTROLS)
    panel.script.register_classes(SCRIPT_REGISTER_OP_TOGGLE_CONTROLS)

    op_props = {
        'prop_bone': prop_bone,
        'prop_name': prop_name,
        'ctrl_bones': json.dumps(ctrl_bones),
        'hook_bones': json.dumps(hook_bones),
        'enable': enable,
    }

    row = panel.row(align=True)

    if enable:
        row.enabled = row.expr_bone(prop_bone)[prop_name] < len(ctrl_bones)
    else:
        row.enabled = row.expr_bone(prop_bone)[prop_name] > 0

    row.operator('pose.rigify_spline_tentacle_toggle_control_{rig_id}', text=text,
                 icon='ADD' if enable else 'REMOVE',
                 properties=op_props)


def create_twist_widget(rig, bone_name, size=1.0, head_tail=0.5, bone_transform_name=None):
    obj = create_widget(rig, bone_name, bone_transform_name)
    if obj is not None:
        verts = [(0.3429814279079437 * size, head_tail, 0.22917263209819794 * size),
                 (0.38110050559043884 * size, head_tail - 0.05291016772389412 * size, 0.1578568667 * size),
                 (0.40457412600517273 * size, head_tail - 0.05291016772389412 * size, 0.0804747119 * size),
                 (0.41250014305114746 * size, head_tail - 0.05291016772389412 * size, 0.0),
                 (0.40457412600517273 * size, head_tail - 0.05291016772389412 * size, -0.080474764 * size),
                 (0.38110050559043884 * size, head_tail - 0.05291016772389412 * size, -0.157856911 * size),
                 (0.3429814279079437 * size, head_tail, -0.22917278110980988 * size),
                 (0.22917293012142181 * size, head_tail, -0.3429813086986542 * size),
                 (0.1578570008277893 * size, head_tail - 0.05291016772389412 * size, -0.3811003565 * size),
                 (0.0804748609662056 * size, head_tail - 0.05291016772389412 * size, -0.4045739769 * size),
                 (0.0, head_tail - 0.05291026830673218 * size, -0.4124999940395355 * size),
                 (-0.080474711954593 * size, head_tail - 0.052910167723892 * size, -0.40457397699 * size),
                 (-0.15785688161849 * size, head_tail - 0.05291016772394 * size, -0.38110026717974 * size),
                 (-0.22917267680168152 * size, head_tail, -0.3429811894893646 * size),
                 (-0.34298115968704224 * size, head_tail, -0.22917254269123077 * size),
                 (-0.38110023736953 * size, head_tail - 0.05291016772389 * size, -0.15785665810108 * size),
                 (-0.40457373857498 * size, head_tail - 0.05291016772389 * size, -0.08047446608543 * size),
                 (-0.4124998152256012 * size, head_tail - 0.05291016772389412 * size, 0.0),
                 (-0.40457355976104 * size, head_tail - 0.05291016772389 * size, 0.080475136637687 * size),
                 (-0.38109982013702 * size, head_tail - 0.05291016772389 * size, 0.157857269048690 * size),
                 (-0.34298068284988403 * size, head_tail, 0.22917301952838898 * size),
                 (-0.2291719913482666 * size, head_tail, 0.34298139810562134 * size),
                 (-0.15785618126392 * size, head_tail - 0.05291016772389 * size, 0.38110047578811 * size),
                 (-0.08047392964363 * size, head_tail - 0.05291016772389 * size, 0.40457388758659 * size),
                 (0.0, head_tail - 0.05291016772389412 * size, 0.41249993443489075 * size),
                 (0.080475620925426 * size, head_tail - 0.05291016772389 * size, 0.40457367897033 * size),
                 (0.157857790589332 * size, head_tail - 0.05291016772389 * size, 0.38109987974166 * size),
                 (0.22917351126670837 * size, head_tail, 0.3429807126522064 * size),
                 (0.381100505590438 * size, head_tail + 0.05290994420647 * size, 0.15785686671733 * size),
                 (0.404574126005172 * size, head_tail + 0.05290994420647 * size, 0.08047470450401 * size),
                 (0.41250014305114746 * size, head_tail + 0.05290994420647621 * size, 0.0),
                 (0.404574126005172 * size, head_tail + 0.05290994420647 * size, -0.0804747715592 * size),
                 (0.381100505590438 * size, head_tail + 0.05290994420647 * size, -0.1578569114208 * size),
                 (0.157857000827789 * size, head_tail + 0.05290994420647 * size, -0.3811003565788 * size),
                 (0.080474860966205 * size, head_tail + 0.05290994420647 * size, -0.4045739769935 * size),
                 (0.0, head_tail + 0.05290984362363815 * size, -0.4124999940395355 * size),
                 (-0.08047471195459 * size, head_tail + 0.05290994420647 * size, -0.4045739769935 * size),
                 (-0.15785688161849 * size, head_tail + 0.05290994420647 * size, -0.38110026717185 * size),
                 (-0.38110023736953 * size, head_tail + 0.05290994420647 * size, -0.15785665810108 * size),
                 (-0.40457373857498 * size, head_tail + 0.05290994420647 * size, -0.08047447353601 * size),
                 (-0.41249981522560 * size, head_tail + 0.05290994420647 * size, 0.0),
                 (-0.40457355976104 * size, head_tail + 0.05290994420647 * size, 0.080475129187107 * size),
                 (-0.38109982013702 * size, head_tail + 0.05290994420647 * size, 0.157857269048690 * size),
                 (-0.15785618126392 * size, head_tail + 0.05290994420647 * size, 0.381100475788116 * size),
                 (-0.08047392964363 * size, head_tail + 0.05290994420647 * size, 0.404573887586593 * size),
                 (0.0, head_tail + 0.05290994420647621 * size, 0.41249993443489075 * size),
                 (0.080475620925426 * size, head_tail + 0.05290994420647 * size, 0.404573678970339 * size),
                 (0.157857790589332 * size, head_tail + 0.05290994420647 * size, 0.381099879741667 * size)]
        edges = [(1, 0), (2, 1), (2, 3), (3, 4), (5, 4), (5, 6), (7, 8), (9, 8), (10, 9), (10, 11),
                 (12, 11), (12, 13), (14, 15), (16, 15), (16, 17), (17, 18), (19, 18), (20, 19),
                 (28, 0), (21, 22), (23, 22), (23, 24), (24, 25), (26, 25), (26, 27), (47, 27),
                 (29, 28), (29, 30), (30, 31), (32, 31), (32, 6), (34, 33), (35, 34), (35, 36),
                 (37, 36), (7, 33), (37, 13), (39, 38), (39, 40), (40, 41), (42, 41), (14, 38),
                 (20, 42), (44, 43), (44, 45), (45, 46), (47, 46), (21, 43)]
        faces = []

        mesh = obj.data
        mesh.from_pydata(verts, edges, faces)
        mesh.update()
        mesh.update()
        return obj
    else:
        return None


def create_sample(obj):
    # generated by ...utils.write_metarig
    bpy.ops.object.mode_set(mode='EDIT')
    arm = obj.data

    bones = {}

    bone = arm.edit_bones.new('tentacle01')
    bone.head[:] = 0.0000, 0.0000, 0.0000
    bone.tail[:] = 0.0000, 0.0000, 0.2000
    bone.roll = 0.0000
    bone.use_connect = False
    bones['tentacle01'] = bone.name
    bone = arm.edit_bones.new('tentacle02')
    bone.head[:] = 0.0000, 0.0000, 0.2000
    bone.tail[:] = 0.0000, 0.0000, 0.4000
    bone.roll = 0.0000
    bone.use_connect = True
    bone.inherit_scale = 'ALIGNED'
    bone.parent = arm.edit_bones[bones['tentacle01']]
    bones['tentacle02'] = bone.name
    bone = arm.edit_bones.new('tentacle03')
    bone.head[:] = 0.0000, 0.0000, 0.4000
    bone.tail[:] = 0.0000, 0.0000, 0.6000
    bone.roll = 0.0000
    bone.use_connect = True
    bone.inherit_scale = 'ALIGNED'
    bone.parent = arm.edit_bones[bones['tentacle02']]
    bones['tentacle03'] = bone.name
    bone = arm.edit_bones.new('tentacle04')
    bone.head[:] = 0.0000, 0.0000, 0.6000
    bone.tail[:] = 0.0000, 0.0000, 0.8000
    bone.roll = 0.0000
    bone.use_connect = True
    bone.inherit_scale = 'ALIGNED'
    bone.parent = arm.edit_bones[bones['tentacle03']]
    bones['tentacle04'] = bone.name
    bone = arm.edit_bones.new('tentacle05')
    bone.head[:] = 0.0000, 0.0000, 0.8000
    bone.tail[:] = 0.0000, 0.0000, 1.0000
    bone.roll = 0.0000
    bone.use_connect = True
    bone.inherit_scale = 'ALIGNED'
    bone.parent = arm.edit_bones[bones['tentacle04']]
    bones['tentacle05'] = bone.name

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones[bones['tentacle01']]
    pbone.rigify_type = 'limbs.spline_tentacle'
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['tentacle02']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['tentacle03']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['tentacle04']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['tentacle05']]
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
