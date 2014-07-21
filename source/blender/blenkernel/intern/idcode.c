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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * return info about ID types
 */

/** \file blender/blenkernel/intern/idcode.c
 *  \ingroup bke
 */

#include <stdlib.h>
#include <string.h>

#include "DNA_ID.h"

#include "BLI_utildefines.h"

#include "BKE_idcode.h"

typedef struct {
	unsigned short code;
	const char *name, *plural;
	
	int flags;
#define IDTYPE_FLAGS_ISLINKABLE (1 << 0)
} IDType;

/* plural need to match rna_main.c's MainCollectionDef */
/* WARNING! Keep it in sync with i18n contexts in BLF_translation.h */
static IDType idtypes[] = {
	{ ID_AC,     "Action",           "actions",         IDTYPE_FLAGS_ISLINKABLE },
	{ ID_AR,     "Armature",         "armatures",       IDTYPE_FLAGS_ISLINKABLE },
	{ ID_BR,     "Brush",            "brushes",         IDTYPE_FLAGS_ISLINKABLE },
	{ ID_CA,     "Camera",           "cameras",         IDTYPE_FLAGS_ISLINKABLE },
	{ ID_CU,     "Curve",            "curves",          IDTYPE_FLAGS_ISLINKABLE },
	{ ID_GD,     "GPencil",          "grease_pencil",   IDTYPE_FLAGS_ISLINKABLE }, /* rename gpencil */
	{ ID_GR,     "Group",            "groups",          IDTYPE_FLAGS_ISLINKABLE },
	{ ID_ID,     "ID",               "ids",             0                       }, /* plural is fake */
	{ ID_IM,     "Image",            "images",          IDTYPE_FLAGS_ISLINKABLE },
	{ ID_IP,     "Ipo",              "ipos",            IDTYPE_FLAGS_ISLINKABLE }, /* deprecated */
	{ ID_KE,     "Key",              "shape_keys",      0                       },
	{ ID_LA,     "Lamp",             "lamps",           IDTYPE_FLAGS_ISLINKABLE },
	{ ID_LI,     "Library",          "libraries",       0                       },
	{ ID_LS,     "FreestyleLineStyle", "linestyles",    IDTYPE_FLAGS_ISLINKABLE },
	{ ID_LT,     "Lattice",          "lattices",        IDTYPE_FLAGS_ISLINKABLE },
	{ ID_MA,     "Material",         "materials",       IDTYPE_FLAGS_ISLINKABLE },
	{ ID_MB,     "Metaball",         "metaballs",       IDTYPE_FLAGS_ISLINKABLE },
	{ ID_MC,     "MovieClip",        "movieclips",      IDTYPE_FLAGS_ISLINKABLE },
	{ ID_ME,     "Mesh",             "meshes",          IDTYPE_FLAGS_ISLINKABLE },
	{ ID_MSK,    "Mask",             "masks",           IDTYPE_FLAGS_ISLINKABLE },
	{ ID_NT,     "NodeTree",         "node_groups",     IDTYPE_FLAGS_ISLINKABLE },
	{ ID_OB,     "Object",           "objects",         IDTYPE_FLAGS_ISLINKABLE },
	{ ID_PA,     "ParticleSettings", "particles",       0                       },
	{ ID_PAL,    "Palettes",         "palettes",        IDTYPE_FLAGS_ISLINKABLE },
	{ ID_PC,     "PaintCurve",       "paint_curves",    IDTYPE_FLAGS_ISLINKABLE },
	{ ID_SCE,    "Scene",            "scenes",          IDTYPE_FLAGS_ISLINKABLE },
	{ ID_SCR,    "Screen",           "screens",         0                       },
	{ ID_SEQ,    "Sequence",         "sequences",       0                       }, /* not actually ID data */
	{ ID_SPK,    "Speaker",          "speakers",        IDTYPE_FLAGS_ISLINKABLE },
	{ ID_SO,     "Sound",            "sounds",          IDTYPE_FLAGS_ISLINKABLE },
	{ ID_TE,     "Texture",          "textures",        IDTYPE_FLAGS_ISLINKABLE },
	{ ID_TXT,    "Text",             "texts",           IDTYPE_FLAGS_ISLINKABLE },
	{ ID_VF,     "VFont",            "fonts",           IDTYPE_FLAGS_ISLINKABLE },
	{ ID_WO,     "World",            "worlds",          IDTYPE_FLAGS_ISLINKABLE },
	{ ID_WM,     "WindowManager",    "window_managers", 0                       },
};

static IDType *idtype_from_name(const char *str) 
{
	int i = ARRAY_SIZE(idtypes);

	while (i--) {
		if (STREQ(str, idtypes[i].name)) {
			return &idtypes[i];
		}
	}

	return NULL;
}
static IDType *idtype_from_code(int code) 
{
	int i = ARRAY_SIZE(idtypes);

	while (i--)
		if (code == idtypes[i].code)
			return &idtypes[i];
	
	return NULL;
}

/**
 * Return if the ID code is a valid ID code.
 *
 * \param code The code to check.
 * \return Boolean, 0 when invalid.
 */
bool BKE_idcode_is_valid(int code)
{
	return idtype_from_code(code) ? true : false;
}

/**
 * Return non-zero when an ID type is linkable.
 *
 * \param code The code to check.
 * \return Boolean, 0 when non linkable.
 */
bool BKE_idcode_is_linkable(int code)
{
	IDType *idt = idtype_from_code(code);
	BLI_assert(idt);
	return idt ? ((idt->flags & IDTYPE_FLAGS_ISLINKABLE) != 0) : false;
}

/**
 * Convert an idcode into a name.
 *
 * \param code The code to convert.
 * \return A static string representing the name of
 * the code.
 */
const char *BKE_idcode_to_name(int code) 
{
	IDType *idt = idtype_from_code(code);
	BLI_assert(idt);
	return idt ? idt->name : NULL;
}

/**
 * Convert a name into an idcode (ie. ID_SCE)
 *
 * \param name The name to convert.
 * \return The code for the name, or 0 if invalid.
 */
int BKE_idcode_from_name(const char *name) 
{
	IDType *idt = idtype_from_name(name);
	BLI_assert(idt);
	return idt ? idt->code : 0;
}

/**
 * Convert an idcode into a name (plural).
 *
 * \param code The code to convert.
 * \return A static string representing the name of
 * the code.
 */
const char *BKE_idcode_to_name_plural(int code) 
{
	IDType *idt = idtype_from_code(code);
	BLI_assert(idt);
	return idt ? idt->plural : NULL;
}

/**
 * Return an ID code and steps the index forward 1.
 *
 * \param index start as 0.
 * \return the code, 0 when all codes have been returned.
 */
int BKE_idcode_iter_step(int *index)
{
	return (*index < ARRAY_SIZE(idtypes)) ? idtypes[(*index)++].code : 0;
}
