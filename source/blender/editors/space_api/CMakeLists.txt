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
  ../io
  ../../blenkernel
  ../../blenlib
  ../../gpu
  ../../makesdna
  ../../makesrna
  ../../windowmanager
  ../../../../intern/guardedalloc
)

set(INC_SYS

)

set(SRC
  spacetypes.c
)

set(LIB
  bf_editor_geometry
  bf_editor_space_action
  bf_editor_space_buttons
  bf_editor_space_clip
  bf_editor_space_console
  bf_editor_space_file
  bf_editor_space_graph
  bf_editor_space_image
  bf_editor_space_info
  bf_editor_space_nla
  bf_editor_space_node
  bf_editor_space_outliner
  bf_editor_space_script
  bf_editor_space_sequencer
  bf_editor_space_spreadsheet
  bf_editor_space_statusbar
  bf_editor_space_text
  bf_editor_space_topbar
  bf_editor_space_userpref
  bf_editor_space_view3d
)

blender_add_lib(bf_editor_space_api "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
