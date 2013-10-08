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
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	../include
	../../blenfont
	../../blenkernel
	../../blenlib
	../../gpu
	../../imbuf
	../../makesdna
	../../makesrna
	../../python
	../../windowmanager
	../../../../intern/guardedalloc
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
)

set(SRC
	interface.c
	interface_anim.c
	interface_draw.c
	interface_eyedropper.c
	interface_handlers.c
	interface_icons.c
	interface_layout.c
	interface_ops.c
	interface_panel.c
	interface_regions.c
	interface_style.c
	interface_templates.c
	interface_utils.c
	interface_widgets.c
	resources.c
	view2d.c
	view2d_ops.c

	interface_intern.h
)

if(WITH_INTERNATIONAL)
	add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_HEADLESS)
	add_definitions(-DWITH_HEADLESS)
endif()

if(WITH_PYTHON)
	add_definitions(-DWITH_PYTHON)
endif()

blender_add_lib(bf_editor_interface "${SRC}" "${INC}" "${INC_SYS}")
