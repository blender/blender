# $Id$
# ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version. The Blender
# Foundation also sells licenses for use in proprietary software under
# the Blender License.  See http://www.blender.org/BL/ for information
# about this.
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
# ***** END GPL/BL DUAL LICENSE BLOCK *****

SETUP_LIBDIRS()

FILE(APPEND ../source/blender/makesdna/intern/dna.c "")

IF(WIN32)
  ADD_EXECUTABLE(blender ${EXETYPE} ../source/blender/makesdna/intern/dna.c ../source/icons/winblender.rc)
ELSE(WIN32)
  ADD_EXECUTABLE(blender ${EXETYPE} ../source/blender/makesdna/intern/dna.c)
ENDIF(WIN32)

ADD_DEPENDENCIES(blender makesdna)

SETUP_LIBLINKS(blender)
FILE(READ ${CMAKE_SOURCE_DIR}/cmake_blender_libs.txt BLENDER_LINK_LIBS)
TARGET_LINK_LIBRARIES(blender ${BLENDER_LINK_LIBS} src blender_creator blender_python blender_render blender_radiosity blender_IK bf_elbeem)

MESSAGE("Setting up blender")
