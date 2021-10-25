/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2015, Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file decklink/DeckLinkAPI.h
 *  \ingroup decklink
 */

#ifndef __DECKLINKAPI_H__
#define __DECKLINKAPI_H__

/* Include the OS specific Declink headers */

#ifdef WIN32
#  include <windows.h>
#  include <objbase.h>
#  include <comutil.h>
#  include "win/DeckLinkAPI_h.h"
	typedef unsigned int   dl_size_t;
#elif defined(__APPLE__)
#  error "Decklink not supported in OSX"
#else
#  include "linux/DeckLinkAPI.h"
	/* Windows COM API uses BOOL, linux uses bool */
#  define BOOL bool
	typedef uint32_t    dl_size_t;
#endif


/* OS independent function to get the device iterator */
IDeckLinkIterator* BMD_CreateDeckLinkIterator(void);

#endif  /* __DECKLINKAPI_H__ */
