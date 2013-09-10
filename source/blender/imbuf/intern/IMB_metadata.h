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

/** \file blender/imbuf/intern/IMB_metadata.h
 *  \ingroup imbuf
 */


#ifndef __IMB_METADATA_H__
#define __IMB_METADATA_H__

struct ImBuf;

typedef struct ImMetaData {
	struct ImMetaData *next, *prev;
	char *key;
	char *value;
	int len;
} ImMetaData;

/** The metadata is a list of key/value pairs (both char *) that can me
 * saved in the header of several image formats.
 * Apart from some common keys like
 * 'Software' and 'Description' (png standard) we'll use keys within the
 * Blender namespace, so should be called 'Blender::StampInfo' or 'Blender::FrameNum'
 * etc...
 */


/* free blender ImMetaData struct */
void IMB_metadata_free(struct ImBuf *img);

/** read the field from the image info into the field 
 *  \param img - the ImBuf that contains the image data
 *  \param key - the key of the field
 *  \param value - the data in the field, first one found with key is returned, 
 *                 memory has to be allocated by user.
 *  \param len - length of value buffer allocated by user.
 *  \return    - 1 (true) if ImageInfo present and value for the key found, 0 (false) otherwise
 */
bool IMB_metadata_get_field(struct ImBuf *img, const char *key, char *value, const size_t len);

/** set user data in the ImMetaData struct, which has to be allocated with IMB_metadata_create
 *  before calling this function.
 *  \param img - the ImBuf that contains the image data
 *  \param key - the key of the field
 *  \param value - the data to be written to the field. zero terminated string
 *  \return    - 1 (true) if ImageInfo present, 0 (false) otherwise
 */
bool IMB_metadata_add_field(struct ImBuf *img, const char *key, const char *value);

/** delete the key/field par in the ImMetaData struct.
 * \param img - the ImBuf that contains the image data
 * \param key - the key of the field
 * \return - 1 (true) if delete the key/field, 0 (false) otherwise
 */
bool IMB_metadata_del_field(struct ImBuf *img, const char *key);

#endif /* __IMB_METADATA_H__ */
