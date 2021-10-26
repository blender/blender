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
  ../include
  ../../blenfont
  ../../blenkernel
  ../../blenlib
  ../../blentranslation
  ../../bmesh
  ../../depsgraph
  ../../functions
  ../../gpu
  ../../makesdna
  ../../makesrna
  ../../nodes
  ../../windowmanager
  ../../../../intern/glew-mx
  ../../../../intern/guardedalloc
)

set(SRC
  space_spreadsheet.cc
  spreadsheet_cache.cc
  spreadsheet_column.cc
  spreadsheet_context.cc
  spreadsheet_data_source.cc
  spreadsheet_data_source_geometry.cc
  spreadsheet_dataset_draw.cc
  spreadsheet_dataset_layout.cc
  spreadsheet_draw.cc
  spreadsheet_layout.cc
  spreadsheet_ops.cc
  spreadsheet_row_filter.cc
  spreadsheet_row_filter_ui.cc

  spreadsheet_cache.hh
  spreadsheet_cell_value.hh
  spreadsheet_column.hh
  spreadsheet_column_values.hh
  spreadsheet_context.hh
  spreadsheet_data_source.hh
  spreadsheet_data_source_geometry.hh
  spreadsheet_dataset_draw.hh
  spreadsheet_dataset_layout.hh
  spreadsheet_draw.hh
  spreadsheet_intern.hh
  spreadsheet_layout.hh
  spreadsheet_row_filter.hh
  spreadsheet_row_filter_ui.hh
)

set(LIB
)

blender_add_lib(bf_editor_space_spreadsheet "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
