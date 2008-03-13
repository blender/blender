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

FILE(GLOB SRC intern/*.cpp)

SET(INC 
  . intern ../blenkernel ../blenlib ../makesdna ../include
  ${FTGL_INC}
  ${FREETYPE_INC}
  ${GETTEXT_INC}
)

ADD_DEFINITIONS(-DFTGL_LIBRARY_STATIC)

IF(WIN32)
  ADD_DEFINITIONS(-D_WIN32 -DUSE_GETTEXT_DLL)
ENDIF(WIN32)

BLENDERLIB(bf_ftfont "${SRC}" "${INC}")
#env.BlenderLib ( 'bf_ftfont', sources, Split(incs), Split(defs), libtype=['international','player'], priority=[0, 205] )
