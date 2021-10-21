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
# ***** END GPL LICENSE BLOCK *****

set(INC
  .
  ../include
  ../../blenkernel
  ../../blenlib
  ../../blenloader
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/guardedalloc
)

set(INC_SYS
)

set(SRC
  intern/asset_catalog.cc
  intern/asset_filter.cc
  intern/asset_handle.cc
  intern/asset_library_reference.cc
  intern/asset_library_reference_enum.cc
  intern/asset_list.cc
  intern/asset_mark_clear.cc
  intern/asset_ops.cc
  intern/asset_temp_id_consumer.cc
  intern/asset_type.cc

  ED_asset_catalog.h
  ED_asset_catalog.hh
  ED_asset_filter.h
  ED_asset_handle.h
  ED_asset_library.h
  ED_asset_list.h
  ED_asset_list.hh
  ED_asset_mark_clear.h
  ED_asset_temp_id_consumer.h
  ED_asset_type.h
  intern/asset_library_reference.hh
)

set(LIB
  bf_blenkernel
  bf_blenloader
)

blender_add_lib(bf_editor_asset "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
