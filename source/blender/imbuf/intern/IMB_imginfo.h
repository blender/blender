/**
 * $Id$ 
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#ifndef _IMB_IMGINFO_H
#define _IMB_IMGINFO_H

#ifdef __cplusplus
extern "C" {
#endif

struct ImBuf;

typedef struct ImgInfo {
	struct ImgInfo *next, *prev;
	char* key;
	char* value;
	int len;
} ImgInfo;

/** The imginfo is a list of key/value pairs (both char*) that can me 
    saved in the header of several image formats.
	Apart from some common keys like 
	'Software' and 'Description' (png standard) we'll use keys within the 
	Blender namespace, so should be called 'Blender::StampInfo' or 'Blender::FrameNum'
	etc... 
*/


/* free blender ImgInfo struct */
void IMB_imginfo_free(struct ImBuf* img);

/** read the field from the image info into the field 
 *  @param img - the ImBuf that contains the image data
 *  @param key - the key of the field
 *  @param value - the data in the field, first one found with key is returned, 
                  memory has to be allocated by user.
 *  @param len - length of value buffer allocated by user.
 *  @return    - 1 (true) if ImageInfo present and value for the key found, 0 (false) otherwise
 */
int IMB_imginfo_get_field(struct ImBuf* img, const char* key, char* value, int len);

/** set user data in the ImgInfo struct, which has to be allocated with IMB_imginfo_create
 *  before calling this function.
 *  @param img - the ImBuf that contains the image data
 *  @param key - the key of the field
 *  @param value - the data to be written to the field. zero terminated string
 *  @return    - 1 (true) if ImageInfo present, 0 (false) otherwise
 */
int IMB_imginfo_add_field(struct ImBuf* img, const char* key, const char* field);

/** delete the key/field par in the ImgInfo struct.
 * @param img - the ImBuf that contains the image data
 * @param key - the key of the field
 * @return - 1 (true) if delete the key/field, 0 (false) otherwise
 */
int IMB_imginfo_del_field(struct ImBuf *img, const char *key);

#endif /* _IMB_IMGINFO_H */

