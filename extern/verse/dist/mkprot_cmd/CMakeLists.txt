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

SET(SRC
  ../v_cmd_gen.c
  ../v_cmd_def_a.c
  ../v_cmd_def_b.c
  ../v_cmd_def_c.c
  ../v_cmd_def_g.c
  ../v_cmd_def_m.c
  ../v_cmd_def_o.c
  ../v_cmd_def_s.c
  ../v_cmd_def_t.c
)

ADD_DEFINITIONS(-DV_GENERATE_FUNC_MODE)
ADD_EXECUTABLE(mkprot ${SRC})

# Uncoment the following to get verse to generate the files using dependency
# tracking without having the generated files submitted in CVS.
#
#ADD_CUSTOM_COMMAND(TARGET mkprot
#  POST_BUILD
#  COMMAND mkprot -src=${CMAKE_CURRENT_SOURCE_DIR}/../ -dst=${CMAKE_CURRENT_SOURCE_DIR}/../
#  MAIN_DEPENDENCY ${EXECUTABLE_OUTPUT_PATH}/${CMAKE_CFG_INTDIR}/mkprot
#  WORKING_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}/${CMAKE_CFG_INTDIR}
#)
MESSAGE(STATUS "Configuring mkprot(verse)")

