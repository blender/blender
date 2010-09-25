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
	../include
	../../blenfont
	../../blenkernel
	../../blenlib
	../../blenloader
	../../gpu
	../../imbuf
	../../makesdna
	../../makesrna
	../../render/extern/include
	../../windowmanager
	../../../../intern/guardedalloc
	../../../../extern/glew/include
	../../freestyle
)

IF(WITH_QUICKTIME)
	LIST(APPEND INC ../../quicktime ${QUICKTIME_INC})
	ADD_DEFINITIONS(-DWITH_QUICKTIME)
ENDIF(WITH_QUICKTIME)

IF(WITH_OPENMP)
	ADD_DEFINITIONS(-DPARALLEL=1)
ENDIF(WITH_OPENMP)

IF(WIN32)
	LIST(APPEND INC ${PTHREADS_INC})
ENDIF(WIN32)

BLENDERLIB(bf_editor_render "${SRC}" "${INC}")
