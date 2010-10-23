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

ADD_SUBDIRECTORY(audaspace)
ADD_SUBDIRECTORY(string)
ADD_SUBDIRECTORY(ghost)
ADD_SUBDIRECTORY(guardedalloc)
ADD_SUBDIRECTORY(moto)
ADD_SUBDIRECTORY(memutil)
ADD_SUBDIRECTORY(iksolver)
ADD_SUBDIRECTORY(opennl)
ADD_SUBDIRECTORY(smoke)

IF(WITH_MOD_FLUID)
	ADD_SUBDIRECTORY(elbeem)
ENDIF(WITH_MOD_FLUID)

IF(WITH_MOD_DECIMATE)
	ADD_SUBDIRECTORY(container)
	ADD_SUBDIRECTORY(decimation)
ENDIF(WITH_MOD_DECIMATE)

IF(WITH_MOD_BOOLEAN)
	ADD_SUBDIRECTORY(boolop)
	ADD_SUBDIRECTORY(bsp)
ENDIF(WITH_MOD_BOOLEAN)

IF(WITH_IK_ITASC)
	ADD_SUBDIRECTORY(itasc)
ENDIF(WITH_IK_ITASC)
