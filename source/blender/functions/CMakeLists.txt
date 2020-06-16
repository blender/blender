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
  ../blenlib
  ../makesdna
  ../../../intern/guardedalloc
)

set(INC_SYS
)

set(SRC
  intern/cpp_types.cc

  FN_array_spans.hh
  FN_cpp_type.hh
  FN_cpp_types.hh
  FN_multi_function.hh
  FN_multi_function_context.hh
  FN_multi_function_data_type.hh
  FN_multi_function_param_type.hh
  FN_multi_function_params.hh
  FN_multi_function_signature.hh
  FN_spans.hh
)

set(LIB
  bf_blenlib
)

blender_add_lib(bf_functions "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
