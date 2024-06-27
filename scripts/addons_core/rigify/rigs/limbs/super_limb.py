# SPDX-FileCopyrightText: 2017-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy

from ...base_generate import SubstitutionRig

from .limb_rigs import BaseLimbRig

from . import arm, leg, paw


RIGS = {'arm': arm.Rig, 'leg': leg.Rig, 'paw': paw.Rig}


class Rig(SubstitutionRig):
    def substitute(self):
        return [self.instantiate_rig(RIGS[self.params.limb_type], self.base_bone)]


def add_parameters(params):
    items = [
        ('arm', 'Arm', ''),
        ('leg', 'Leg', ''),
        ('paw', 'Paw', '')
    ]

    params.limb_type = bpy.props.EnumProperty(
        items=items,
        name="Limb Type",
        default='arm'
    )

    BaseLimbRig.add_parameters(params)


def parameters_ui(layout, params):
    r = layout.row()
    r.prop(params, "limb_type")

    RIGS[params.limb_type].parameters_ui(layout, params)


def create_sample(obj):
    arm.create_sample(obj, limb=True)
