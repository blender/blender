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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/probe.c
 *  \ingroup bke
 */

#include "DNA_object_types.h"
#include "DNA_probe_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_animsys.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_probe.h"

void BKE_probe_init(Probe *probe)
{
	BLI_assert(MEMCMP_STRUCT_OFS_IS_ZERO(probe, id));

	copy_v3_fl(probe->mininf, -5.0f);
	copy_v3_fl(probe->maxinf, 5.0f);
	copy_v3_fl(probe->minpar, -5.0f);
	copy_v3_fl(probe->maxpar, 5.0f);
	probe->falloff = 0.25f;
	probe->clipsta = 0.5f;
	probe->clipend = 40.0f;
}

void *BKE_probe_add(Main *bmain, const char *name)
{
	Probe *probe;

	probe =  BKE_libblock_alloc(bmain, ID_PRB, name);

	BKE_probe_init(probe);

	return probe;
}

Probe *BKE_probe_copy(Main *bmain, Probe *probe)
{
	Probe *probe_new;

	probe_new = BKE_libblock_copy(bmain, &probe->id);

	BKE_id_copy_ensure_local(bmain, &probe->id, &probe_new->id);

	return probe_new;
}

void BKE_probe_make_local(Main *bmain, Probe *probe, const bool lib_local)
{
	BKE_id_make_local_generic(bmain, &probe->id, true, lib_local);
}

void BKE_probe_free(Probe *probe)
{
	BKE_animdata_free((ID *)probe, false);
}
