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
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	../include
	../../blenkernel
	../../blenlib
	../../makesdna
	../../makesrna
	../../windowmanager
	../../../../intern/guardedalloc
)

set(INC_SYS

)

set(SRC
	sound_ops.c

	sound_intern.h
)

if(WITH_AUDASPACE)
	list(APPEND INC
		../../../../intern/audaspace/intern
	)
	add_definitions(-DWITH_AUDASPACE)
endif()

if(WITH_CODEC_FFMPEG)
	add_definitions(-DWITH_FFMPEG)
endif()

if(WITH_CODEC_SNDFILE)
	add_definitions(-DWITH_SNDFILE)
endif()

blender_add_lib(bf_editor_sound "${SRC}" "${INC}" "${INC_SYS}")
