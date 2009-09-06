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
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

SET(SRC apple/quicktime_import.c apple/quicktime_export.c)

SET(INC 
	.
	../quicktime
	../makesdna
	../../../intern/guardedalloc
	../blenlib
	../blenkernel
	../avi
	../imbuf
	../imbuf/intern
	../blenloader
	../render/extern/include
	../include
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
