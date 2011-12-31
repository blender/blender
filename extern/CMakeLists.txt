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

# Otherwise we get warnings here that we cant fix in external projects
remove_strict_flags()

add_subdirectory(colamd)

if(WITH_BULLET)
	add_subdirectory(bullet2)
endif()

if(WITH_MOD_CLOTH_ELTOPO)
	add_subdirectory(eltopo)
endif()

if(WITH_BINRELOC)
	add_subdirectory(binreloc)
endif()

if(WITH_BUILTIN_GLEW)
	add_subdirectory(glew)
endif()

if(WITH_GAMEENGINE)
	add_subdirectory(recastnavigation)
endif()

if(WITH_IMAGE_OPENJPEG AND (NOT UNIX OR APPLE))
	add_subdirectory(libopenjpeg)
endif()

if(WITH_IMAGE_REDCODE)
	add_subdirectory(libredcode)
endif()

if(WITH_LZO)
	add_subdirectory(lzo)
endif()

if(WITH_LZMA)
	add_subdirectory(lzma)
endif()

if(WITH_LIBMV)
	add_subdirectory(libmv)
endif()
