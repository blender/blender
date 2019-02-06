/*
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
 * The Original Code is Copyright (C) 2006 by Blender Foundation
 * All rights reserved.
 */
/** \file \ingroup render
 */


#ifndef __RE_SHADER_EXT_H__
#define __RE_SHADER_EXT_H__

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* this include is for texture exports                        */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* localized texture result data */
/* note; tr tg tb ta has to remain in this order */
typedef struct TexResult {
	float tin, tr, tg, tb, ta;
	int talpha;
	float *nor;
} TexResult;

typedef struct BakeImBufuserData {
	float *displacement_buffer;
	char *mask_buffer;
} BakeImBufuserData;

/* node shaders... */
struct ImBuf;
struct ImagePool;
struct MTex;
struct Object;
struct Tex;

/* this one uses nodes */
int multitex_ext(struct Tex *tex,
                 float texvec[3],
                 float dxt[3], float dyt[3],
                 int osatex,
                 struct TexResult *texres,
                 const short thread,
                 struct ImagePool *pool,
                 bool scene_color_manage,
                 const bool skip_load_image);
/* nodes disabled */
int multitex_ext_safe(struct Tex *tex, float texvec[3], struct TexResult *texres, struct ImagePool *pool, bool scene_color_manage, const bool skip_load_image);
/* only for internal node usage */
int multitex_nodes(struct Tex *tex, float texvec[3], float dxt[3], float dyt[3], int osatex, struct TexResult *texres,
                   const short thread, short which_output, struct MTex *mtex, struct ImagePool *pool);

#endif /* __RE_SHADER_EXT_H__ */
