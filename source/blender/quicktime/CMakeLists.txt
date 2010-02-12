# $Id$
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
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

IF(USE_QTKIT)
	SET(SRC apple/qtkit_import.m apple/qtkit_export.m)
ELSE(USE_QTKIT)
	SET(SRC apple/quicktime_import.c apple/quicktime_export.c)
ENDIF(USE_QTKIT)

SET(INC 
	.
	../quicktime
	../makesdna
	../makesrna
	../../../intern/guardedalloc
	../blenlib
	../blenkernel
	../avi
	../imbuf
	../imbuf/intern
	../blenloader
	../render/extern/include
	../include
	../windowmanager
)

SET(INC ${INC} ${QUICKTIME_INC})
ADD_DEFINITIONS(-DWITH_QUICKTIME)

BLENDERLIB(bf_quicktime "${SRC}" "${INC}")
#if env['OURPLATFORM'] in ['win32-vc', 'darwin']:
#    types = ['core', 'player']
#    priorities = [95, 200]
#else:
#    types = ['core']
#    priorities = [95]
#
#env.BlenderLib ('bf_quicktime', sources=source_files, includes=incs, defines=['WITH_QUICKTIME'], libtype=types, priority=priorities)
