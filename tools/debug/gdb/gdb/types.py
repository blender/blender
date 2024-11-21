# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from gdb import Type, Field


def get_basic_type(type: Type) -> Type:
    pass


def has_field(type: Type) -> bool:
    pass


def make_enum_dict(enum_type: Type) -> dict[str, int]:
    pass
