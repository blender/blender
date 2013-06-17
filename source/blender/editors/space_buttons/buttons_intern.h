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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_buttons/buttons_intern.h
 *  \ingroup spbuttons
 */

#ifndef __BUTTONS_INTERN_H__
#define __BUTTONS_INTERN_H__

#include "DNA_listBase.h"
#include "RNA_types.h"

struct ARegion;
struct ARegionType;
struct ID;
struct SpaceButs;
struct Tex;
struct bContext;
struct bContextDataResult;
struct bNode;
struct bNodeTree;
struct uiLayout;
struct wmOperatorType;

/* buts->scaflag */		
#define BUTS_SENS_SEL       1
#define BUTS_SENS_ACT       2
#define BUTS_SENS_LINK      4
#define BUTS_CONT_SEL       8
#define BUTS_CONT_ACT       16
#define BUTS_CONT_LINK      32
#define BUTS_ACT_SEL        64
#define BUTS_ACT_ACT        128
#define BUTS_ACT_LINK       256
#define BUTS_SENS_STATE     512
#define BUTS_ACT_STATE      1024

/* context data */

typedef struct ButsContextPath {
	PointerRNA ptr[8];
	int len;
	int flag;
	int tex_ctx;
} ButsContextPath;

typedef struct ButsTextureUser {
	struct ButsTextureUser *next, *prev;

	struct ID *id;

	PointerRNA ptr;
	PropertyRNA *prop;

	struct bNodeTree *ntree;
	struct bNode *node;

	const char *category;
	int icon;
	const char *name;

	int index;
} ButsTextureUser;

typedef struct ButsContextTexture {
	ListBase users;

	struct Tex *texture;

	struct ButsTextureUser *user;
	int index;
} ButsContextTexture;

/* internal exports only */

/* buttons_header.c */
void buttons_header_buttons(const struct bContext *C, struct ARegion *ar);

/* buttons_context.c */
void buttons_context_compute(const struct bContext *C, struct SpaceButs *sbuts);
int buttons_context(const struct bContext *C, const char *member, struct bContextDataResult *result);
void buttons_context_draw(const struct bContext *C, struct uiLayout *layout);
void buttons_context_register(struct ARegionType *art);
struct ID *buttons_context_id_path(const struct bContext *C);

extern const char *buttons_context_dir[]; /* doc access */

/* buttons_texture.c */
void buttons_texture_context_compute(const struct bContext *C, struct SpaceButs *sbuts);

/* buttons_ops.c */
void BUTTONS_OT_file_browse(struct wmOperatorType *ot);
void BUTTONS_OT_directory_browse(struct wmOperatorType *ot);
void BUTTONS_OT_toolbox(struct wmOperatorType *ot);

#endif /* __BUTTONS_INTERN_H__ */

