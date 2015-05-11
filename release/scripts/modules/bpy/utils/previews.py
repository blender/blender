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

# <pep8 compliant>

"""
This module contains utility functions to handle custom previews.

It behaves as a high-level 'cached' previews manager.

This allows addons to generate their own previews, and use them as icons in UI widgets
('icon_value' of UILayout functions).
"""

__all__ = (
    "new",
    "remove",
    )

import _bpy
_utils_previews = _bpy._utils_previews
del _bpy


_uuid_open = set()


# High-level previews manager.
# not accessed directly
class _BPyImagePreviewCollection(dict):
    """
    Dict-like class of previews.
    """

    # Internal notes:
    # - keys in the dict are stored by name
    # - values are instances of bpy.types.ImagePreview
    # - Blender's internal 'PreviewImage' struct uses 'self._uuid' prefix.

    def __init__(self):
        super().__init__()
        self._uuid = hex(id(self))
        _uuid_open.add(self._uuid)

    def __del__(self):
        if self._uuid not in _uuid_open:
            return

        raise ResourceWarning(
                "<%s id=%s[%d]>: left open, remove with "
                "'bpy.utils.previews.remove()'" %
                (self.__class__.__name__, self._uuid, len(self)))
        self.close()

    def _gen_key(self, name):
        return ":".join((self._uuid, name))

    def new(self, name):
        if name in self:
            raise KeyException("key %r already exists")
        p = self[name] = _utils_previews.new(
                self._gen_key(name))
        return p
    new.__doc__ = _utils_previews.new.__doc__

    def load(self, name, path, path_type, force_reload=False):
        if name in self:
            raise KeyException("key %r already exists")
        p = self[name] = _utils_previews.load(
                self._gen_key(name), path, path_type, force_reload)
        return p
    load.__doc__ = _utils_previews.load.__doc__

    def release(self, name):
        p = self.pop(name, None)
        if p is not None:
            _utils_previews.release(self._gen_key(name))
    release.__doc__ = _utils_previews.release.__doc__

    def clear(self):
        for name in self.keys():
            _utils_previews.release(self._gen_key(name))
        super().clear()

    def close(self):
        self.clear()
        _uuid_open.remove(self._uuid)

    def __delitem__(self, key):
        return self.release(key)

    def __repr__(self):
        return "<%s id=%s[%d], %s>" % (
                self.__class__.__name__,
                self._uuid,
                len(self),
                super().__repr__())


def new():
    """
    Return a new preview collection.
    """

    return _BPyImagePreviewCollection()


def remove(p):
    """
    Remove the specified previews collection.
    """
    p.close()


# don't complain about resources on exit (only unregister)
import atexit

def exit_clear_warning():
    del _BPyImagePreviewCollection.__del__

atexit.register(exit_clear_warning)
del atexit, exit_clear_warning
