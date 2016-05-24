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
# The Original Code is Copyright (C) 2014, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Joshua Leung, Lukas Toenne
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	intern
	util
	../blenkernel
	../blenlib
	../bmesh
	../makesdna
	../makesrna
	../modifiers
	../windowmanager
	../../../intern/atomic
	../../../intern/guardedalloc
)

set(INC_SYS
)

set(SRC
	intern/depsgraph.cc
	intern/depsnode.cc
	intern/depsnode_component.cc
	intern/depsnode_operation.cc
	intern/depsgraph_build.cc
	intern/depsgraph_build_nodes.cc
	intern/depsgraph_build_relations.cc
	intern/depsgraph_debug.cc
	intern/depsgraph_eval.cc
	intern/depsgraph_query.cc
	intern/depsgraph_queue.cc
	intern/depsgraph_tag.cc
	intern/depsgraph_type_defines.cc
	util/depsgraph_util_cycle.cc
	util/depsgraph_util_pchanmap.cc
	util/depsgraph_util_transitive.cc

	DEG_depsgraph.h
	DEG_depsgraph_build.h
	DEG_depsgraph_debug.h
	DEG_depsgraph_query.h
	intern/depsgraph.h
	intern/depsnode.h
	intern/depsnode_component.h
	intern/depsnode_operation.h
	intern/depsnode_opcodes.h
	intern/depsgraph_build.h
	intern/depsgraph_debug.h
	intern/depsgraph_intern.h
	intern/depsgraph_queue.h
	intern/depsgraph_types.h

	util/depsgraph_util_cycle.h
	util/depsgraph_util_function.h
	util/depsgraph_util_hash.h
	util/depsgraph_util_map.h
	util/depsgraph_util_pchanmap.h
	util/depsgraph_util_set.h
	util/depsgraph_util_transitive.h
)

if(WITH_CXX11)
	add_definitions(-DDEG_STD_UNORDERED_MAP)
elseif(HAVE_STD_UNORDERED_MAP_HEADER)
	if(HAVE_UNORDERED_MAP_IN_STD_NAMESPACE)
		add_definitions(-DDEG_STD_UNORDERED_MAP)
	else()
		if(HAVE_UNORDERED_MAP_IN_TR1_NAMESPACE)
			add_definitions(-DDEG_STD_UNORDERED_MAP_IN_TR1_NAMESPACE)
		else()
			add_definitions(-DDEG_NO_UNORDERED_MAP)
			message(STATUS "Replacing unordered_map/set with map/set (warning: slower!)")
		endif()
	endif()
else()
	if(HAVE_UNORDERED_MAP_IN_TR1_NAMESPACE)
		add_definitions(-DDEG_TR1_UNORDERED_MAP)
	else()
		add_definitions(-DDEG_NO_UNORDERED_MAP)
		message(STATUS "Replacing unordered_map/set with map/set (warning: slower!)")
	endif()
endif()

if(WITH_LEGACY_DEPSGRAPH)
	add_definitions(-DWITH_LEGACY_DEPSGRAPH)
endif()

if(WITH_BOOST)
	list(APPEND INC_SYS
		${BOOST_INCLUDE_DIR}
	)
	add_definitions(-DHAVE_BOOST_FUNCTION_BINDINGS)
endif()

if(WITH_OPENSUBDIV)
	add_definitions(-DWITH_OPENSUBDIV)
endif()

blender_add_lib(bf_depsgraph "${SRC}" "${INC}" "${INC_SYS}")
