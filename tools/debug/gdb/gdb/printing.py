# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from .gdb import Value


class PrettyPrinter:
    def __init__(self, name: str, subprinters=None):
        pass

    def __call__(self, value: Value):
        pass
