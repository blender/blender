/**
 * blenlib/BKE_image.h (mar-2001 nzc)
 *	
 * $Id$ 
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
#ifndef BKE_IMAGE_H
#define BKE_IMAGE_H

#ifdef __cplusplus
extern "C" {
#endif

struct Image;
struct ImBuf;
struct Tex;
struct anim;

void free_image(struct Image *me);
void free_image_buffers(struct Image *ima);
struct Image *add_image(char *name);
void free_unused_animimages(void);

void makepicstring(char *string, int frame);
void addImageExtension(char *string);

struct anim *openanim(char * name, int flags);
void ima_ibuf_is_nul(struct Tex *tex, struct Image *ima);
void load_image(struct Image * ima, int flags, char *relabase, int framenum);
void converttopremul(struct ImBuf *ibuf);

void tag_image_time(struct Image *ima);
void free_old_images(void);


#ifdef __cplusplus
}
#endif

#endif

