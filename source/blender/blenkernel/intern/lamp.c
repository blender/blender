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
 */

/** \file blender/blenkernel/intern/lamp.c
 *  \ingroup bke
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_animsys.h"
#include "BKE_colortools.h"
#include "BKE_icons.h"
#include "BKE_lamp.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_library_remap.h"
#include "BKE_main.h"
#include "BKE_node.h"

void BKE_lamp_init(Lamp *la)
{
	BLI_assert(MEMCMP_STRUCT_OFS_IS_ZERO(la, id));

	la->r = la->g = la->b = la->k = 1.0f;
	la->energy = 10.0f;
	la->dist = 25.0f;
	la->spotsize = DEG2RADF(45.0f);
	la->spotblend = 0.15f;
	la->att2 = 1.0f;
	la->mode = LA_SHADOW;
	la->bufsize = 512;
	la->clipsta = 0.5f;
	la->clipend = 40.0f;
	la->bleedexp = 2.5f;
	la->samp = 3;
	la->bias = 1.0f;
	la->soft = 3.0f;
	la->area_size = la->area_sizey = la->area_sizez = 0.25f;
	la->buffers = 1;
	la->preview = NULL;
	la->falloff_type = LA_FALLOFF_INVSQUARE;
	la->coeff_const = 1.0f;
	la->coeff_lin = 0.0f;
	la->coeff_quad = 0.0f;
	la->curfalloff = curvemapping_add(1, 0.0f, 1.0f, 1.0f, 0.0f);
	la->cascade_max_dist = 1000.0f;
	la->cascade_count = 4;
	la->cascade_exponent = 0.8f;
	la->cascade_fade = 0.1f;
	la->contact_dist = 0.2f;
	la->contact_bias = 0.03f;
	la->contact_spread = 0.2f;
	la->contact_thickness = 0.2f;
	la->spec_fac = 1.0f;

	curvemapping_initialize(la->curfalloff);
}

Lamp *BKE_lamp_add(Main *bmain, const char *name)
{
	Lamp *la;

	la =  BKE_libblock_alloc(bmain, ID_LA, name, 0);

	BKE_lamp_init(la);

	return la;
}

/**
 * Only copy internal data of Lamp ID from source to already allocated/initialized destination.
 * You probably nerver want to use that directly, use id_copy or BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag  Copying options (see BKE_library.h's LIB_ID_COPY_... flags for more).
 */
void BKE_lamp_copy_data(Main *bmain, Lamp *la_dst, const Lamp *la_src, const int flag)
{
	la_dst->curfalloff = curvemapping_copy(la_src->curfalloff);

	if (la_src->nodetree) {
		/* Note: nodetree is *not* in bmain, however this specific case is handled at lower level
		 *       (see BKE_libblock_copy_ex()). */
		BKE_id_copy_ex(bmain, (ID *)la_src->nodetree, (ID **)&la_dst->nodetree, flag, false);
	}

	if ((flag & LIB_ID_COPY_NO_PREVIEW) == 0) {
		BKE_previewimg_id_copy(&la_dst->id, &la_src->id);
	}
	else {
		la_dst->preview = NULL;
	}
}

Lamp *BKE_lamp_copy(Main *bmain, const Lamp *la)
{
	Lamp *la_copy;
	BKE_id_copy_ex(bmain, &la->id, (ID **)&la_copy, 0, false);
	return la_copy;
}

Lamp *BKE_lamp_localize(Lamp *la)
{
	/* TODO replace with something like
	 * 	Lamp *la_copy;
	 * 	BKE_id_copy_ex(bmain, &la->id, (ID **)&la_copy, LIB_ID_COPY_NO_MAIN | LIB_ID_COPY_NO_PREVIEW | LIB_ID_COPY_NO_USER_REFCOUNT, false);
	 * 	return la_copy;
	 *
	 * ... Once f*** nodes are fully converted to that too :( */

	Lamp *lan = BKE_libblock_copy_nolib(&la->id, false);

	lan->curfalloff = curvemapping_copy(la->curfalloff);

	if (la->nodetree)
		lan->nodetree = ntreeLocalize(la->nodetree);

	lan->preview = NULL;

	return lan;
}

void BKE_lamp_make_local(Main *bmain, Lamp *la, const bool lib_local)
{
	BKE_id_make_local_generic(bmain, &la->id, true, lib_local);
}

void BKE_lamp_free(Lamp *la)
{
	BKE_animdata_free((ID *)la, false);

	curvemapping_free(la->curfalloff);

	/* is no lib link block, but lamp extension */
	if (la->nodetree) {
		ntreeFreeTree(la->nodetree);
		MEM_freeN(la->nodetree);
		la->nodetree = NULL;
	}

	BKE_previewimg_free(&la->preview);
	BKE_icon_id_delete(&la->id);
	la->id.icon_id = 0;
}
