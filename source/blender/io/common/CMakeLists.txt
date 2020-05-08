# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2020, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

set(INC
  .
  ../../blenkernel
  ../../blenlib
  ../../depsgraph
  ../../makesdna
)

set(INC_SYS
)

set(SRC
  intern/abstract_hierarchy_iterator.cc

  IO_abstract_hierarchy_iterator.h
)

set(LIB
  bf_blenkernel
  bf_blenlib
)

blender_add_lib(bf_io_common "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

target_link_libraries(bf_io_common INTERFACE)
