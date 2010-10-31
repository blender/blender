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
	../../makesdna
	../../makesrna
	../../windowmanager
	../../../../intern/guardedalloc
)

SET(SRC
	space_text.c
	text_draw.c
	text_header.c
	text_ops.c
	text_python.c
)

IF(WITH_PYTHON)
	LIST(APPEND INC ${PYTHON_INC} ../../python)
	ADD_DEFINITIONS(-DWITH_PYTHON)
ENDIF(WITH_PYTHON)

BLENDERLIB(bf_editor_text "${SRC}" "${INC}")
