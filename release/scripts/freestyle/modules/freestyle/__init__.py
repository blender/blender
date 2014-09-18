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

"""
This module provides data types of view map components (0D and 1D
elements), base classes for defining line stylization rules
(predicates, functions, chaining iterators, and stroke shaders), as
well as helper functions for style module writing.

Submodules:

* :mod:`freestyle.types`
* :mod:`freestyle.predicates`
* :mod:`freestyle.functions`
* :mod:`freestyle.chainingiterators`
* :mod:`freestyle.shaders`
* :mod:`freestyle.utils`
"""


# module members
from . import chainingiterators, functions, predicates, shaders, types, utils
