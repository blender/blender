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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

"""
This module contains application values that remain unchanged during runtime.

.. data:: version

   The Blender version as a tuple of 3 numbers. eg. (2, 50, 11)


.. data:: version_string

   The Blender version formatted as a string.

.. data:: home

   The blender home directory, normally matching $HOME

.. data:: binary_path

   The location of blenders executable, useful for utilities that spawn new instances.

"""
# constants
import _bpy
version = _bpy._VERSION
version_string = _bpy._VERSION_STR
home = _bpy._HOME
binary_path = _bpy._BINPATH
