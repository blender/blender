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
 * The Original Code is Copyright (C) 2005 Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Austin Benesh. Ton Roosendaal.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/imbuf/intern/openexr/openexr_api.h
 *  \ingroup openexr
 */


#ifndef __OPENEXR_API_H__
#define __OPENEXR_API_H__

#ifdef __cplusplus
extern "C" {
#endif
  
#include <stdio.h>
  
  /**
 * Test presence of OpenEXR file.
 * @param mem pointer to loaded OpenEXR bitstream
 */
  
int		imb_is_a_openexr			(unsigned char *mem);
	
int		imb_save_openexr			(struct ImBuf *ibuf, const char *name, int flags);

struct ImBuf *imb_load_openexr		(unsigned char *mem, size_t size, int flags);

#ifdef __cplusplus
}
#endif



#endif /* __OPENEXR_API_H */
