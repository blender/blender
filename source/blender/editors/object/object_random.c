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
 * The Original Code is Copyright (C) 2014 by Blender Foundation
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/object/object_random.c
 *  \ingroup edobj
 */

#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_rand.h"


#include "BKE_context.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_transverts.h"

#include "object_intern.h"


/**
 * Generic randomize vertices function
 */

static bool object_rand_transverts(
        TransVertStore *tvs,
        const float offset, const float uniform, const float normal_factor,
        const unsigned int seed)
{
	bool use_normal = (normal_factor != 0.0f);
	struct RNG *rng;
	TransVert *tv;
	int a;

	if (!tvs || !(tvs->transverts)) {
		return false;
	}

	rng = BLI_rng_new(seed);

	tv = tvs->transverts;
	for (a = 0; a < tvs->transverts_tot; a++, tv++) {
		const float t = max_ff(0.0f, uniform + ((1.0f - uniform) * BLI_rng_get_float(rng)));
		float vec[3];
		BLI_rng_get_float_unit_v3(rng, vec);

		if (use_normal && (tv->flag & TX_VERT_USE_NORMAL)) {
			float no[3];

			/* avoid >90d rotation to align with normal */
			if (dot_v3v3(vec, tv->normal) < 0.0f) {
				negate_v3_v3(no, tv->normal);
			}
			else {
				copy_v3_v3(no, tv->normal);
			}

			interp_v3_v3v3_slerp_safe(vec, vec, no, normal_factor);
		}

		madd_v3_v3fl(tv->loc, vec, offset * t);
	}

	BLI_rng_free(rng);

	return true;
}

static int object_rand_verts_exec(bContext *C, wmOperator *op)
{
	const float offset = RNA_float_get(op->ptr, "offset");
	const float uniform = RNA_float_get(op->ptr, "uniform");
	const float normal_factor = RNA_float_get(op->ptr, "normal");
	const unsigned int seed = RNA_int_get(op->ptr, "seed");

	TransVertStore tvs = {NULL};
	Object *obedit = CTX_data_edit_object(C);

	if (obedit) {
		int mode = TM_ALL_JOINTS;

		if (normal_factor != 0.0f) {
			mode |= TX_VERT_USE_NORMAL;
		}

		ED_transverts_create_from_obedit(&tvs, obedit, mode);
		if (tvs.transverts_tot == 0)
			return OPERATOR_CANCELLED;

		object_rand_transverts(&tvs, offset, uniform, normal_factor, seed);

		ED_transverts_update_obedit(&tvs, obedit);
		ED_transverts_free(&tvs);
	}

	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obedit);

	return OPERATOR_FINISHED;
}

void TRANSFORM_OT_vertex_random(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Randomize";
	ot->description = "Randomize vertices";
	ot->idname = "TRANSFORM_OT_vertex_random";

	/* api callbacks */
	ot->exec = object_rand_verts_exec;
	ot->poll = ED_transverts_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_float(ot->srna, "offset",  0.1f, -FLT_MAX, FLT_MAX, "Amount", "Distance to offset", -10.0f, 10.0f);
	RNA_def_float(ot->srna, "uniform",  0.0f, 0.0f, 1.0f, "Uniform",
	              "Increase for uniform offset distance", 0.0f, 1.0f);
	RNA_def_float(ot->srna, "normal",  0.0f, 0.0f, 1.0f, "normal",
	              "Align offset direction to normals", 0.0f, 1.0f);
	RNA_def_int(ot->srna, "seed", 0, 0, 10000, "Random Seed", "Seed for the random number generator", 0, 50);
}
