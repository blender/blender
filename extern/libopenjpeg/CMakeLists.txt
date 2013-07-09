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

set(INC
	.
)

set(INC_SYS

)

if(WIN32)
	add_definitions(-DOPJ_STATIC)
endif()

set(SRC
	bio.c
	cio.c
	dwt.c
	event.c
	image.c
	j2k.c
	j2k_lib.c
	jp2.c
	jpt.c
	mct.c
	mqc.c
	openjpeg.c
	pi.c
	raw.c
	t1.c
	t2.c
	tcd.c
	tgt.c
	cidx_manager.c
	phix_manager.c
	ppix_manager.c
	thix_manager.c
	tpix_manager.c

	bio.h
	cio.h
	dwt.h
	event.h
	fix.h
	image.h
	int.h
	j2k.h
	j2k_lib.h
	jp2.h
	jpt.h
	mct.h
	mqc.h
	openjpeg.h
	opj_includes.h
	opj_malloc.h
	pi.h
	raw.h
	t1.h
	t1_luts.h
	t2.h
	tcd.h
	tgt.h
	cidx_manager.h
	indexbox_manager.h
	opj_config.h
)

blender_add_lib(extern_openjpeg "${SRC}" "${INC}" "${INC_SYS}")
