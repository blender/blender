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

add_subdirectory(editors)
add_subdirectory(windowmanager)
add_subdirectory(blenkernel)
add_subdirectory(blenlib)
add_subdirectory(render)
add_subdirectory(blenfont)
add_subdirectory(blenloader)
add_subdirectory(readblenfile)
add_subdirectory(blenpluginapi)
add_subdirectory(ikplugin)
add_subdirectory(gpu)
add_subdirectory(imbuf)
add_subdirectory(avi)
add_subdirectory(nodes)
add_subdirectory(modifiers)
add_subdirectory(makesdna)
add_subdirectory(makesrna)
ADD_SUBDIRECTORY(freestyle)

if(WITH_IMAGE_OPENEXR)
	add_subdirectory(imbuf/intern/openexr)
endif()

if(WITH_IMAGE_DDS)
	add_subdirectory(imbuf/intern/dds)
endif()

if(WITH_IMAGE_CINEON)
	add_subdirectory(imbuf/intern/cineon)
endif()

if(WITH_CODEC_QUICKTIME)
	add_subdirectory(quicktime)
endif()

if(WITH_PYTHON)
	add_subdirectory(python)
endif()

if(WITH_OPENCOLLADA)
  add_subdirectory(collada)
endif()

