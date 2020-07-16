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
# ***** END GPL LICENSE BLOCK *****

set(INC
  include
)

set(INC_SYS

)

set(SRC
  source/sky_model.cpp
  source/sky_nishita.cpp

  include/sky_model.h
  source/sky_float3.h
  source/sky_model_data.h
)

set(LIB
)

blender_add_lib(bf_intern_sky "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
