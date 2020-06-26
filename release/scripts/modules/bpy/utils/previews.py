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

This allows scripts to generate their own previews, and use them as icons in UI widgets
('icon_value' for UILayout functions).


Custom Icon Example
-------------------

.. literalinclude:: __/__/__/release/scripts/templates_py/ui_previews_custom_icon.py
"""

__all__ = (
    "new",
    "remove",
    "ImagePreviewCollection",
)

import _bpy
_utils_previews = _bpy._utils_previews
del _bpy


_uuid_open = set()


# High-level previews manager.
# not accessed directly
class ImagePreviewCollection(dict):
    """
    Dictionary-like class of previews.

    This is a subclass of Python's built-in dict type,
    used to store multiple image previews.

    .. note::

        - instance with :mod:`bpy.utils.previews.new`
        - keys must be ``str`` type.
        - values will be :class:`bpy.types.ImagePreview`
    """

    # Internal notes:
    # - Blender's internal 'PreviewImage' struct uses 'self._uuid' prefix.

    def __init__(self):
        super().__init__()
        self._uuid = hex(id(self))
        _uuid_open.add(self._uuid)

    def __del__(self):
        if self._uuid not in _uuid_open:
            return

        raise ResourceWarning(
            "%r: left open, remove with 'bpy.utils.previews.remove()'" % self
        )
        self.close()

    def _gen_key(self, name):
        return ":".join((self._uuid, name))

    def new(self, name):
        if name in self:
            raise KeyError(f"key {name!r} already exists")
        p = self[name] = _utils_previews.new(
            self._gen_key(name))
        return p
    new.__doc__ = _utils_previews.new.__doc__

    def load(self, name, path, path_type, force_reload=False):
        if name in self:
            raise KeyError(f"key {name!r} already exists")
        p = self[name] = _utils_previews.load(
            self._gen_key(name), path, path_type, force_reload)
        return p
    load.__doc__ = _utils_previews.load.__doc__

    def clear(self):
        """Clear all previews."""
        for name in self.keys():
            _utils_previews.release(self._gen_key(name))
        super().clear()

    def close(self):
        """Close the collection and clear all previews."""
        self.clear()
        _uuid_open.remove(self._uuid)

    def __delitem__(self, key):
        _utils_previews.release(self._gen_key(key))
        super().__delitem__(key)

    def __repr__(self):
        return "<%s id=%s[%d], %r>" % (
            self.__class__.__name__, self._uuid, len(self), super()
        )


def new():
    """
    :return: a new preview collection.
    :rtype: :class:`ImagePreviewCollection`
    """

    return ImagePreviewCollection()


def remove(pcoll):
    """
    Remove the specified previews collection.

    :arg pcoll: Preview collection to close.
    :type pcoll: :class:`ImagePreviewCollection`
    """
    pcoll.close()


# don't complain about resources on exit (only unregister)
import atexit


def exit_clear_warning():
    del ImagePreviewCollection.__del__


atexit.register(exit_clear_warning)
del atexit, exit_clear_warning
