# SPDX-FileCopyrightText: 2017-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy

from ...utils.rig import connected_children_names
from ...utils.layers import ControlLayersOption
from ...utils.bones import BoneUtilityMixin, flip_bone_chain

from ...base_generate import SubstitutionRig

from . import basic_spine, basic_tail, super_head


class Rig(SubstitutionRig, BoneUtilityMixin):
    """Compatibility proxy for the monolithic super_spine rig that splits it into parts."""

    def substitute(self):
        params_copy = self.params_copy
        orgs = [self.base_bone] + connected_children_names(self.obj, self.base_bone)

        # Split the bone list according to the settings
        spine_orgs = orgs
        head_orgs = None
        tail_orgs = None

        pivot_pos = self.params.pivot_pos

        if self.params.use_head:
            neck_pos = self.params.neck_pos
            if neck_pos <= pivot_pos:
                self.raise_error("Neck cannot be below or the same as pivot.")
            if neck_pos >= len(orgs):
                self.raise_error("Neck is too short.")

            spine_orgs = orgs[0: neck_pos-1]
            head_orgs = orgs[neck_pos-1:]

        if self.params.use_tail:
            tail_pos = self.params.tail_pos
            if tail_pos < 2:
                self.raise_error("Tail is too short.")
            if tail_pos >= pivot_pos:
                self.raise_error("Tail cannot be above or the same as pivot.")

            tail_orgs = list(reversed(spine_orgs[0: tail_pos]))
            spine_orgs = spine_orgs[tail_pos:]
            pivot_pos -= tail_pos

        # Split the bone chain and flip the tail
        if head_orgs or tail_orgs:
            bpy.ops.object.mode_set(mode='EDIT')

            if spine_orgs[0] != orgs[0]:
                self.set_bone_parent(spine_orgs[0], self.get_bone_parent(orgs[0]))

            if head_orgs:
                self.get_bone(head_orgs[0]).use_connect = False

            if tail_orgs:
                flip_bone_chain(self.obj, reversed(tail_orgs))
                self.set_bone_parent(tail_orgs[0], spine_orgs[0])

            bpy.ops.object.mode_set(mode='OBJECT')

        # Create the parts
        self.assign_params(spine_orgs[0], params_copy, pivot_pos=pivot_pos, make_fk_controls=False)

        result = [self.instantiate_rig(basic_spine.Rig, spine_orgs[0])]

        if tail_orgs:
            self.assign_params(tail_orgs[0], params_copy, connect_chain=True)

            result += [self.instantiate_rig(basic_tail.Rig, tail_orgs[0])]

        if head_orgs:
            self.assign_params(head_orgs[0], params_copy, connect_chain=True)

            result += [self.instantiate_rig(super_head.Rig, head_orgs[0])]

        return result


def add_parameters(params):
    basic_spine.Rig.add_parameters(params)
    basic_tail.Rig.add_parameters(params)
    super_head.Rig.add_parameters(params)

    params.neck_pos = bpy.props.IntProperty(
        name='neck_position',
        default=6,
        min=0,
        description='Neck start position'
    )

    params.tail_pos = bpy.props.IntProperty(
        name='tail_position',
        default=2,
        min=2,
        description='Where the tail starts'
    )

    params.use_tail = bpy.props.BoolProperty(
        name='use_tail',
        default=False,
        description='Create tail bones'
    )

    params.use_head = bpy.props.BoolProperty(
        name='use_head',
        default=True,
        description='Create head and neck bones'
    )


def parameters_ui(layout, params):
    """ Create the ui for the rig parameters."""

    layout.label(text="Note: this combined rig is deprecated.", icon='INFO')

    r = layout.row(align=True)
    r.prop(params, "use_head", toggle=True, text="Head")
    r.prop(params, "use_tail", toggle=True, text="Tail")

    r = layout.row()
    r.prop(params, "neck_pos")
    r.enabled = params.use_head

    r = layout.row()
    r.prop(params, "pivot_pos")

    r = layout.row()
    r.prop(params, "tail_pos")
    r.enabled = params.use_tail

    r = layout.row()
    col = r.column(align=True)
    row = col.row(align=True)
    for i, axis in enumerate(['x', 'y', 'z']):
        row.prop(params, "copy_rotation_axes", index=i, toggle=True, text=axis)
    r.enabled = params.use_tail

    ControlLayersOption.TWEAK.parameters_ui(layout, params)


def create_sample(obj):
    bones = basic_spine.create_sample(obj)
    basic_tail.create_sample(obj, parent=bones['spine'])
    super_head.create_sample(obj, parent=bones['spine.003'])
