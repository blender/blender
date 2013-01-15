# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8-80 compliant>

"""
This module contains RestrictBlend context manager.
"""

__all__ = (
    "RestrictBlend",
    )

import bpy as _bpy


class _RestrictContext():
    __slots__ = ()
    _real_data = _bpy.data
    # safe, the pointer never changes
    _real_pref = _bpy.context.user_preferences

    @property
    def window_manager(self):
        return self._real_data.window_managers[0]

    @property
    def user_preferences(self):
        return self._real_pref


class _RestrictData():
    __slots__ = ()


_context_restrict = _RestrictContext()
_data_restrict = _RestrictData()


class RestrictBlend():
    __slots__ = ("context", "data")

    def __enter__(self):
        self.data = _bpy.data
        self.context = _bpy.context
        _bpy.data = _data_restrict
        _bpy.context = _context_restrict

    def __exit__(self, type, value, traceback):
        _bpy.data = self.data
        _bpy.context = self.context
