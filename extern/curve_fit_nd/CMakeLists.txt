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
	.
)

set(INC_SYS

)

set(SRC
	intern/curve_fit_cubic.c
	intern/curve_fit_cubic_refit.c
	intern/curve_fit_corners_detect.c

	curve_fit_nd.h
	intern/curve_fit_inline.h
	intern/generic_alloc_impl.h
	intern/generic_heap.c
	intern/generic_heap.h
)

blender_add_lib(extern_curve_fit_nd "${SRC}" "${INC}" "${INC_SYS}")
