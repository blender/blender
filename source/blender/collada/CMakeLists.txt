# $Id: CMakeLists.txt 21789 2009-07-22 05:35:12Z kazanbas $
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

FILE(GLOB SRC *.cpp)

IF(APPLE)
SET(INC
  .
  ../blenlib
  ../blenkernel
  ../windowmanager
  ../makesdna
  ../makesrna
  ../editors/include
  ../../../intern/guardedalloc
  ${OPENCOLLADA_INC}/COLLADAStreamWriter
  ${OPENCOLLADA_INC}/COLLADABaseUtils
  ${OPENCOLLADA_INC}/COLLADAFramework
  ${OPENCOLLADA_INC}/COLLADASaxFrameworkLoader 
)

ELSE(APPLE)

SET(INC
  .
  ../blenlib
  ../blenkernel
  ../windowmanager
  ../makesdna
  ../makesrna
  ../editors/include
  ../../../intern/guardedalloc
  ${OPENCOLLADA_INC}/COLLADAStreamWriter/include
  ${OPENCOLLADA_INC}/COLLADABaseUtils/include
  ${OPENCOLLADA_INC}/COLLADAFramework/include
  ${OPENCOLLADA_INC}/COLLADASaxFrameworkLoader/include 
)

ENDIF(APPLE)

BLENDERLIB(bf_collada "${SRC}" "${INC}")
