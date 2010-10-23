# $Id: CMakeLists.txt 12931 2007-12-17 18:20:48Z theeth $
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

SET(INC
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

SET(SRC
	interface.c
	interface_anim.c
	interface_draw.c
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
)

IF(WITH_INTERNATIONAL)
	ADD_DEFINITIONS(-DINTERNATIONAL)
ENDIF(WITH_INTERNATIONAL)

IF(NOT WITH_PYTHON)
	ADD_DEFINITIONS(-DDISABLE_PYTHON)
ENDIF(NOT WITH_PYTHON)

BLENDERLIB(bf_editor_interface "${SRC}" "${INC}")
