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
# The Original Code is Copyright (C) 2015, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Blender Foundation.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
)

set(INC_SYS
)

set(SRC
	DeckLinkAPI.cpp
	DeckLinkAPI.h
)

if(WIN32)
	list(APPEND SRC
		win/DeckLinkAPI_h.h
		win/DeckLinkAPI_i.c
	)
endif()

if(UNIX AND NOT APPLE)
	list(APPEND SRC
		linux/DeckLinkAPI.h
		linux/DeckLinkAPIConfiguration.h
		linux/DeckLinkAPIDeckControl.h
		linux/DeckLinkAPIDiscovery.h
		linux/DeckLinkAPIDispatch.cpp
		linux/DeckLinkAPIModes.h
		linux/DeckLinkAPIVersion.h
		linux/DeckLinkAPITypes.h
		linux/LinuxCOM.h
	)
endif()

blender_add_lib(bf_intern_decklink "${SRC}" "${INC}" "${INC_SYS}")
