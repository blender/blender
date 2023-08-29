/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 */

#include "MEM_guardedalloc.h"

#include "DNA_layer_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_math_vector.h"
#include "BLI_rand.h"

#include "BKE_context.h"
#include "BKE_layer.h"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_transverts.hh"

#include "object_intern.h"

/**
 * Generic randomize vertices function
 */

static bool object_rand_transverts(TransVertStore *tvs,
                                   const float offset,
                                   const float uniform,
                                   const float normal_factor,
                                   const uint seed)
{
  bool use_normal = (normal_factor != 0.0f);
  RNG *rng;
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
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob_active = CTX_data_edit_object(C);
  const int ob_mode = ob_active->mode;

  const float offset = RNA_float_get(op->ptr, "offset");
  const float uniform = RNA_float_get(op->ptr, "uniform");
  const float normal_factor = RNA_float_get(op->ptr, "normal");
  const uint seed = RNA_int_get(op->ptr, "seed");

  bool changed_multi = false;
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_mode_unique_data(
      scene, view_layer, CTX_wm_view3d(C), &objects_len, eObjectMode(ob_mode));
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *ob_iter = objects[ob_index];

    TransVertStore tvs = {nullptr};

    if (ob_iter) {
      int mode = TM_ALL_JOINTS;

      if (normal_factor != 0.0f) {
        mode |= TX_VERT_USE_NORMAL;
      }

      ED_transverts_create_from_obedit(&tvs, ob_iter, mode);
      if (tvs.transverts_tot == 0) {
        continue;
      }

      int seed_iter = seed;
      /* This gives a consistent result regardless of object order. */
      if (ob_index) {
        seed_iter += BLI_ghashutil_strhash_p(ob_iter->id.name);
      }

      object_rand_transverts(&tvs, offset, uniform, normal_factor, seed_iter);

      ED_transverts_update_obedit(&tvs, ob_iter);
      ED_transverts_free(&tvs);

      WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob_iter);
      changed_multi = true;
    }
  }
  MEM_freeN(objects);

  return changed_multi ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void TRANSFORM_OT_vertex_random(wmOperatorType *ot)
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
  ot->prop = RNA_def_float_distance(
      ot->srna, "offset", 0.0f, -FLT_MAX, FLT_MAX, "Amount", "Distance to offset", -10.0f, 10.0f);
  RNA_def_float_factor(ot->srna,
                       "uniform",
                       0.0f,
                       0.0f,
                       1.0f,
                       "Uniform",
                       "Increase for uniform offset distance",
                       0.0f,
                       1.0f);
  RNA_def_float_factor(ot->srna,
                       "normal",
                       0.0f,
                       0.0f,
                       1.0f,
                       "Normal",
                       "Align offset direction to normals",
                       0.0f,
                       1.0f);
  RNA_def_int(
      ot->srna, "seed", 0, 0, 10000, "Random Seed", "Seed for the random number generator", 0, 50);

  /* Set generic modal callbacks. */
  WM_operator_type_modal_from_exec_for_object_edit_coords(ot);
}
