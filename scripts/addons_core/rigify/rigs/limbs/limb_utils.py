# SPDX-FileCopyrightText: 2014-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
import re

from mathutils import Vector
from ...utils import org, strip_org, make_mechanism_name, make_deformer_name
from ...utils import MetarigError

bilateral_suffixes = ['.L', '.R']


def orient_bone(cls, eb, axis, scale=1.0, reverse=False):
    v = Vector((0, 0, 0))

    setattr(v, axis, scale)

    if reverse:
        tail_vec = v @ cls.obj.matrix_world
        eb.head[:] = eb.tail
        eb.tail[:] = eb.head + tail_vec
    else:
        tail_vec = v @ cls.obj.matrix_world
        eb.tail[:] = eb.head + tail_vec

    eb.roll = 0.0


def make_constraint(cls, bone, constraint):
    bpy.ops.object.mode_set(mode='OBJECT')
    pb = cls.obj.pose.bones

    owner_pb = pb[bone]
    const = owner_pb.constraints.new(constraint['constraint'])

    constraint['target'] = cls.obj

    # filter constraint props to those that actually exist in the current
    # type of constraint, then assign values to each
    for p in [k for k in constraint.keys() if k in dir(const)]:
        if p in dir(const):
            setattr(const, p, constraint[p])
        else:
            raise MetarigError(
                "RIGIFY ERROR: property %s does not exist in %s constraint" % (
                    p, constraint['constraint']
                ))


def get_bone_name(name, btype, suffix=''):
    # RE pattern match right or left parts
    # match the letter "L" (or "R"), followed by an optional dot (".")
    # and 0 or more digits at the end of the string
    pattern = r'^(\S+)(\.\S+)$'

    name = strip_org(name)

    types = {
        'mch': make_mechanism_name(name),
        'org': org(name),
        'def': make_deformer_name(name),
        'ctrl': name
    }

    name = types[btype]

    if suffix:
        results = re.match(pattern,  name)

        if results:
            bone_name, addition = results.groups()
            name = bone_name + "_" + suffix + addition
        else:
            name = name + "_" + suffix

    return name
