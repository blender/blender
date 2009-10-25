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

FILE(GLOB SRC *.cpp)

SET(INC
	.
	../../../source/kernel/gen_system
	../../../source/blender/makesdna
	../../../source/gameengine/SceneGraph
	../../../intern/string
	../../../intern/moto/include
	../../../extern/glew/include
	../Expressions
	${PYTHON_INC}
)

BLENDERLIB(bf_rasterizer "${SRC}" "${INC}")
#env.BlenderLib ( 'bf_rasterizer', sources, Split(incs), [], libtype=['game','player'], priority=[35,115], compileflags = cflags )
