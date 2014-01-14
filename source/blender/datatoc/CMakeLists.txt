# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public LicenseS
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
# Contributor(s): Campbell Barton.
#
# ***** END GPL LICENSE BLOCK *****


# -----------------------------------------------------------------------------
# Build datatoc executable
set(SRC
	datatoc.c
)

# SRC_DNA_INC is defined in the parent dir
add_executable(datatoc ${SRC})


# -----------------------------------------------------------------------------
# Build datatoc_icon executable

if(NOT WITH_HEADLESS)

	set(SRC
		datatoc_icon.c
	)

	if(WIN32)
		include_directories(
			../blenlib
			../../../intern/utfconv
		)

		# for winstuff_dir.c
		add_definitions(-DUSE_STANDALONE)

		list(APPEND SRC
			../blenlib/intern/winstuff_dir.c
			../../../intern/utfconv/utfconv.c
		)
	endif()

	include_directories(${PNG_INCLUDE_DIR})
	link_directories(${PNG_LIBPATH} ${ZLIB_LIBPATH})

	add_executable(datatoc_icon ${SRC})

	target_link_libraries(datatoc_icon ${PNG_LIBRARIES} ${ZLIB_LIBRARIES})
endif()
