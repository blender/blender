# <pep8-80 compliant>

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


NOT_SUPPORTED = False

if "bpy" in locals():
    import importlib
    if compat.check_version(2, 79, 0) == 0:
        importlib.reload(v279)
    elif compat.check_version(2, 80, 0) == 0:
        importlib.reload(v280)
    elif compat.check_version(2, 81, 0) == 0:
        importlib.reload(v281)
    elif compat.check_version(2, 82, 0) == 0:
        importlib.reload(v282)
    elif compat.check_version(2, 83, 0) == 0:
        importlib.reload(v283)
    elif compat.check_version(2, 90, 0) == 0:
        importlib.reload(v290)
    else:
        NOT_SUPPORTED = True
else:
    import bpy
    from .. import compatibility as compat
    if compat.check_version(2, 79, 0) == 0:
        from .v279 import *
    elif compat.check_version(2, 80, 0) == 0:
        from .v280 import *
    elif compat.check_version(2, 81, 0) == 0:
        from .v281 import *
    elif compat.check_version(2, 82, 0) == 0:
        from .v282 import *
    elif compat.check_version(2, 83, 0) == 0:
        from .v283 import *
    elif compat.check_version(2, 90, 0) == 0:
        from .v290 import *
    else:
        NOT_SUPPORTED = True
