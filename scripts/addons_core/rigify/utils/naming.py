# SPDX-FileCopyrightText: 2019-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import random
import time
import re
import collections
import enum

from typing import Optional, TYPE_CHECKING


if TYPE_CHECKING:
    from ..base_generate import BaseGenerator


ORG_PREFIX = "ORG-"  # Prefix of original bones.
MCH_PREFIX = "MCH-"  # Prefix of mechanism bones.
DEF_PREFIX = "DEF-"  # Prefix of deformation bones.
ROOT_NAME = "root"   # Name of the root bone.

_PREFIX_TABLE = {'org': "ORG", 'mch': "MCH", 'def': "DEF", 'ctrl': ''}

########################################################################
# Name structure
########################################################################

NameParts = collections.namedtuple('NameParts', ['prefix', 'base', 'side_z', 'side', 'number'])


def split_name(name: str):
    name_parts = re.match(
        r'^(?:(ORG|MCH|DEF)-)?(.*?)([._-][tTbB])?([._-][lLrR])?(?:\.(\d+))?$', name)
    return NameParts(*name_parts.groups())


def is_control_bone(name: str):
    return not split_name(name).prefix


def combine_name(parts: NameParts, *, prefix=None, base=None, side_z=None, side=None, number=None):
    eff_prefix = prefix if prefix is not None else parts.prefix
    eff_number = number if number is not None else parts.number
    if isinstance(eff_number, int):
        eff_number = '%03d' % eff_number

    return ''.join([
        eff_prefix+'-' if eff_prefix else '',
        base if base is not None else parts.base,
        side_z if side_z is not None else parts.side_z or '',
        side if side is not None else parts.side or '',
        '.'+eff_number if eff_number else '',
    ])


def insert_before_lr(name: str, text: str) -> str:
    parts = split_name(name)

    if parts.side:
        return combine_name(parts, base=parts.base + text)
    else:
        return name + text


def make_derived_name(name: str, subtype: str, suffix: Optional[str] = None):
    """ Replaces the name prefix, and optionally adds the suffix (before .LR if found).
    """
    assert(subtype in _PREFIX_TABLE)

    parts = split_name(name)
    new_base = parts.base + (suffix or '')

    return combine_name(parts, prefix=_PREFIX_TABLE[subtype], base=new_base)


########################################################################
# Name mirroring
########################################################################

class Side(enum.IntEnum):
    LEFT = -1
    MIDDLE = 0
    RIGHT = 1

    @staticmethod
    def from_parts(parts: NameParts):
        if parts.side:
            if parts.side[1].lower() == 'l':
                return Side.LEFT
            else:
                return Side.RIGHT
        else:
            return Side.MIDDLE

    @staticmethod
    def to_string(parts: NameParts, side: 'Side'):
        if side != Side.MIDDLE:
            side_char = 'L' if side == Side.LEFT else 'R'
            side_str = parts.side or parts.side_z

            if side_str:
                sep, side_char2 = side_str[0:2]
                if side_char2.lower() == side_char2:
                    side_char = side_char.lower()
            else:
                sep = '.'

            return sep + side_char
        else:
            return ''

    @staticmethod
    def to_name(parts: NameParts, side: 'Side'):
        new_side = Side.to_string(parts, side)
        return combine_name(parts, side=new_side)


class SideZ(enum.IntEnum):
    TOP = 2
    MIDDLE = 0
    BOTTOM = -2

    @staticmethod
    def from_parts(parts: NameParts):
        if parts.side_z:
            if parts.side_z[1].lower() == 't':
                return SideZ.TOP
            else:
                return SideZ.BOTTOM
        else:
            return SideZ.MIDDLE

    @staticmethod
    def to_string(parts: NameParts, side: 'SideZ'):
        if side != SideZ.MIDDLE:
            side_char = 'T' if side == SideZ.TOP else 'B'
            side_str = parts.side_z or parts.side

            if side_str:
                sep, side_char2 = side_str[0:2]
                if side_char2.lower() == side_char2:
                    side_char = side_char.lower()
            else:
                sep = '.'

            return sep + side_char
        else:
            return ''

    @staticmethod
    def to_name(parts: NameParts, side: 'SideZ'):
        new_side = SideZ.to_string(parts, side)
        return combine_name(parts, side_z=new_side)


NameSides = collections.namedtuple('NameSides', ['base', 'side', 'side_z'])


def get_name_side(name: str):
    return Side.from_parts(split_name(name))


def get_name_side_z(name: str):
    return SideZ.from_parts(split_name(name))


def get_name_base_and_sides(name: str):
    parts = split_name(name)
    base = combine_name(parts, side='', side_z='')
    return NameSides(base, Side.from_parts(parts),  SideZ.from_parts(parts))


def change_name_side(name: str,
                     side: Optional[Side] = None, *,
                     side_z: Optional[SideZ] = None):
    parts = split_name(name)
    new_side = None if side is None else Side.to_string(parts, side)
    new_side_z = None if side_z is None else SideZ.to_string(parts, side_z)
    return combine_name(parts, side=new_side, side_z=new_side_z)


def mirror_name(name: str):
    parts = split_name(name)
    side = Side.from_parts(parts)

    if side != Side.MIDDLE:
        return Side.to_name(parts, -side)
    else:
        return name


def mirror_name_z(name: str):
    parts = split_name(name)
    side = SideZ.from_parts(parts)

    if side != SideZ.MIDDLE:
        return SideZ.to_name(parts, -side)
    else:
        return name


########################################################################
# Name manipulation
########################################################################

def get_name(bone) -> Optional[str]:
    return bone.name if bone else None


def strip_trailing_number(name: str):
    return combine_name(split_name(name), number='')


def strip_prefix(name: str):
    return combine_name(split_name(name), prefix='')


def unique_name(collection, base_name: str):
    parts = split_name(base_name)
    name = combine_name(parts, number='')
    count = 1

    while name in collection:
        name = combine_name(parts, number=count)
        count += 1

    return name


def strip_org(name: str):
    """ Returns the name with ORG_PREFIX stripped from it.
    """
    if name.startswith(ORG_PREFIX):
        return name[len(ORG_PREFIX):]
    else:
        return name


org_name = strip_org


def strip_mch(name: str):
    """ Returns the name with MCH_PREFIX stripped from it.
        """
    if name.startswith(MCH_PREFIX):
        return name[len(MCH_PREFIX):]
    else:
        return name


def strip_def(name: str):
    """ Returns the name with DEF_PREFIX stripped from it.
        """
    if name.startswith(DEF_PREFIX):
        return name[len(DEF_PREFIX):]
    else:
        return name


def org(name: str):
    """ Prepends the ORG_PREFIX to a name if it doesn't already have
        it, and returns it.
    """
    if name.startswith(ORG_PREFIX):
        return name
    else:
        return ORG_PREFIX + name


make_original_name = org


def mch(name: str):
    """ Prepends the MCH_PREFIX to a name if it doesn't already have
        it, and returns it.
    """
    if name.startswith(MCH_PREFIX):
        return name
    else:
        return MCH_PREFIX + name


make_mechanism_name = mch


def deformer(name: str):
    """ Prepends the DEF_PREFIX to a name if it doesn't already have
        it, and returns it.
    """
    if name.startswith(DEF_PREFIX):
        return name
    else:
        return DEF_PREFIX + name


make_deformer_name = deformer


def random_id(length=8):
    """ Generates a random alphanumeric id string.
    """
    t_length = int(length / 2)
    r_length = int(length / 2) + int(length % 2)

    chars = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 'a', 'b', 'c', 'd', 'e', 'f',
             'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
             'x', 'y', 'z']
    text = ""
    for i in range(0, r_length):
        text += random.choice(chars)
    text += str(hex(int(time.time())))[2:][-t_length:].rjust(t_length, '0')[::-1]
    return text


def choose_derived_bone(generator: 'BaseGenerator', original: str, subtype: str, *,
                        by_owner=True, recursive=True):
    bones = generator.obj.pose.bones
    names = generator.find_derived_bones(original, by_owner=by_owner, recursive=recursive)

    direct = make_derived_name(original, subtype)
    if direct in names and direct in bones:
        return direct

    prefix = _PREFIX_TABLE[subtype] + '-'
    matching = [name for name in names if name.startswith(prefix) and name in bones]

    if len(matching) > 0:
        return matching[0]

    # Try matching bones created by legacy rigs just by name - there is no origin data
    from ..base_generate import LegacyRig

    if isinstance(generator.bone_owners.get(direct), LegacyRig):
        if not by_owner or generator.bone_owners.get(original) is generator.bone_owners[direct]:
            assert direct in bones
            return direct

    return None


_MIRROR_MAP_RAW = [
    ("Left", "Right"),
    ("L", "R"),
]
_MIRROR_MAP = {
    **{a: b for a, b in _MIRROR_MAP_RAW},
    **{b: a for a, b in _MIRROR_MAP_RAW},
    **{a.lower(): b.lower() for a, b in _MIRROR_MAP_RAW},
    **{b.lower(): a.lower() for a, b in _MIRROR_MAP_RAW},
}
_MIRROR_RE = [
    r"(?<![a-z])(left|light)(?![a-z])",
    r"(?<=[\._])(l|r)(?![a-z])",
]


def mirror_name_fuzzy(name: str) -> str:
    """Try to mirror a name by trying various patterns without expecting any rigid structure."""

    for reg in _MIRROR_RE:
        new_name = re.sub(reg, lambda m: _MIRROR_MAP.get(m[0], m[0]), name, flags=re.IGNORECASE)
        if new_name != name:
            return new_name

    return name
