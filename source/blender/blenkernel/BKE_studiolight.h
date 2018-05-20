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
 * The Original Code is Copyright (C) 2006-2007 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_STUDIOLIGHT_H__
#define __BKE_STUDIOLIGHT_H__

/** \file BKE_studiolight.h
 *  \ingroup bke
 *
 * Studio lighting for the 3dview
 */

#include "BLI_sys_types.h"

#include "DNA_space_types.h"

/*
 * These defines are the indexes in the StudioLight.diffuse_light
 * X_POS means the light that is traveling towards the positive X
 * So Light direction.
 */
#define STUDIOLIGHT_X_POS 0
#define STUDIOLIGHT_X_NEG 1
#define STUDIOLIGHT_Y_POS 2
#define STUDIOLIGHT_Y_NEG 3
#define STUDIOLIGHT_Z_POS 4
#define STUDIOLIGHT_Z_NEG 5

enum StudioLightFlag
{
	STUDIOLIGHT_DIFFUSE_LIGHT_CALCULATED   = (1 << 0),
	STUDIOLIGHT_LIGHT_DIRECTION_CALCULATED = (1 << 1),
	STUDIOLIGHT_EXTERNAL_FILE              = (1 << 2),
	STUDIOLIGHT_ORIENTATION_CAMERA         = (1 << 3),
	STUDIOLIGHT_ORIENTATION_WORLD          = (1 << 4),
} StudioLightFlag;

typedef struct StudioLight
{
	struct StudioLight *next, *prev;
	int flag;
	char name[FILE_MAXFILE];
	char path[FILE_MAX];
	int icon_id;
	int index;
	float diffuse_light[6][3];
	float light_direction[3];
} StudioLight;

void BKE_studiolight_init(void);
void BKE_studiolight_free(void);
struct StudioLight *BKE_studiolight_find(const char *name);
struct StudioLight *BKE_studiolight_findindex(int index);
unsigned int *BKE_studiolight_preview(StudioLight *sl, int icon_size);
const struct ListBase *BKE_studiolight_listbase(void);
void BKE_studiolight_ensure_flag(StudioLight *sl, int flag);

#endif /*  __BKE_STUDIOLIGHT_H__ */
