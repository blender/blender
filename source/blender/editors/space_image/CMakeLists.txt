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

FILE(GLOB SRC *.c)

SET(INC
	../../blenkernel
	../../blenlib
	../../imbuf
	../include
	../../../../intern/guardedalloc
	../../makesdna
	../../makesrna
	../../render/extern/include
	../../windowmanager
)

IF(WITH_OPENEXR)
	ADD_DEFINITIONS(-DWITH_OPENEXR)
ENDIF(WITH_OPENEXR)

IF(WITH_TIFF)
	ADD_DEFINITIONS(-DWITH_TIFF)
ENDIF(WITH_TIFF)

IF(WIN32)
	SET(INC ${INC} ${PTHREADS_INC})
ENDIF(WIN32)

BLENDERLIB(bf_editor_space_image "${SRC}" "${INC}")
