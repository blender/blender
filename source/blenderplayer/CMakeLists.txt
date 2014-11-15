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

# message(STATUS "Configuring blenderplayer")

setup_libdirs()

if(WITH_CODEC_QUICKTIME)
	add_definitions(-DWITH_QUICKTIME)
endif()

if(WIN32 AND NOT UNIX)
	string(SUBSTRING ${BLENDER_VERSION} 0 1 bver1)
	string(SUBSTRING ${BLENDER_VERSION} 2 1 bver2)
	string(SUBSTRING ${BLENDER_VERSION} 3 1 bver3)
	if(MINGW)
		add_definitions(
			-DWINDRES
			-DBLEN_VER_RC_STR_M=${BLENDER_VERSION}
			-DBLEN_VER_RC_1=${bver1}
			-DBLEN_VER_RC_2=${bver2}
			-DBLEN_VER_RC_3=${bver3}
			-DBLEN_VER_RC_4=0
		)
	else()
		add_definitions(
			-DBLEN_VER_RC_STR=${BLENDER_VERSION}
			-DBLEN_VER_RC_1=${bver1}
			-DBLEN_VER_RC_2=${bver2}
			-DBLEN_VER_RC_3=${bver3}
			-DBLEN_VER_RC_4=0
		)
	endif()

	add_executable(blenderplayer ${EXETYPE} bad_level_call_stubs/stubs.c ../icons/winblender.rc)

	install(TARGETS blenderplayer
			COMPONENT Blenderplayer
			DESTINATION ".")

elseif(APPLE)
	add_executable(blenderplayer ${EXETYPE} bad_level_call_stubs/stubs.c)
	# setup Info.plist
	execute_process(COMMAND date "+%Y-%m-%d" OUTPUT_VARIABLE BLENDER_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
	set(PLAYER_SOURCEDIR ${CMAKE_SOURCE_DIR}/release/darwin/blenderplayer.app)
	set(PLAYER_SOURCEINFO ${PLAYER_SOURCEDIR}/Contents/Info.plist)
	set_target_properties(blenderplayer PROPERTIES
		MACOSX_BUNDLE_INFO_PLIST ${PLAYER_SOURCEINFO}
		MACOSX_BUNDLE_SHORT_VERSION_STRING ${BLENDER_VERSION}
		MACOSX_BUNDLE_LONG_VERSION_STRING "${BLENDER_VERSION} ${BLENDER_DATE}")

else()
	add_executable(blenderplayer bad_level_call_stubs/stubs.c)

	if(NOT WITH_INSTALL_PORTABLE)
		install(
			TARGETS blenderplayer
			DESTINATION bin
		)
	endif()
endif()

add_dependencies(blenderplayer makesdna)

get_property(BLENDER_LINK_LIBS GLOBAL PROPERTY BLENDER_LINK_LIBS)

list(APPEND BLENDER_LINK_LIBS
	ge_player_common
	ge_player_ghost
	blenkernel_blc
)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
	list(APPEND BLENDER_LINK_LIBS extern_binreloc)
endif()

# if(UNIX)
	# Sort libraries
	set(BLENDER_SORTED_LIBS
		ge_player_ghost 
		ge_player_common 
		bf_intern_string 
		bf_intern_ghost
		bf_intern_ghostndof3dconnexion
		bf_rna
		bf_blenkernel
		bf_intern_rigidbody
		bf_blenloader
		ge_blen_routines
		bf_editor_datafiles
		ge_converter 
		ge_logic_ketsji 
		ge_phys_bullet 
		ge_phys_dummy
		ge_logic 
		ge_rasterizer 
		ge_oglrasterizer 
		ge_logic_expressions 
		ge_scenegraph
		bf_ikplugin
		bf_intern_itasc
		bf_intern_iksolver
		bf_intern_smoke
		bf_modifiers
		bf_intern_moto 
		bf_nodes
		bf_gpu
		bf_imbuf
		bf_avi 
		ge_logic_network 
		ge_logic_ngnetwork 
		ge_logic_loopbacknetwork 
		extern_bullet 
		bf_intern_guardedalloc 
		bf_intern_memutil 
		bf_python_ext
		bf_python_mathutils
		bf_python_bmesh
		bf_intern_utfconv
		bf_imbuf_cineon
		bf_imbuf_openexr 
		bf_imbuf_openimageio
		extern_openjpeg
		extern_redcode
		bf_imbuf_dds
		bf_dna
		ge_videotex
		bf_blenfont
		bf_intern_audaspace
		blenkernel_blc
		bf_bmesh
		bf_blenlib
		bf_intern_utfconv
		extern_binreloc
		extern_minilzo
		bf_intern_ghost # duplicate for linking
		bf_blenkernel # duplicate for linking
		bf_intern_mikktspace
		extern_recastnavigation
		bf_intern_raskter
		bf_intern_opencolorio
		bf_intern_opennl
		bf_intern_glew_mx
		extern_rangetree
		extern_wcwidth
		extern_libmv
		extern_glog
	)

	if(WITH_MOD_CLOTH_ELTOPO)
		list(APPEND BLENDER_SORTED_LIBS extern_eltopo)
	endif()

	if(NOT WITH_SYSTEM_GLEW)
		list(APPEND BLENDER_SORTED_LIBS ${BLENDER_GLEW_LIBRARIES})
	endif()

	if(WITH_LZMA)
		list(APPEND BLENDER_SORTED_LIBS extern_lzma)
	endif()

	if(WITH_LIBMV)
		list(APPEND BLENDER_SORTED_LIBS extern_ceres)
	endif()

	list(APPEND BLENDER_SORTED_LIBS extern_colamd)

	if(WITH_MOD_BOOLEAN)
		list(APPEND BLENDER_SORTED_LIBS extern_carve)
	endif()

	if(WITH_GHOST_XDND)
		list(APPEND BLENDER_SORTED_LIBS extern_xdnd)
	endif()

	if(WITH_CODEC_QUICKTIME)
		list(APPEND BLENDER_SORTED_LIBS bf_quicktime)
	endif()

	if(WITH_CXX_GUARDEDALLOC)
		list(APPEND BLENDER_SORTED_LIBS bf_intern_guardedalloc_cpp)
	endif()

	if(WITH_INTERNATIONAL)
		list(APPEND BLENDER_SORTED_LIBS bf_intern_locale)
	endif()

	foreach(SORTLIB ${BLENDER_SORTED_LIBS})
		set(REMLIB ${SORTLIB})
		foreach(SEARCHLIB ${BLENDER_LINK_LIBS})
			if(${SEARCHLIB} STREQUAL ${SORTLIB})
				set(REMLIB "")
			endif()
		endforeach()
		if(REMLIB)
			# message(STATUS "Removing library ${REMLIB} from blenderplayer linking because: not configured")
			list(APPEND REM_MSG ${REMLIB})
			list(REMOVE_ITEM BLENDER_SORTED_LIBS ${REMLIB})
		endif()
	endforeach()
	if(REM_MSG)
		list(SORT REM_MSG)
		message(STATUS "Player Skipping: (${REM_MSG})")
	endif()
	target_link_libraries(blenderplayer ${BLENDER_SORTED_LIBS})

	unset(SEARCHLIB)
	unset(SORTLIB)
	unset(REMLIB)
	unset(REM_MSG)

# else()
# 	target_link_libraries(blenderplayer ${BLENDER_LINK_LIBS})
# endif()

if(WITH_PLAYER)
	add_subdirectory(bad_level_call_stubs)
endif()

setup_liblinks(blenderplayer)

# We put CLEW and CUEW here because OPENSUBDIV_LIBRARIES dpeends on them..
if(WITH_CYCLES OR WITH_COMPOSITOR OR WITH_OPENSUBDIV)
	target_link_libraries(blenderplayer "extern_clew")
	target_link_libraries(blenderplayer "extern_cuew")
endif()
