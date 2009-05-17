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

ADD_SUBDIRECTORY(avi)
ADD_SUBDIRECTORY(nodes)
ADD_SUBDIRECTORY(blenkernel)
ADD_SUBDIRECTORY(blenlib)
ADD_SUBDIRECTORY(blenloader)
ADD_SUBDIRECTORY(blenpluginapi)
ADD_SUBDIRECTORY(imbuf)
ADD_SUBDIRECTORY(imbuf/intern/cineon)
ADD_SUBDIRECTORY(gpu)
ADD_SUBDIRECTORY(makesdna)
ADD_SUBDIRECTORY(python)
ADD_SUBDIRECTORY(radiosity)
ADD_SUBDIRECTORY(readblenfile)
ADD_SUBDIRECTORY(render)
ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(yafray)

IF(WITH_INTERNATIONAL)
  ADD_SUBDIRECTORY(ftfont)
ENDIF(WITH_INTERNATIONAL)

IF(WITH_OPENEXR)
  ADD_SUBDIRECTORY(imbuf/intern/openexr)
ENDIF(WITH_OPENEXR)

IF(WITH_DDS)
  ADD_SUBDIRECTORY(imbuf/intern/dds)
ENDIF(WITH_DDS)

IF(WITH_QUICKTIME)
  ADD_SUBDIRECTORY(quicktime)
ENDIF(WITH_QUICKTIME)
