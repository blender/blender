# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This module contains RestrictBlend context manager.
"""

__all__ = (
    "RestrictBlend",
)

import bpy as _bpy


class _RestrictContext:
    __slots__ = ()
    _real_data = _bpy.data
    # safe, the pointer never changes
    _real_pref = _bpy.context.preferences

    @property
    def window_manager(self):
        return self._real_data.window_managers[0]

    @property
    def preferences(self):
        return self._real_pref


class _RestrictData:
    __slots__ = ()


_context_restrict = _RestrictContext()
_data_restrict = _RestrictData()


class RestrictBlend:
    __slots__ = ("context", "data")

    def __enter__(self):
        self.data = _bpy.data
        self.context = _bpy.context
        _bpy.data = _data_restrict
        _bpy.context = _context_restrict

    def __exit__(self, _type, _value, _traceback):
        _bpy.data = self.data
        _bpy.context = self.context
