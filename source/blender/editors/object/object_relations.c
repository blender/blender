/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup edobj
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_vfont_types.h"
#include "DNA_world_types.h"

#include "BLI_kdtree.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_DerivedMesh.h"
#include "BKE_action.h"
#include "BKE_anim_data.h"
#include "BKE_armature.h"
#include "BKE_camera.h"
#include "BKE_collection.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_curves.h"
#include "BKE_displist.h"
#include "BKE_editmesh.h"
#include "BKE_fcurve.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_idprop.h"
#include "BKE_idtype.h"
#include "BKE_lattice.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_lib_override.h"
#include "BKE_lib_query.h"
#include "BKE_lib_remap.h"
#include "BKE_light.h"
#include "BKE_lightprobe.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_pointcloud.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_speaker.h"
#include "BKE_texture.h"
#include "BKE_volume.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "ED_armature.h"
#include "ED_curve.h"
#include "ED_gpencil_legacy.h"
#include "ED_keyframing.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "MOD_nodes.h"

#include "object_intern.h"

/* ------------------------------------------------------------------- */
/** \name Make Vertex Parent Operator
 * \{ */

static bool vertex_parent_set_poll(bContext *C)
{
  return ED_operator_editmesh(C) || ED_operator_editsurfcurve(C) || ED_operator_editlattice(C);
}

static int vertex_parent_set_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  View3D *v3d = CTX_wm_view3d(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *obedit = CTX_data_edit_object(C);
  Object *par;

#define INDEX_UNSET -1
  int par1, par2, par3, par4;
  par1 = par2 = par3 = par4 = INDEX_UNSET;

  /* we need 1 to 3 selected vertices */

  if (obedit->type == OB_MESH) {
    Mesh *me = obedit->data;
    BMEditMesh *em;

    EDBM_mesh_load(bmain, obedit);
    EDBM_mesh_make(obedit, scene->toolsettings->selectmode, true);

    DEG_id_tag_update(obedit->data, 0);

    em = me->edit_mesh;

    BKE_editmesh_looptri_and_normals_calc(em);

    /* Make sure the evaluated mesh is updated.
     *
     * Most reliable way is to update the tagged objects, which will ensure
     * proper copy-on-write update, but also will make sure all dependent
     * objects are also up to date. */
    BKE_scene_graph_update_tagged(depsgraph, bmain);

    BMVert *eve;
    BMIter iter;
    int curr_index;
    BM_ITER_MESH_INDEX (eve, &iter, em->bm, BM_VERTS_OF_MESH, curr_index) {
      if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
        if (par1 == INDEX_UNSET) {
          par1 = curr_index;
        }
        else if (par2 == INDEX_UNSET) {
          par2 = curr_index;
        }
        else if (par3 == INDEX_UNSET) {
          par3 = curr_index;
        }
        else if (par4 == INDEX_UNSET) {
          par4 = curr_index;
        }
        else {
          break;
        }
      }
    }
  }
  else if (ELEM(obedit->type, OB_SURF, OB_CURVES_LEGACY)) {
    ListBase *editnurb = object_editcurve_get(obedit);

    for (Nurb *nu = editnurb->first; nu != NULL; nu = nu->next) {
      if (nu->type == CU_BEZIER) {
        BezTriple *bezt = nu->bezt;
        for (int curr_index = 0; curr_index < nu->pntsu; curr_index++, bezt++) {
          if (BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt)) {
            if (par1 == INDEX_UNSET) {
              par1 = curr_index;
            }
            else if (par2 == INDEX_UNSET) {
              par2 = curr_index;
            }
            else if (par3 == INDEX_UNSET) {
              par3 = curr_index;
            }
            else if (par4 == INDEX_UNSET) {
              par4 = curr_index;
            }
            else {
              break;
            }
          }
        }
      }
      else {
        BPoint *bp = nu->bp;
        const int num_points = nu->pntsu * nu->pntsv;
        for (int curr_index = 0; curr_index < num_points; curr_index++, bp++) {
          if (bp->f1 & SELECT) {
            if (par1 == INDEX_UNSET) {
              par1 = curr_index;
            }
            else if (par2 == INDEX_UNSET) {
              par2 = curr_index;
            }
            else if (par3 == INDEX_UNSET) {
              par3 = curr_index;
            }
            else if (par4 == INDEX_UNSET) {
              par4 = curr_index;
            }
            else {
              break;
            }
          }
        }
      }
    }
  }
  else if (obedit->type == OB_LATTICE) {
    Lattice *lt = obedit->data;

    const int num_points = lt->editlatt->latt->pntsu * lt->editlatt->latt->pntsv *
                           lt->editlatt->latt->pntsw;
    BPoint *bp = lt->editlatt->latt->def;
    for (int curr_index = 0; curr_index < num_points; curr_index++, bp++) {
      if (bp->f1 & SELECT) {
        if (par1 == INDEX_UNSET) {
          par1 = curr_index;
        }
        else if (par2 == INDEX_UNSET) {
          par2 = curr_index;
        }
        else if (par3 == INDEX_UNSET) {
          par3 = curr_index;
        }
        else if (par4 == INDEX_UNSET) {
          par4 = curr_index;
        }
        else {
          break;
        }
      }
    }
  }

  if (par4 != INDEX_UNSET || par1 == INDEX_UNSET || (par2 != INDEX_UNSET && par3 == INDEX_UNSET)) {
    BKE_report(op->reports, RPT_ERROR, "Select either 1 or 3 vertices to parent to");
    return OPERATOR_CANCELLED;
  }

  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    if (ob != obedit) {
      DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
      par = obedit->parent;

      if (BKE_object_parent_loop_check(par, ob)) {
        BKE_report(op->reports, RPT_ERROR, "Loop in parents");
      }
      else {
        Object workob;
        BKE_view_layer_synced_ensure(scene, view_layer);
        ob->parent = BKE_view_layer_active_object_get(view_layer);
        if (par3 != INDEX_UNSET) {
          ob->partype = PARVERT3;
          ob->par1 = par1;
          ob->par2 = par2;
          ob->par3 = par3;

          /* inverse parent matrix */
          BKE_object_workob_calc_parent(depsgraph, scene, ob, &workob);
          invert_m4_m4(ob->parentinv, workob.object_to_world);
        }
        else {
          ob->partype = PARVERT1;
          ob->par1 = par1;

          /* inverse parent matrix */
          BKE_object_workob_calc_parent(depsgraph, scene, ob, &workob);
          invert_m4_m4(ob->parentinv, workob.object_to_world);
        }
      }
    }
  }
  CTX_DATA_END;

  DEG_relations_tag_update(bmain);

  WM_event_add_notifier(C, NC_OBJECT, NULL);

  return OPERATOR_FINISHED;

#undef INDEX_UNSET
}

void OBJECT_OT_vertex_parent_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Vertex Parent";
  ot->description = "Parent selected objects to the selected vertices";
  ot->idname = "OBJECT_OT_vertex_parent_set";

  /* api callbacks */
  ot->invoke = WM_operator_confirm_or_exec;
  ot->poll = vertex_parent_set_poll;
  ot->exec = vertex_parent_set_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  WM_operator_properties_confirm_or_exec(ot);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Clear Parent Operator
 * \{ */

EnumPropertyItem prop_clear_parent_types[] = {
    {CLEAR_PARENT_ALL,
     "CLEAR",
     0,
     "Clear Parent",
     "Completely clear the parenting relationship, including involved modifiers if any"},
    {CLEAR_PARENT_KEEP_TRANSFORM,
     "CLEAR_KEEP_TRANSFORM",
     0,
     "Clear and Keep Transformation",
     "As 'Clear Parent', but keep the current visual transformations of the object"},
    {CLEAR_PARENT_INVERSE,
     "CLEAR_INVERSE",
     0,
     "Clear Parent Inverse",
     "Reset the transform corrections applied to the parenting relationship, does not remove "
     "parenting itself"},
    {0, NULL, 0, NULL, NULL},
};

/* Helper for ED_object_parent_clear() - Remove deform-modifiers associated with parent */
static void object_remove_parent_deform_modifiers(Object *ob, const Object *par)
{
  if (ELEM(par->type, OB_ARMATURE, OB_LATTICE, OB_CURVES_LEGACY)) {
    ModifierData *md, *mdn;

    /* assume that we only need to remove the first instance of matching deform modifier here */
    for (md = ob->modifiers.first; md; md = mdn) {
      bool free = false;

      mdn = md->next;

      /* need to match types (modifier + parent) and references */
      if ((md->type == eModifierType_Armature) && (par->type == OB_ARMATURE)) {
        ArmatureModifierData *amd = (ArmatureModifierData *)md;
        if (amd->object == par) {
          free = true;
        }
      }
      else if ((md->type == eModifierType_Lattice) && (par->type == OB_LATTICE)) {
        LatticeModifierData *lmd = (LatticeModifierData *)md;
        if (lmd->object == par) {
          free = true;
        }
      }
      else if ((md->type == eModifierType_Curve) && (par->type == OB_CURVES_LEGACY)) {
        CurveModifierData *cmd = (CurveModifierData *)md;
        if (cmd->object == par) {
          free = true;
        }
      }

      /* free modifier if match */
      if (free) {
        BKE_modifier_remove_from_list(ob, md);
        BKE_modifier_free(md);
      }
    }
  }
}

void ED_object_parent_clear(Object *ob, const int type)
{
  if (ob->parent == NULL) {
    return;
  }

  switch (type) {
    case CLEAR_PARENT_ALL: {
      /* for deformers, remove corresponding modifiers to prevent
       * a large number of modifiers building up */
      object_remove_parent_deform_modifiers(ob, ob->parent);

      /* clear parenting relationship completely */
      ob->parent = NULL;
      ob->partype = PAROBJECT;
      ob->parsubstr[0] = 0;
      break;
    }
    case CLEAR_PARENT_KEEP_TRANSFORM: {
      /* remove parent, and apply the parented transform
       * result as object's local transforms */
      ob->parent = NULL;
      BKE_object_apply_mat4(ob, ob->object_to_world, true, false);
      break;
    }
    case CLEAR_PARENT_INVERSE: {
      /* object stays parented, but the parent inverse
       * (i.e. offset from parent to retain binding state)
       * is cleared. In other words: nothing to do here! */
      break;
    }
  }

  /* Always clear parentinv matrix for sake of consistency, see #41950. */
  unit_m4(ob->parentinv);

  DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
}

/* NOTE: poll should check for editable scene. */
static int parent_clear_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const int type = RNA_enum_get(op->ptr, "type");

  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    ED_object_parent_clear(ob, type);
  }
  CTX_DATA_END;

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
  WM_event_add_notifier(C, NC_OBJECT | ND_PARENT, NULL);
  return OPERATOR_FINISHED;
}

void OBJECT_OT_parent_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Parent";
  ot->description = "Clear the object's parenting";
  ot->idname = "OBJECT_OT_parent_clear";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = parent_clear_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna, "type", prop_clear_parent_types, CLEAR_PARENT_ALL, "Type", "");
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Make Parent Operator
 * \{ */

void ED_object_parent(Object *ob, Object *par, const int type, const char *substr)
{
  /* Always clear parentinv matrix for sake of consistency, see #41950. */
  unit_m4(ob->parentinv);

  if (!par || BKE_object_parent_loop_check(par, ob)) {
    ob->parent = NULL;
    ob->partype = PAROBJECT;
    ob->parsubstr[0] = 0;
    return;
  }

  /* Other partypes are deprecated, do not use here! */
  BLI_assert(ELEM(type & PARTYPE, PAROBJECT, PARSKEL, PARVERT1, PARVERT3, PARBONE));

  /* this could use some more checks */

  ob->parent = par;
  ob->partype &= ~PARTYPE;
  ob->partype |= type;
  STRNCPY(ob->parsubstr, substr);
}

EnumPropertyItem prop_make_parent_types[] = {
    {PAR_OBJECT, "OBJECT", 0, "Object", ""},
    {PAR_ARMATURE, "ARMATURE", 0, "Armature Deform", ""},
    {PAR_ARMATURE_NAME, "ARMATURE_NAME", 0, "   With Empty Groups", ""},
    {PAR_ARMATURE_AUTO, "ARMATURE_AUTO", 0, "   With Automatic Weights", ""},
    {PAR_ARMATURE_ENVELOPE, "ARMATURE_ENVELOPE", 0, "   With Envelope Weights", ""},
    {PAR_BONE, "BONE", 0, "Bone", ""},
    {PAR_BONE_RELATIVE, "BONE_RELATIVE", 0, "Bone Relative", ""},
    {PAR_CURVE, "CURVE", 0, "Curve Deform", ""},
    {PAR_FOLLOW, "FOLLOW", 0, "Follow Path", ""},
    {PAR_PATH_CONST, "PATH_CONST", 0, "Path Constraint", ""},
    {PAR_LATTICE, "LATTICE", 0, "Lattice Deform", ""},
    {PAR_VERTEX, "VERTEX", 0, "Vertex", ""},
    {PAR_VERTEX_TRI, "VERTEX_TRI", 0, "Vertex (Triangle)", ""},
    {0, NULL, 0, NULL, NULL},
};

bool ED_object_parent_set(ReportList *reports,
                          const bContext *C,
                          Scene *scene,
                          Object *const ob,
                          Object *const par,
                          int partype,
                          const bool xmirror,
                          const bool keep_transform,
                          const int vert_par[3])
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  bPoseChannel *pchan = NULL;
  bPoseChannel *pchan_eval = NULL;
  Object *parent_eval = DEG_get_evaluated_object(depsgraph, par);

  DEG_id_tag_update(&par->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  /* Preconditions. */
  if (ob == par) {
    /* Parenting an object to itself is impossible. */
    return false;
  }

  if (BKE_object_parent_loop_check(par, ob)) {
    BKE_report(reports, RPT_ERROR, "Loop in parents");
    return false;
  }

  switch (partype) {
    case PAR_FOLLOW:
    case PAR_PATH_CONST: {
      if (par->type != OB_CURVES_LEGACY) {
        return false;
      }
      Curve *cu = par->data;
      Curve *cu_eval = parent_eval->data;
      if ((cu->flag & CU_PATH) == 0) {
        cu->flag |= CU_PATH | CU_FOLLOW;
        cu_eval->flag |= CU_PATH | CU_FOLLOW;
        /* force creation of path data */
        BKE_displist_make_curveTypes(depsgraph, scene, par, false);
      }
      else {
        cu->flag |= CU_FOLLOW;
        cu_eval->flag |= CU_FOLLOW;
      }

      /* if follow, add F-Curve for ctime (i.e. "eval_time") so that path-follow works */
      if (partype == PAR_FOLLOW) {
        /* get or create F-Curve */
        bAction *act = ED_id_action_ensure(bmain, &cu->id);
        FCurve *fcu = ED_action_fcurve_ensure(bmain, act, NULL, NULL, "eval_time", 0);

        /* setup dummy 'generator' modifier here to get 1-1 correspondence still working */
        if (!fcu->bezt && !fcu->fpt && !fcu->modifiers.first) {
          add_fmodifier(&fcu->modifiers, FMODIFIER_TYPE_GENERATOR, fcu);
        }
      }

      /* fall back on regular parenting now (for follow only) */
      if (partype == PAR_FOLLOW) {
        partype = PAR_OBJECT;
      }
      break;
    }
    case PAR_BONE:
    case PAR_BONE_RELATIVE:
      pchan = BKE_pose_channel_active_if_layer_visible(par);
      pchan_eval = BKE_pose_channel_active_if_layer_visible(parent_eval);

      if (pchan == NULL || pchan_eval == NULL) {
        /* If pchan_eval is NULL, pchan should also be NULL. */
        BLI_assert_msg(pchan == NULL, "Missing evaluated bone data");
        BKE_report(reports, RPT_ERROR, "No active bone");
        return false;
      }
  }

  Object workob;

  /* Apply transformation of previous parenting. */
  if (keep_transform) {
    /* Was removed because of bug #23577,
     * but this can be handy in some cases too #32616, so make optional. */
    BKE_object_apply_mat4(ob, ob->object_to_world, false, false);
  }

  /* Set the parent (except for follow-path constraint option). */
  if (partype != PAR_PATH_CONST) {
    ob->parent = par;
    /* Always clear parentinv matrix for sake of consistency, see #41950. */
    unit_m4(ob->parentinv);
    DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
  }

  /* Handle types. */
  if (pchan) {
    STRNCPY(ob->parsubstr, pchan->name);
  }
  else {
    ob->parsubstr[0] = 0;
  }

  switch (partype) {
    case PAR_PATH_CONST:
      /* Don't do anything here, since this is not technically "parenting". */
      break;
    case PAR_CURVE:
    case PAR_LATTICE:
    case PAR_ARMATURE:
    case PAR_ARMATURE_NAME:
    case PAR_ARMATURE_ENVELOPE:
    case PAR_ARMATURE_AUTO:
      /* partype is now set to PAROBJECT so that invisible 'virtual'
       * modifiers don't need to be created.
       * NOTE: the old (2.4x) method was to set ob->partype = PARSKEL,
       * creating the virtual modifiers.
       */
      ob->partype = PAROBJECT;     /* NOTE: DNA define, not operator property. */
      /* ob->partype = PARSKEL; */ /* NOTE: DNA define, not operator property. */

      /* BUT, to keep the deforms, we need a modifier,
       * and then we need to set the object that it uses
       * - We need to ensure that the modifier we're adding doesn't already exist,
       *   so we check this by assuming that the parent is selected too.
       */
      /* XXX currently this should only happen for meshes, curves, surfaces,
       * and lattices - this stuff isn't available for meta-balls yet. */
      if (ELEM(ob->type, OB_MESH, OB_CURVES_LEGACY, OB_SURF, OB_FONT, OB_LATTICE)) {
        ModifierData *md;

        switch (partype) {
          case PAR_CURVE: /* curve deform */
            if (BKE_modifiers_is_deformed_by_curve(ob) != par) {
              md = ED_object_modifier_add(reports, bmain, scene, ob, NULL, eModifierType_Curve);
              if (md) {
                ((CurveModifierData *)md)->object = par;
              }
              if (par->runtime.curve_cache &&
                  par->runtime.curve_cache->anim_path_accum_length == NULL) {
                DEG_id_tag_update(&par->id, ID_RECALC_GEOMETRY);
              }
            }
            break;
          case PAR_LATTICE: /* lattice deform */
            if (BKE_modifiers_is_deformed_by_lattice(ob) != par) {
              md = ED_object_modifier_add(reports, bmain, scene, ob, NULL, eModifierType_Lattice);
              if (md) {
                ((LatticeModifierData *)md)->object = par;
              }
            }
            break;
          default: /* armature deform */
            if (BKE_modifiers_is_deformed_by_armature(ob) != par) {
              md = ED_object_modifier_add(reports, bmain, scene, ob, NULL, eModifierType_Armature);
              if (md) {
                ((ArmatureModifierData *)md)->object = par;
              }
            }
            break;
        }
      }
      break;
    case PAR_BONE:
      ob->partype = PARBONE; /* NOTE: DNA define, not operator property. */
      if (pchan->bone) {
        pchan->bone->flag &= ~BONE_RELATIVE_PARENTING;
        pchan_eval->bone->flag &= ~BONE_RELATIVE_PARENTING;
      }
      break;
    case PAR_BONE_RELATIVE:
      ob->partype = PARBONE; /* NOTE: DNA define, not operator property. */
      if (pchan->bone) {
        pchan->bone->flag |= BONE_RELATIVE_PARENTING;
        pchan_eval->bone->flag |= BONE_RELATIVE_PARENTING;
      }
      break;
    case PAR_VERTEX:
      ob->partype = PARVERT1;
      ob->par1 = vert_par[0];
      break;
    case PAR_VERTEX_TRI:
      ob->partype = PARVERT3;
      copy_v3_v3_int(&ob->par1, vert_par);
      break;
    case PAR_OBJECT:
    case PAR_FOLLOW:
      ob->partype = PAROBJECT; /* NOTE: DNA define, not operator property. */
      break;
  }

  /* Constraint and set parent inverse. */
  const bool is_armature_parent = ELEM(
      partype, PAR_ARMATURE, PAR_ARMATURE_NAME, PAR_ARMATURE_ENVELOPE, PAR_ARMATURE_AUTO);
  if (partype == PAR_PATH_CONST) {
    bConstraint *con;
    bFollowPathConstraint *data;
    float cmat[4][4], vec[3];

    con = BKE_constraint_add_for_object(ob, "AutoPath", CONSTRAINT_TYPE_FOLLOWPATH);

    data = con->data;
    data->tar = par;

    BKE_constraint_target_matrix_get(
        depsgraph, scene, con, 0, CONSTRAINT_OBTYPE_OBJECT, NULL, cmat, scene->r.cfra);
    sub_v3_v3v3(vec, ob->object_to_world[3], cmat[3]);

    copy_v3_v3(ob->loc, vec);
  }
  else if (is_armature_parent && (ob->type == OB_MESH) && (par->type == OB_ARMATURE)) {
    if (partype == PAR_ARMATURE_NAME) {
      ED_object_vgroup_calc_from_armature(
          reports, depsgraph, scene, ob, par, ARM_GROUPS_NAME, false);
    }
    else if (partype == PAR_ARMATURE_ENVELOPE) {
      ED_object_vgroup_calc_from_armature(
          reports, depsgraph, scene, ob, par, ARM_GROUPS_ENVELOPE, xmirror);
    }
    else if (partype == PAR_ARMATURE_AUTO) {
      WM_cursor_wait(true);
      ED_object_vgroup_calc_from_armature(
          reports, depsgraph, scene, ob, par, ARM_GROUPS_AUTO, xmirror);
      WM_cursor_wait(false);
    }
    /* get corrected inverse */
    ob->partype = PAROBJECT;
    BKE_object_workob_calc_parent(depsgraph, scene, ob, &workob);

    invert_m4_m4(ob->parentinv, workob.object_to_world);
  }
  else if (is_armature_parent && (ob->type == OB_GPENCIL_LEGACY) && (par->type == OB_ARMATURE)) {
    if (partype == PAR_ARMATURE) {
      ED_gpencil_add_armature(C, reports, ob, par);
    }
    else if (partype == PAR_ARMATURE_NAME) {
      ED_gpencil_add_armature_weights(C, reports, ob, par, GP_PAR_ARMATURE_NAME);
    }
    else if (ELEM(partype, PAR_ARMATURE_AUTO, PAR_ARMATURE_ENVELOPE)) {
      WM_cursor_wait(true);
      ED_gpencil_add_armature_weights(C, reports, ob, par, GP_PAR_ARMATURE_AUTO);
      WM_cursor_wait(false);
    }
    /* get corrected inverse */
    ob->partype = PAROBJECT;
    BKE_object_workob_calc_parent(depsgraph, scene, ob, &workob);

    invert_m4_m4(ob->parentinv, workob.object_to_world);
  }
  else if ((ob->type == OB_GPENCIL_LEGACY) && (par->type == OB_LATTICE)) {
    /* Add Lattice modifier */
    if (partype == PAR_LATTICE) {
      ED_gpencil_add_lattice_modifier(C, reports, ob, par);
    }
    /* get corrected inverse */
    ob->partype = PAROBJECT;
    BKE_object_workob_calc_parent(depsgraph, scene, ob, &workob);

    invert_m4_m4(ob->parentinv, workob.object_to_world);
  }
  else {
    /* calculate inverse parent matrix */
    BKE_object_workob_calc_parent(depsgraph, scene, ob, &workob);
    invert_m4_m4(ob->parentinv, workob.object_to_world);
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  return true;
}

static void parent_set_vert_find(KDTree_3d *tree, Object *child, int vert_par[3], bool is_tri)
{
  const float *co_find = child->object_to_world[3];
  if (is_tri) {
    KDTreeNearest_3d nearest[3];
    int tot;

    tot = BLI_kdtree_3d_find_nearest_n(tree, co_find, nearest, 3);
    BLI_assert(tot == 3);
    UNUSED_VARS(tot);

    vert_par[0] = nearest[0].index;
    vert_par[1] = nearest[1].index;
    vert_par[2] = nearest[2].index;

    BLI_assert(min_iii(UNPACK3(vert_par)) >= 0);
  }
  else {
    vert_par[0] = BLI_kdtree_3d_find_nearest(tree, co_find, NULL);
    BLI_assert(vert_par[0] >= 0);
    vert_par[1] = 0;
    vert_par[2] = 0;
  }
}

struct ParentingContext {
  ReportList *reports;
  Scene *scene;
  Object *par;
  int partype;
  bool is_vertex_tri;
  bool xmirror;
  bool keep_transform;
};

static bool parent_set_nonvertex_parent(bContext *C, struct ParentingContext *parenting_context)
{
  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    if (ob == parenting_context->par) {
      /* ED_object_parent_set() will fail (and thus return false), but this case shouldn't break
       * this loop. It's expected that the active object is also selected. */
      continue;
    }

    if (!ED_object_parent_set(parenting_context->reports,
                              C,
                              parenting_context->scene,
                              ob,
                              parenting_context->par,
                              parenting_context->partype,
                              parenting_context->xmirror,
                              parenting_context->keep_transform,
                              NULL))
    {
      return false;
    }
  }
  CTX_DATA_END;

  return true;
}

static bool parent_set_vertex_parent_with_kdtree(bContext *C,
                                                 struct ParentingContext *parenting_context,
                                                 struct KDTree_3d *tree)
{
  int vert_par[3] = {0, 0, 0};

  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    if (ob == parenting_context->par) {
      /* ED_object_parent_set() will fail (and thus return false), but this case shouldn't break
       * this loop. It's expected that the active object is also selected. */
      continue;
    }

    parent_set_vert_find(tree, ob, vert_par, parenting_context->is_vertex_tri);
    if (!ED_object_parent_set(parenting_context->reports,
                              C,
                              parenting_context->scene,
                              ob,
                              parenting_context->par,
                              parenting_context->partype,
                              parenting_context->xmirror,
                              parenting_context->keep_transform,
                              vert_par))
    {
      return false;
    }
  }
  CTX_DATA_END;
  return true;
}

static bool parent_set_vertex_parent(bContext *C, struct ParentingContext *parenting_context)
{
  struct KDTree_3d *tree = NULL;
  int tree_tot;

  tree = BKE_object_as_kdtree(parenting_context->par, &tree_tot);
  BLI_assert(tree != NULL);

  if (tree_tot < (parenting_context->is_vertex_tri ? 3 : 1)) {
    BKE_report(parenting_context->reports, RPT_ERROR, "Not enough vertices for vertex-parent");
    BLI_kdtree_3d_free(tree);
    return false;
  }

  const bool ok = parent_set_vertex_parent_with_kdtree(C, parenting_context, tree);
  BLI_kdtree_3d_free(tree);
  return ok;
}

static int parent_set_exec(bContext *C, wmOperator *op)
{
  const int partype = RNA_enum_get(op->ptr, "type");
  struct ParentingContext parenting_context = {
      .reports = op->reports,
      .scene = CTX_data_scene(C),
      .par = ED_object_active_context(C),
      .partype = partype,
      .is_vertex_tri = partype == PAR_VERTEX_TRI,
      .xmirror = RNA_boolean_get(op->ptr, "xmirror"),
      .keep_transform = RNA_boolean_get(op->ptr, "keep_transform"),
  };

  bool ok;
  if (ELEM(parenting_context.partype, PAR_VERTEX, PAR_VERTEX_TRI)) {
    ok = parent_set_vertex_parent(C, &parenting_context);
  }
  else {
    ok = parent_set_nonvertex_parent(C, &parenting_context);
  }
  if (!ok) {
    return OPERATOR_CANCELLED;
  }

  Main *bmain = CTX_data_main(C);
  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
  WM_event_add_notifier(C, NC_OBJECT | ND_PARENT, NULL);

  return OPERATOR_FINISHED;
}

static int parent_set_invoke_menu(bContext *C, wmOperatorType *ot)
{
  Object *parent = ED_object_active_context(C);
  uiPopupMenu *pup = UI_popup_menu_begin(C, IFACE_("Set Parent To"), ICON_NONE);
  uiLayout *layout = UI_popup_menu_layout(pup);

  PointerRNA opptr;
#if 0
  uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_OBJECT);
#else
  uiItemFullO_ptr(layout, ot, IFACE_("Object"), ICON_NONE, NULL, WM_OP_EXEC_DEFAULT, 0, &opptr);
  RNA_enum_set(&opptr, "type", PAR_OBJECT);
  RNA_boolean_set(&opptr, "keep_transform", false);

  uiItemFullO_ptr(layout,
                  ot,
                  IFACE_("Object (Keep Transform)"),
                  ICON_NONE,
                  NULL,
                  WM_OP_EXEC_DEFAULT,
                  0,
                  &opptr);
  RNA_enum_set(&opptr, "type", PAR_OBJECT);
  RNA_boolean_set(&opptr, "keep_transform", true);
#endif

  uiItemBooleanO(layout,
                 IFACE_("Object (Without Inverse)"),
                 ICON_NONE,
                 "OBJECT_OT_parent_no_inverse_set",
                 "keep_transform",
                 0);

  uiItemBooleanO(layout,
                 IFACE_("Object (Keep Transform Without Inverse)"),
                 ICON_NONE,
                 "OBJECT_OT_parent_no_inverse_set",
                 "keep_transform",
                 1);

  struct {
    bool mesh, gpencil, curves;
  } has_children_of_type = {0};

  CTX_DATA_BEGIN (C, Object *, child, selected_editable_objects) {
    if (child == parent) {
      continue;
    }
    if (child->type == OB_MESH) {
      has_children_of_type.mesh = true;
    }
    if (child->type == OB_GPENCIL_LEGACY) {
      has_children_of_type.gpencil = true;
    }
    if (child->type == OB_CURVES) {
      has_children_of_type.curves = true;
    }
  }
  CTX_DATA_END;

  if (parent->type == OB_ARMATURE) {
    uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_ARMATURE);
    uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_ARMATURE_NAME);
    if (!has_children_of_type.gpencil) {
      uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_ARMATURE_ENVELOPE);
    }
    if (has_children_of_type.mesh || has_children_of_type.gpencil) {
      uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_ARMATURE_AUTO);
    }
    uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_BONE);
    uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_BONE_RELATIVE);
  }
  else if (parent->type == OB_CURVES_LEGACY) {
    uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_CURVE);
    uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_FOLLOW);
    uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_PATH_CONST);
  }
  else if (parent->type == OB_LATTICE) {
    uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_LATTICE);
  }
  else if (parent->type == OB_MESH) {
    if (has_children_of_type.curves) {
      uiItemO(layout, "Object (Attach Curves to Surface)", ICON_NONE, "CURVES_OT_surface_set");
    }
  }

  /* vertex parenting */
  if (OB_TYPE_SUPPORT_PARVERT(parent->type)) {
    uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_VERTEX);
    uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_VERTEX_TRI);
  }

  UI_popup_menu_end(C, pup);

  return OPERATOR_INTERFACE;
}

static int parent_set_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  if (RNA_property_is_set(op->ptr, op->type->prop)) {
    return parent_set_exec(C, op);
  }
  return parent_set_invoke_menu(C, op->type);
}

static bool parent_set_poll_property(const bContext *UNUSED(C),
                                     wmOperator *op,
                                     const PropertyRNA *prop)
{
  const char *prop_id = RNA_property_identifier(prop);

  /* Only show XMirror for PAR_ARMATURE_ENVELOPE and PAR_ARMATURE_AUTO! */
  if (STREQ(prop_id, "xmirror")) {
    const int type = RNA_enum_get(op->ptr, "type");
    if (ELEM(type, PAR_ARMATURE_ENVELOPE, PAR_ARMATURE_AUTO)) {
      return true;
    }
    return false;
  }

  return true;
}

void OBJECT_OT_parent_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Parent";
  ot->description = "Set the object's parenting";
  ot->idname = "OBJECT_OT_parent_set";

  /* api callbacks */
  ot->invoke = parent_set_invoke;
  ot->exec = parent_set_exec;
  ot->poll = ED_operator_object_active;
  ot->poll_property = parent_set_poll_property;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna, "type", prop_make_parent_types, 0, "Type", "");
  RNA_def_boolean(
      ot->srna,
      "xmirror",
      false,
      "X Mirror",
      "Apply weights symmetrically along X axis, for Envelope/Automatic vertex groups creation");
  RNA_def_boolean(ot->srna,
                  "keep_transform",
                  false,
                  "Keep Transform",
                  "Apply transformation before parenting");
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Make Parent Without Inverse Operator
 * \{ */

static int parent_noinv_set_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *par = ED_object_active_context(C);

  const bool keep_transform = RNA_boolean_get(op->ptr, "keep_transform");

  DEG_id_tag_update(&par->id, ID_RECALC_TRANSFORM);

  /* context iterator */
  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    if (ob != par) {
      if (BKE_object_parent_loop_check(par, ob)) {
        BKE_report(op->reports, RPT_ERROR, "Loop in parents");
      }
      else {
        /* set recalc flags */
        DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

        /* set parenting type for object - object only... */
        ob->parent = par;
        ob->partype = PAROBJECT; /* NOTE: DNA define, not operator property. */

        if (keep_transform) {
          BKE_object_apply_parent_inverse(ob);
          continue;
        }

        /* clear inverse matrix and also the object location */
        unit_m4(ob->parentinv);
        memset(ob->loc, 0, sizeof(float[3]));
      }
    }
  }
  CTX_DATA_END;

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
  WM_event_add_notifier(C, NC_OBJECT | ND_PARENT, NULL);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_parent_no_inverse_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Parent without Inverse";
  ot->description = "Set the object's parenting without setting the inverse parent correction";
  ot->idname = "OBJECT_OT_parent_no_inverse_set";

  /* api callbacks */
  ot->invoke = WM_operator_confirm_or_exec;
  ot->exec = parent_noinv_set_exec;
  ot->poll = ED_operator_object_active_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  WM_operator_properties_confirm_or_exec(ot);

  RNA_def_boolean(ot->srna,
                  "keep_transform",
                  false,
                  "Keep Transform",
                  "Preserve the world transform throughout parenting");
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Clear Track Operator
 * \{ */

enum {
  CLEAR_TRACK = 1,
  CLEAR_TRACK_KEEP_TRANSFORM = 2,
};

static const EnumPropertyItem prop_clear_track_types[] = {
    {CLEAR_TRACK, "CLEAR", 0, "Clear Track", ""},
    {CLEAR_TRACK_KEEP_TRANSFORM,
     "CLEAR_KEEP_TRANSFORM",
     0,
     "Clear and Keep Transformation (Clear Track)",
     ""},
    {0, NULL, 0, NULL, NULL},
};

/* NOTE: poll should check for editable scene. */
static int object_track_clear_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const int type = RNA_enum_get(op->ptr, "type");

  if (CTX_data_edit_object(C)) {
    BKE_report(op->reports, RPT_ERROR, "Operation cannot be performed in edit mode");
    return OPERATOR_CANCELLED;
  }
  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    bConstraint *con, *pcon;

    /* remove track-object for old track */
    ob->track = NULL;
    DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);

    /* also remove all tracking constraints */
    for (con = ob->constraints.last; con; con = pcon) {
      pcon = con->prev;
      if (ELEM(con->type,
               CONSTRAINT_TYPE_TRACKTO,
               CONSTRAINT_TYPE_LOCKTRACK,
               CONSTRAINT_TYPE_DAMPTRACK)) {
        BKE_constraint_remove(&ob->constraints, con);
      }
    }

    if (type == CLEAR_TRACK_KEEP_TRANSFORM) {
      BKE_object_apply_mat4(ob, ob->object_to_world, true, true);
    }
  }
  CTX_DATA_END;

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_track_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Track";
  ot->description = "Clear tracking constraint or flag from object";
  ot->idname = "OBJECT_OT_track_clear";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = object_track_clear_exec;

  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna, "type", prop_clear_track_types, 0, "Type", "");
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Make Track Operator
 * \{ */

enum {
  CREATE_TRACK_DAMPTRACK = 1,
  CREATE_TRACK_TRACKTO = 2,
  CREATE_TRACK_LOCKTRACK = 3,
};

static const EnumPropertyItem prop_make_track_types[] = {
    {CREATE_TRACK_DAMPTRACK, "DAMPTRACK", 0, "Damped Track Constraint", ""},
    {CREATE_TRACK_TRACKTO, "TRACKTO", 0, "Track to Constraint", ""},
    {CREATE_TRACK_LOCKTRACK, "LOCKTRACK", 0, "Lock Track Constraint", ""},
    {0, NULL, 0, NULL, NULL},
};

static int track_set_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *obact = ED_object_active_context(C);

  const int type = RNA_enum_get(op->ptr, "type");

  switch (type) {
    case CREATE_TRACK_DAMPTRACK: {
      bConstraint *con;
      bDampTrackConstraint *data;

      CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
        if (ob != obact) {
          con = BKE_constraint_add_for_object(ob, "AutoTrack", CONSTRAINT_TYPE_DAMPTRACK);

          data = con->data;
          data->tar = obact;
          DEG_id_tag_update(&ob->id,
                            ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);

          /* Light, Camera and Speaker track differently by default */
          if (ELEM(ob->type, OB_LAMP, OB_CAMERA, OB_SPEAKER)) {
            data->trackflag = TRACK_nZ;
          }
        }
      }
      CTX_DATA_END;
      break;
    }
    case CREATE_TRACK_TRACKTO: {
      bConstraint *con;
      bTrackToConstraint *data;

      CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
        if (ob != obact) {
          con = BKE_constraint_add_for_object(ob, "AutoTrack", CONSTRAINT_TYPE_TRACKTO);

          data = con->data;
          data->tar = obact;
          DEG_id_tag_update(&ob->id,
                            ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);

          /* Light, Camera and Speaker track differently by default */
          if (ELEM(ob->type, OB_LAMP, OB_CAMERA, OB_SPEAKER)) {
            data->reserved1 = TRACK_nZ;
            data->reserved2 = UP_Y;
          }
        }
      }
      CTX_DATA_END;
      break;
    }
    case CREATE_TRACK_LOCKTRACK: {
      bConstraint *con;
      bLockTrackConstraint *data;

      CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
        if (ob != obact) {
          con = BKE_constraint_add_for_object(ob, "AutoTrack", CONSTRAINT_TYPE_LOCKTRACK);

          data = con->data;
          data->tar = obact;
          DEG_id_tag_update(&ob->id,
                            ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);

          /* Light, Camera and Speaker track differently by default */
          if (ELEM(ob->type, OB_LAMP, OB_CAMERA, OB_SPEAKER)) {
            data->trackflag = TRACK_nZ;
            data->lockflag = LOCK_Y;
          }
        }
      }
      CTX_DATA_END;
      break;
    }
  }

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_track_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Track";
  ot->description = "Make the object track another object, using various methods/constraints";
  ot->idname = "OBJECT_OT_track_set";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = track_set_exec;

  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", prop_make_track_types, 0, "Type", "");
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Link to Scene Operator
 * \{ */

#if 0
static void link_to_scene(Main *UNUSED(bmain), ushort UNUSED(nr))
{
  Scene *sce = (Scene *)BLI_findlink(&bmain->scene, G.curscreen->scenenr - 1);
  Base *base, *nbase;

  if (sce == NULL) {
    return;
  }
  if (sce->id.lib) {
    return;
  }

  for (base = FIRSTBASE; base; base = base->next) {
    if (BASE_SELECTED(v3d, base)) {
      nbase = MEM_mallocN(sizeof(Base), "newbase");
      *nbase = *base;
      BLI_addhead(&(sce->base), nbase);
      id_us_plus((ID *)base->object);
    }
  }
}
#endif

static int make_links_scene_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene_to = BLI_findlink(&bmain->scenes, RNA_enum_get(op->ptr, "scene"));

  if (scene_to == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Could not find scene");
    return OPERATOR_CANCELLED;
  }

  if (scene_to == CTX_data_scene(C)) {
    BKE_report(op->reports, RPT_ERROR, "Cannot link objects into the same scene");
    return OPERATOR_CANCELLED;
  }

  if (!BKE_id_is_editable(bmain, &scene_to->id)) {
    BKE_report(op->reports, RPT_ERROR, "Cannot link objects into a linked scene");
    return OPERATOR_CANCELLED;
  }

  Collection *collection_to = scene_to->master_collection;
  CTX_DATA_BEGIN (C, Base *, base, selected_bases) {
    BKE_collection_object_add(bmain, collection_to, base->object);
  }
  CTX_DATA_END;

  DEG_relations_tag_update(bmain);

  /* redraw the 3D view because the object center points are colored differently */
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, NULL);

  /* one day multiple scenes will be visible, then we should have some update function for them
   */
  return OPERATOR_FINISHED;
}

enum {
  MAKE_LINKS_OBDATA = 1,
  MAKE_LINKS_MATERIALS = 2,
  MAKE_LINKS_ANIMDATA = 3,
  MAKE_LINKS_GROUP = 4,
  MAKE_LINKS_DUPLICOLLECTION = 5,
  MAKE_LINKS_MODIFIERS = 6,
  MAKE_LINKS_FONTS = 7,
  MAKE_LINKS_SHADERFX = 8,
};

/* Return true if make link data is allowed, false otherwise */
static bool allow_make_links_data(const int type, Object *ob_src, Object *ob_dst)
{
  switch (type) {
    case MAKE_LINKS_OBDATA:
      if (ob_src->type == ob_dst->type && ob_src->type != OB_EMPTY) {
        return true;
      }
      break;
    case MAKE_LINKS_MATERIALS:
      if (OB_TYPE_SUPPORT_MATERIAL(ob_src->type) && OB_TYPE_SUPPORT_MATERIAL(ob_dst->type) &&
          /* Linking non-grease-pencil materials to a grease-pencil object causes issues.
           * We make sure that if one of the objects is a grease-pencil object, the other must be
           * as well. */
          ((ob_src->type == OB_GPENCIL_LEGACY) == (ob_dst->type == OB_GPENCIL_LEGACY)))
      {
        return true;
      }
      break;
    case MAKE_LINKS_DUPLICOLLECTION:
      if (ob_dst->type == OB_EMPTY) {
        return true;
      }
      break;
    case MAKE_LINKS_ANIMDATA:
    case MAKE_LINKS_GROUP:
      return true;
    case MAKE_LINKS_MODIFIERS:
      if (!ELEM(OB_EMPTY, ob_src->type, ob_dst->type)) {
        return true;
      }
      break;
    case MAKE_LINKS_FONTS:
      if ((ob_src->data != ob_dst->data) && (ob_src->type == OB_FONT) && (ob_dst->type == OB_FONT))
      {
        return true;
      }
      break;
    case MAKE_LINKS_SHADERFX:
      if ((ob_src->type == OB_GPENCIL_LEGACY) && (ob_dst->type == OB_GPENCIL_LEGACY)) {
        return true;
      }
      break;
  }
  return false;
}

static int make_links_data_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Main *bmain = CTX_data_main(C);
  const int type = RNA_enum_get(op->ptr, "type");
  Object *ob_src;
  ID *obdata_id;
  int a;

  /* collection */
  LinkNode *ob_collections = NULL;
  bool is_cycle = false;
  bool is_lib = false;

  ob_src = ED_object_active_context(C);

  /* avoid searching all collections in source object each time */
  if (type == MAKE_LINKS_GROUP) {
    ob_collections = BKE_object_groups(bmain, scene, ob_src);
  }

  CTX_DATA_BEGIN (C, Base *, base_dst, selected_editable_bases) {
    Object *ob_dst = base_dst->object;

    if (ob_src != ob_dst) {
      if (allow_make_links_data(type, ob_src, ob_dst)) {
        obdata_id = ob_dst->data;

        switch (type) {
          case MAKE_LINKS_OBDATA: /* obdata */
            id_us_min(obdata_id);

            obdata_id = ob_src->data;
            id_us_plus(obdata_id);
            ob_dst->data = obdata_id;

            /* if amount of material indices changed: */
            BKE_object_materials_test(bmain, ob_dst, ob_dst->data);

            DEG_id_tag_update(&ob_dst->id, ID_RECALC_GEOMETRY);
            break;
          case MAKE_LINKS_MATERIALS:
            /* new approach, using functions from kernel */
            for (a = 0; a < ob_src->totcol; a++) {
              Material *ma = BKE_object_material_get(ob_src, a + 1);
              /* also works with `ma == NULL` */
              BKE_object_material_assign(bmain, ob_dst, ma, a + 1, BKE_MAT_ASSIGN_USERPREF);
            }
            DEG_id_tag_update(&ob_dst->id, ID_RECALC_GEOMETRY);
            break;
          case MAKE_LINKS_ANIMDATA:
            BKE_animdata_copy_id(bmain, (ID *)ob_dst, (ID *)ob_src, 0);
            if (ob_dst->data && ob_src->data) {
              if (!BKE_id_is_editable(bmain, obdata_id)) {
                is_lib = true;
                break;
              }
              BKE_animdata_copy_id(bmain, (ID *)ob_dst->data, (ID *)ob_src->data, 0);
            }
            DEG_id_tag_update(&ob_dst->id,
                              ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
            break;
          case MAKE_LINKS_GROUP: {
            LinkNode *collection_node;

            /* first clear collections */
            BKE_object_groups_clear(bmain, scene, ob_dst);

            /* now add in the collections from the link nodes */
            for (collection_node = ob_collections; collection_node;
                 collection_node = collection_node->next) {
              if (ob_dst->instance_collection != collection_node->link) {
                BKE_collection_object_add(bmain, collection_node->link, ob_dst);
              }
              else {
                is_cycle = true;
              }
            }
            break;
          }
          case MAKE_LINKS_DUPLICOLLECTION:
            ob_dst->instance_collection = ob_src->instance_collection;
            if (ob_dst->instance_collection) {
              id_us_plus(&ob_dst->instance_collection->id);
              ob_dst->transflag |= OB_DUPLICOLLECTION;
            }
            DEG_id_tag_update(&ob_dst->id, ID_RECALC_COPY_ON_WRITE);
            break;
          case MAKE_LINKS_MODIFIERS:
            BKE_object_link_modifiers(ob_dst, ob_src);
            DEG_id_tag_update(&ob_dst->id,
                              ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
            break;
          case MAKE_LINKS_FONTS: {
            Curve *cu_src = ob_src->data;
            Curve *cu_dst = ob_dst->data;

            if (!BKE_id_is_editable(bmain, obdata_id)) {
              is_lib = true;
              break;
            }

            if (cu_dst->vfont) {
              id_us_min(&cu_dst->vfont->id);
            }
            cu_dst->vfont = cu_src->vfont;
            id_us_plus((ID *)cu_dst->vfont);
            if (cu_dst->vfontb) {
              id_us_min(&cu_dst->vfontb->id);
            }
            cu_dst->vfontb = cu_src->vfontb;
            id_us_plus((ID *)cu_dst->vfontb);
            if (cu_dst->vfonti) {
              id_us_min(&cu_dst->vfonti->id);
            }
            cu_dst->vfonti = cu_src->vfonti;
            id_us_plus((ID *)cu_dst->vfonti);
            if (cu_dst->vfontbi) {
              id_us_min(&cu_dst->vfontbi->id);
            }
            cu_dst->vfontbi = cu_src->vfontbi;
            id_us_plus((ID *)cu_dst->vfontbi);

            DEG_id_tag_update(&ob_dst->id,
                              ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
            break;
          }
          case MAKE_LINKS_SHADERFX:
            ED_object_shaderfx_link(ob_dst, ob_src);
            DEG_id_tag_update(&ob_dst->id,
                              ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
            break;
        }
      }
    }
  }
  CTX_DATA_END;

  if (type == MAKE_LINKS_GROUP) {
    if (ob_collections) {
      BLI_linklist_free(ob_collections, NULL);
    }

    if (is_cycle) {
      BKE_report(op->reports, RPT_WARNING, "Skipped some collections because of cycle detected");
    }
  }

  if (is_lib) {
    BKE_report(op->reports, RPT_WARNING, "Skipped editing library object data");
  }

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, NULL);
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, CTX_wm_view3d(C));
  WM_event_add_notifier(C, NC_OBJECT, NULL);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_make_links_scene(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Link Objects to Scene";
  ot->description = "Link selection to another scene";
  ot->idname = "OBJECT_OT_make_links_scene";

  /* api callbacks */
  ot->invoke = WM_enum_search_invoke;
  ot->exec = make_links_scene_exec;
  /* better not run the poll check */

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_enum(ot->srna, "scene", DummyRNA_NULL_items, 0, "Scene", "");
  RNA_def_enum_funcs(prop, RNA_scene_local_itemf);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
}

void OBJECT_OT_make_links_data(wmOperatorType *ot)
{
  static const EnumPropertyItem make_links_items[] = {
      {MAKE_LINKS_OBDATA, "OBDATA", 0, "Link Object Data", "Replace assigned Object Data"},
      {MAKE_LINKS_MATERIALS, "MATERIAL", 0, "Link Materials", "Replace assigned Materials"},
      {MAKE_LINKS_ANIMDATA,
       "ANIMATION",
       0,
       "Link Animation Data",
       "Replace assigned Animation Data"},
      {MAKE_LINKS_GROUP, "GROUPS", 0, "Link Collections", "Replace assigned Collections"},
      {MAKE_LINKS_DUPLICOLLECTION,
       "DUPLICOLLECTION",
       0,
       "Link Instance Collection",
       "Replace assigned Collection Instance"},
      {MAKE_LINKS_FONTS, "FONTS", 0, "Link Fonts to Text", "Replace Text object Fonts"},
      RNA_ENUM_ITEM_SEPR,
      {MAKE_LINKS_MODIFIERS, "MODIFIERS", 0, "Copy Modifiers", "Replace Modifiers"},
      {MAKE_LINKS_SHADERFX,
       "EFFECTS",
       0,
       "Copy Grease Pencil Effects",
       "Replace Grease Pencil Effects"},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Link/Transfer Data";
  ot->description = "Transfer data from active object to selected objects";
  ot->idname = "OBJECT_OT_make_links_data";

  /* api callbacks */
  ot->exec = make_links_data_exec;
  ot->poll = ED_operator_object_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", make_links_items, 0, "Type", "");
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Make Single User Operator
 * \{ */

static bool single_data_needs_duplication(ID *id)
{
  /* NOTE: When dealing with linked data, we always make a local copy of it.
   * While in theory we could rather make it local when it only has one user, this is difficult
   * in practice with current code of this function. */
  return (id != NULL && (id->us > 1 || ID_IS_LINKED(id)));
}

static void libblock_relink_collection(Main *bmain,
                                       Collection *collection,
                                       const bool do_collection)
{
  if (do_collection) {
    BKE_libblock_relink_to_newid(bmain, &collection->id, 0);
  }

  for (CollectionObject *cob = collection->gobject.first; cob != NULL; cob = cob->next) {
    BKE_libblock_relink_to_newid(bmain, &cob->ob->id, 0);
  }

  LISTBASE_FOREACH (CollectionChild *, child, &collection->children) {
    libblock_relink_collection(bmain, child->collection, true);
  }
}

static Collection *single_object_users_collection(Main *bmain,
                                                  Scene *scene,
                                                  Collection *collection,
                                                  const int flag,
                                                  const bool copy_collections,
                                                  const bool is_master_collection)
{
  /* Generate new copies for objects in given collection and all its children,
   * and optionally also copy collections themselves. */
  if (copy_collections && !is_master_collection) {
    Collection *collection_new = (Collection *)BKE_id_copy_ex(
        bmain, &collection->id, NULL, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS);
    id_us_min(&collection_new->id);
    collection = ID_NEW_SET(collection, collection_new);
  }

  /* We do not remap to new objects here, this is done in separate step. */
  LISTBASE_FOREACH (CollectionObject *, cob, &collection->gobject) {
    Object *ob = cob->ob;
    /* an object may be in more than one collection */
    if ((ob->id.newid == NULL) && ((ob->flag & flag) == flag)) {
      if (!ID_IS_LINKED(ob) && BKE_object_scenes_users_get(bmain, ob) > 1) {
        ID_NEW_SET(
            ob, BKE_id_copy_ex(bmain, &ob->id, NULL, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
        id_us_min(ob->id.newid);
      }
    }
  }

  /* Since master collection has already be duplicated as part of scene copy,
   * we do not duplicate it here.
   * However, this means its children need to be re-added manually here,
   * otherwise their parent lists are empty (which will lead to crashes, see #63101). */
  CollectionChild *child_next, *child = collection->children.first;
  CollectionChild *orig_child_last = collection->children.last;
  for (; child != NULL; child = child_next) {
    child_next = child->next;
    Collection *collection_child_new = single_object_users_collection(
        bmain, scene, child->collection, flag, copy_collections, false);

    if (is_master_collection && copy_collections && child->collection != collection_child_new) {
      /* We do not want a collection sync here, our collections are in a complete uninitialized
       * state currently. With current code, that would lead to a memory leak - because of
       * reasons. It would be a useless loss of computing anyway, since caller has to fully
       * refresh view-layers/collections caching at the end. */
      BKE_collection_child_add_no_sync(collection, collection_child_new);
      BLI_remlink(&collection->children, child);
      MEM_freeN(child);
      if (child == orig_child_last) {
        break;
      }
    }
  }

  return collection;
}

/* Warning, sets ID->newid pointers of objects and collections, but does not clear them. */
static void single_object_users(
    Main *bmain, Scene *scene, View3D *v3d, const int flag, const bool copy_collections)
{
  /* duplicate all the objects of the scene (and matching collections, if required). */
  Collection *master_collection = scene->master_collection;
  single_object_users_collection(bmain, scene, master_collection, flag, copy_collections, true);

  /* Will also handle the master collection. */
  BKE_libblock_relink_to_newid(bmain, &scene->id, 0);

  /* Collection and object pointers in collections */
  libblock_relink_collection(bmain, scene->master_collection, false);

  /* We also have to handle runtime things in UI. */
  if (v3d) {
    ID_NEW_REMAP(v3d->camera);
  }

  /* Making single user may affect other scenes if they share
   * with current one some collections in their ViewLayer. */
  BKE_main_collection_sync_remap(bmain);
}

void ED_object_single_user(Main *bmain, Scene *scene, Object *ob)
{
  FOREACH_SCENE_OBJECT_BEGIN (scene, ob_iter) {
    ob_iter->flag &= ~OB_DONE;
  }
  FOREACH_SCENE_OBJECT_END;

  /* tag only the one object */
  ob->flag |= OB_DONE;

  single_object_users(bmain, scene, NULL, OB_DONE, false);
  BKE_main_id_newptr_and_tag_clear(bmain);
}

static void single_obdata_users(
    Main *bmain, Scene *scene, ViewLayer *view_layer, View3D *v3d, const int flag)
{
  Light *la;
  Curve *cu;
  Camera *cam;
  Mesh *me;
  Lattice *lat;
  ID *id;

  FOREACH_OBJECT_FLAG_BEGIN (scene, view_layer, v3d, flag, ob) {
    if (BKE_id_is_editable(bmain, &ob->id)) {
      id = ob->data;
      if (single_data_needs_duplication(id)) {
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);

        switch (ob->type) {
          case OB_EMPTY:
            ob->data = ID_NEW_SET(
                ob->data,
                BKE_id_copy_ex(bmain, ob->data, NULL, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            break;
          case OB_LAMP:
            ob->data = la = ID_NEW_SET(
                ob->data,
                BKE_id_copy_ex(bmain, ob->data, NULL, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            break;
          case OB_CAMERA:
            cam = ob->data = ID_NEW_SET(
                ob->data,
                BKE_id_copy_ex(bmain, ob->data, NULL, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            ID_NEW_REMAP(cam->dof.focus_object);
            break;
          case OB_MESH:
            /* Needed to remap texcomesh below. */
            me = ob->data = ID_NEW_SET(
                ob->data,
                BKE_id_copy_ex(bmain, ob->data, NULL, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            break;
          case OB_MBALL:
            ob->data = ID_NEW_SET(
                ob->data,
                BKE_id_copy_ex(bmain, ob->data, NULL, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            break;
          case OB_CURVES_LEGACY:
          case OB_SURF:
          case OB_FONT:
            ob->data = cu = ID_NEW_SET(
                ob->data,
                BKE_id_copy_ex(bmain, ob->data, NULL, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            ID_NEW_REMAP(cu->bevobj);
            ID_NEW_REMAP(cu->taperobj);
            break;
          case OB_LATTICE:
            ob->data = lat = ID_NEW_SET(
                ob->data,
                BKE_id_copy_ex(bmain, ob->data, NULL, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            break;
          case OB_ARMATURE:
            DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
            ob->data = ID_NEW_SET(
                ob->data,
                BKE_id_copy_ex(bmain, ob->data, NULL, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            BKE_pose_rebuild(bmain, ob, ob->data, true);
            break;
          case OB_SPEAKER:
            ob->data = ID_NEW_SET(
                ob->data,
                BKE_id_copy_ex(bmain, ob->data, NULL, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            break;
          case OB_LIGHTPROBE:
            ob->data = ID_NEW_SET(
                ob->data,
                BKE_id_copy_ex(bmain, ob->data, NULL, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            break;
          case OB_GPENCIL_LEGACY:
            ob->data = ID_NEW_SET(
                ob->data,
                BKE_id_copy_ex(bmain, ob->data, NULL, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            break;
          case OB_CURVES:
            ob->data = ID_NEW_SET(
                ob->data,
                BKE_id_copy_ex(bmain, ob->data, NULL, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            break;
          case OB_POINTCLOUD:
            ob->data = ID_NEW_SET(
                ob->data,
                BKE_id_copy_ex(bmain, ob->data, NULL, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            break;
          case OB_VOLUME:
            ob->data = ID_NEW_SET(
                ob->data,
                BKE_id_copy_ex(bmain, ob->data, NULL, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            break;
          case OB_GREASE_PENCIL:
            ob->data = ID_NEW_SET(
                ob->data,
                BKE_id_copy_ex(bmain, ob->data, NULL, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS));
            break;
          default:
            printf("ERROR %s: can't copy %s\n", __func__, id->name);
            BLI_assert_msg(0, "This should never happen.");

            /* We need to end the FOREACH_OBJECT_FLAG_BEGIN iterator to prevent memory leak. */
            BKE_scene_objects_iterator_end(&iter_macro);
            return;
        }

        id_us_min(id);
      }
    }
  }
  FOREACH_OBJECT_FLAG_END;

  me = bmain->meshes.first;
  while (me) {
    ID_NEW_REMAP(me->texcomesh);
    me = me->id.next;
  }
}

void ED_object_single_obdata_user(Main *bmain, Scene *scene, Object *ob)
{
  FOREACH_SCENE_OBJECT_BEGIN (scene, ob_iter) {
    ob_iter->flag &= ~OB_DONE;
  }
  FOREACH_SCENE_OBJECT_END;

  /* Tag only the one object. */
  ob->flag |= OB_DONE;

  single_obdata_users(bmain, scene, NULL, NULL, OB_DONE);
}

static void single_object_action_users(
    Main *bmain, Scene *scene, ViewLayer *view_layer, View3D *v3d, const int flag)
{
  FOREACH_OBJECT_FLAG_BEGIN (scene, view_layer, v3d, flag, ob) {
    if (BKE_id_is_editable(bmain, &ob->id)) {
      AnimData *adt = BKE_animdata_from_id(&ob->id);
      if (adt == NULL) {
        continue;
      }

      ID *id_act = (ID *)adt->action;
      if (single_data_needs_duplication(id_act)) {
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
        BKE_animdata_duplicate_id_action(bmain, &ob->id, USER_DUP_ACT | USER_DUP_LINKED_ID);
      }
    }
  }
  FOREACH_OBJECT_FLAG_END;
}

static void single_objectdata_action_users(
    Main *bmain, Scene *scene, ViewLayer *view_layer, View3D *v3d, const int flag)
{
  FOREACH_OBJECT_FLAG_BEGIN (scene, view_layer, v3d, flag, ob) {
    if (BKE_id_is_editable(bmain, &ob->id) && ob->data != NULL) {
      ID *id_obdata = (ID *)ob->data;
      AnimData *adt = BKE_animdata_from_id(id_obdata);
      if (adt == NULL) {
        continue;
      }

      ID *id_act = (ID *)adt->action;
      if (single_data_needs_duplication(id_act)) {
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
        BKE_animdata_duplicate_id_action(bmain, id_obdata, USER_DUP_ACT | USER_DUP_LINKED_ID);
      }
    }
  }
  FOREACH_OBJECT_FLAG_END;
}

static void single_mat_users(
    Main *bmain, Scene *scene, ViewLayer *view_layer, View3D *v3d, const int flag)
{
  Material *ma, *man;
  int a;

  FOREACH_OBJECT_FLAG_BEGIN (scene, view_layer, v3d, flag, ob) {
    if (BKE_id_is_editable(bmain, &ob->id)) {
      for (a = 1; a <= ob->totcol; a++) {
        ma = BKE_object_material_get(ob, (short)a);
        if (single_data_needs_duplication(&ma->id)) {
          man = (Material *)BKE_id_copy_ex(
              bmain, &ma->id, NULL, LIB_ID_COPY_DEFAULT | LIB_ID_COPY_ACTIONS);
          man->id.us = 0;
          BKE_object_material_assign(bmain, ob, man, (short)a, BKE_MAT_ASSIGN_USERPREF);
        }
      }
    }
  }
  FOREACH_OBJECT_FLAG_END;
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Make Local Operator
 * \{ */

enum {
  MAKE_LOCAL_SELECT_OB = 1,
  MAKE_LOCAL_SELECT_OBDATA = 2,
  MAKE_LOCAL_SELECT_OBDATA_MATERIAL = 3,
  MAKE_LOCAL_ALL = 4,
};

static int tag_localizable_looper(LibraryIDLinkCallbackData *cb_data)
{
  ID **id_pointer = cb_data->id_pointer;
  if (*id_pointer) {
    (*id_pointer)->tag &= ~LIB_TAG_DOIT;
  }

  return IDWALK_RET_NOP;
}

static void tag_localizable_objects(bContext *C, const int mode)
{
  Main *bmain = CTX_data_main(C);

  BKE_main_id_tag_all(bmain, LIB_TAG_DOIT, false);

  /* Set LIB_TAG_DOIT flag for all selected objects, so next we can check whether
   * object is gonna to become local or not.
   */
  CTX_DATA_BEGIN (C, Object *, object, selected_objects) {
    object->id.tag |= LIB_TAG_DOIT;

    /* If obdata is also going to become local, mark it as such too. */
    if (mode == MAKE_LOCAL_SELECT_OBDATA && object->data) {
      ID *data_id = (ID *)object->data;
      data_id->tag |= LIB_TAG_DOIT;
    }
  }
  CTX_DATA_END;

  /* Also forbid making objects local if other library objects are using
   * them for modifiers or constraints.
   */
  for (Object *object = bmain->objects.first; object; object = object->id.next) {
    if ((object->id.tag & LIB_TAG_DOIT) == 0) {
      BKE_library_foreach_ID_link(
          NULL, &object->id, tag_localizable_looper, NULL, IDWALK_READONLY);
    }
    if (object->data) {
      ID *data_id = (ID *)object->data;
      if ((data_id->tag & LIB_TAG_DOIT) == 0) {
        BKE_library_foreach_ID_link(NULL, data_id, tag_localizable_looper, NULL, IDWALK_READONLY);
      }
    }
  }

  /* TODO(sergey): Drivers targets? */
}

/**
 * Instance indirectly referenced zero user objects,
 * otherwise they're lost on reload, see #40595.
 */
static bool make_local_all__instance_indirect_unused(Main *bmain,
                                                     const Scene *scene,
                                                     ViewLayer *view_layer,
                                                     Collection *collection)
{
  Object *ob;
  bool changed = false;

  for (ob = bmain->objects.first; ob; ob = ob->id.next) {
    if (ID_IS_LINKED(ob) && (ob->id.us == 0)) {
      Base *base;

      id_us_plus(&ob->id);

      BKE_collection_object_add(bmain, collection, ob);
      BKE_view_layer_synced_ensure(scene, view_layer);
      base = BKE_view_layer_base_find(view_layer, ob);
      ED_object_base_select(base, BA_SELECT);
      DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);

      changed = true;
    }
  }

  return changed;
}

static void make_local_animdata_tag_strips(ListBase *strips)
{
  NlaStrip *strip;

  for (strip = strips->first; strip; strip = strip->next) {
    if (strip->act) {
      strip->act->id.tag &= ~LIB_TAG_PRE_EXISTING;
    }

    make_local_animdata_tag_strips(&strip->strips);
  }
}

/* Tag all actions used by given animdata to be made local. */
static void make_local_animdata_tag(AnimData *adt)
{
  if (adt) {
    /* Actions - Active and Temp */
    if (adt->action) {
      adt->action->id.tag &= ~LIB_TAG_PRE_EXISTING;
    }
    if (adt->tmpact) {
      adt->tmpact->id.tag &= ~LIB_TAG_PRE_EXISTING;
    }

    /* Drivers */
    /* TODO: need to handle the ID-targets too? */

    /* NLA Data */
    LISTBASE_FOREACH (NlaTrack *, nlt, &adt->nla_tracks) {
      make_local_animdata_tag_strips(&nlt->strips);
    }
  }
}

static void make_local_material_tag(Material *ma)
{
  if (ma) {
    ma->id.tag &= ~LIB_TAG_PRE_EXISTING;
    make_local_animdata_tag(BKE_animdata_from_id(&ma->id));

    /* About nodetrees: root one is made local together with material,
     * others we keep linked for now... */
  }
}

static int make_local_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  ParticleSystem *psys;
  Material *ma, ***matarar;
  const int mode = RNA_enum_get(op->ptr, "type");
  int a;

  /* NOTE: we (ab)use LIB_TAG_PRE_EXISTING to cherry pick which ID to make local... */
  if (mode == MAKE_LOCAL_ALL) {
    const Scene *scene = CTX_data_scene(C);
    ViewLayer *view_layer = CTX_data_view_layer(C);
    Collection *collection = CTX_data_collection(C);

    BKE_main_id_tag_all(bmain, LIB_TAG_PRE_EXISTING, false);

    /* De-select so the user can differentiate newly instanced from existing objects. */
    BKE_view_layer_base_deselect_all(scene, view_layer);

    if (make_local_all__instance_indirect_unused(bmain, scene, view_layer, collection)) {
      BKE_report(op->reports,
                 RPT_INFO,
                 "Orphan library objects added to the current scene to avoid loss");
    }
  }
  else {
    BKE_main_id_tag_all(bmain, LIB_TAG_PRE_EXISTING, true);
    tag_localizable_objects(C, mode);

    CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
      if ((ob->id.tag & LIB_TAG_DOIT) == 0) {
        continue;
      }

      ob->id.tag &= ~LIB_TAG_PRE_EXISTING;
      make_local_animdata_tag(BKE_animdata_from_id(&ob->id));
      for (psys = ob->particlesystem.first; psys; psys = psys->next) {
        psys->part->id.tag &= ~LIB_TAG_PRE_EXISTING;
      }

      if (mode == MAKE_LOCAL_SELECT_OBDATA_MATERIAL) {
        for (a = 0; a < ob->totcol; a++) {
          ma = ob->mat[a];
          if (ma) {
            make_local_material_tag(ma);
          }
        }

        matarar = BKE_object_material_array_p(ob);
        if (matarar) {
          for (a = 0; a < ob->totcol; a++) {
            ma = (*matarar)[a];
            if (ma) {
              make_local_material_tag(ma);
            }
          }
        }
      }

      if (ELEM(mode, MAKE_LOCAL_SELECT_OBDATA, MAKE_LOCAL_SELECT_OBDATA_MATERIAL) &&
          ob->data != NULL) {
        ID *ob_data = ob->data;
        ob_data->tag &= ~LIB_TAG_PRE_EXISTING;
        make_local_animdata_tag(BKE_animdata_from_id(ob_data));
      }
    }
    CTX_DATA_END;
  }

  BKE_library_make_local(bmain, NULL, NULL, true, false); /* NULL is all libraries. */

  WM_event_add_notifier(C, NC_WINDOW, NULL);
  return OPERATOR_FINISHED;
}

void OBJECT_OT_make_local(wmOperatorType *ot)
{
  static const EnumPropertyItem type_items[] = {
      {MAKE_LOCAL_SELECT_OB, "SELECT_OBJECT", 0, "Selected Objects", ""},
      {MAKE_LOCAL_SELECT_OBDATA, "SELECT_OBDATA", 0, "Selected Objects and Data", ""},
      {MAKE_LOCAL_SELECT_OBDATA_MATERIAL,
       "SELECT_OBDATA_MATERIAL",
       0,
       "Selected Objects, Data and Materials",
       ""},
      {MAKE_LOCAL_ALL, "ALL", 0, "All", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Make Local";
  ot->description = "Make library linked data-blocks local to this file";
  ot->idname = "OBJECT_OT_make_local";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = make_local_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", type_items, 0, "Type", "");
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Make Library Override Operator
 * \{ */

static bool make_override_library_object_overridable_check(Main *bmain, Object *object)
{
  /* An object is actually overridable only if it is in at least one local collection.
   * Unfortunately 'direct link' flag is not enough here. */
  LISTBASE_FOREACH (Collection *, collection, &bmain->collections) {
    if (!ID_IS_LINKED(collection) && BKE_collection_has_object(collection, object)) {
      return true;
    }
  }
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    if (!ID_IS_LINKED(scene) && BKE_collection_has_object(scene->master_collection, object)) {
      return true;
    }
  }
  return false;
}

static int make_override_library_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *obact = CTX_data_active_object(C);
  ID *id_root = NULL;
  bool is_override_instancing_object = false;

  bool user_overrides_from_selected_objects = false;

  if (!ID_IS_LINKED(obact) && obact->instance_collection != NULL &&
      ID_IS_LINKED(obact->instance_collection))
  {
    if (!ID_IS_OVERRIDABLE_LIBRARY(obact->instance_collection)) {
      BKE_reportf(op->reports,
                  RPT_ERROR_INVALID_INPUT,
                  "Collection '%s' (instantiated by the active object) is not overridable",
                  obact->instance_collection->id.name + 2);
      return OPERATOR_CANCELLED;
    }

    id_root = &obact->instance_collection->id;
    is_override_instancing_object = true;
    user_overrides_from_selected_objects = false;
  }
  else if (!make_override_library_object_overridable_check(bmain, obact)) {
    const int i = RNA_property_int_get(op->ptr, op->type->prop);
    const uint collection_session_uuid = *((const uint *)&i);
    if (collection_session_uuid == MAIN_ID_SESSION_UUID_UNSET) {
      BKE_reportf(op->reports,
                  RPT_ERROR_INVALID_INPUT,
                  "Could not find an overridable root hierarchy for object '%s'",
                  obact->id.name + 2);
      return OPERATOR_CANCELLED;
    }
    Collection *collection = BLI_listbase_bytes_find(&bmain->collections,
                                                     &collection_session_uuid,
                                                     sizeof(collection_session_uuid),
                                                     offsetof(ID, session_uuid));
    id_root = &collection->id;
    user_overrides_from_selected_objects = true;
  }
  /* Else, poll func ensures us that ID_IS_LINKED(obact) is true, or that it is already an existing
   * liboverride. */
  else {
    BLI_assert(ID_IS_LINKED(obact) || ID_IS_OVERRIDE_LIBRARY_REAL(obact));
    id_root = &obact->id;
    user_overrides_from_selected_objects = true;
  }

  /* Make already existing selected liboverrides editable. */
  bool is_active_override = false;
  FOREACH_SELECTED_OBJECT_BEGIN (view_layer, CTX_wm_view3d(C), ob_iter) {
    if (ID_IS_OVERRIDE_LIBRARY_REAL(ob_iter) && !ID_IS_LINKED(ob_iter)) {
      ob_iter->id.override_library->flag &= ~LIBOVERRIDE_FLAG_SYSTEM_DEFINED;
      is_active_override = is_active_override || (&ob_iter->id == id_root);
      DEG_id_tag_update(&ob_iter->id, ID_RECALC_COPY_ON_WRITE);
    }
  }
  FOREACH_SELECTED_OBJECT_END;
  /* If the active object is a liboverride, there is no point going further, since in the weird
   * case where some other selected objects would be linked ones, there is no way to properly
   * create overrides for them currently.
   *
   * Could be added later if really needed, but would rather avoid that extra complexity here. */
  if (is_active_override) {
    return OPERATOR_FINISHED;
  }

  const bool do_fully_editable = !user_overrides_from_selected_objects;

  GSet *user_overrides_objects_uids = do_fully_editable ? NULL :
                                                          BLI_gset_new(BLI_ghashutil_inthash_p,
                                                                       BLI_ghashutil_intcmp,
                                                                       __func__);

  if (do_fully_editable) {
    /* Pass. */
  }
  else if (user_overrides_from_selected_objects) {
    /* Only selected objects can be 'user overrides'. */
    FOREACH_SELECTED_OBJECT_BEGIN (view_layer, CTX_wm_view3d(C), ob_iter) {
      BLI_gset_add(user_overrides_objects_uids, POINTER_FROM_UINT(ob_iter->id.session_uuid));
    }
    FOREACH_SELECTED_OBJECT_END;
  }
  else {
    /* Only armatures inside the root collection (and their children) can be 'user overrides'. */
    FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN ((Collection *)id_root, ob_iter) {
      if (ob_iter->type == OB_ARMATURE) {
        BLI_gset_add(user_overrides_objects_uids, POINTER_FROM_UINT(ob_iter->id.session_uuid));
      }
    }
    FOREACH_COLLECTION_OBJECT_RECURSIVE_END;
  }

  BKE_main_id_tag_all(bmain, LIB_TAG_DOIT, false);

  /* For the time being, replace selected linked objects by their overrides in all collections.
   * While this may not be the absolute best behavior in all cases, in most common one this should
   * match the expected result. */
  if (user_overrides_objects_uids != NULL) {
    LISTBASE_FOREACH (Collection *, coll_iter, &bmain->collections) {
      if (ID_IS_LINKED(coll_iter)) {
        continue;
      }
      LISTBASE_FOREACH (CollectionObject *, coll_ob_iter, &coll_iter->gobject) {
        if (BLI_gset_haskey(user_overrides_objects_uids,
                            POINTER_FROM_UINT(coll_ob_iter->ob->id.session_uuid)))
        {
          /* Tag for remapping when creating overrides. */
          coll_iter->id.tag |= LIB_TAG_DOIT;
          break;
        }
      }
    }
  }

  ID *id_root_override;
  const bool success = BKE_lib_override_library_create(bmain,
                                                       scene,
                                                       view_layer,
                                                       NULL,
                                                       id_root,
                                                       id_root,
                                                       &obact->id,
                                                       &id_root_override,
                                                       do_fully_editable);

  if (!do_fully_editable) {
    /* Define liboverrides from selected/validated objects as user defined. */
    ID *id_hierarchy_root_override = id_root_override->override_library->hierarchy_root;
    ID *id_iter;
    FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
      if (ID_IS_LINKED(id_iter) || !ID_IS_OVERRIDE_LIBRARY_REAL(id_iter) ||
          id_iter->override_library->hierarchy_root != id_hierarchy_root_override)
      {
        continue;
      }
      if (BLI_gset_haskey(user_overrides_objects_uids,
                          POINTER_FROM_UINT(id_iter->override_library->reference->session_uuid)))
      {
        id_iter->override_library->flag &= ~LIBOVERRIDE_FLAG_SYSTEM_DEFINED;
      }
    }
    FOREACH_MAIN_ID_END;

    BLI_gset_free(user_overrides_objects_uids, NULL);
  }

  if (success) {
    if (is_override_instancing_object) {
      /* Remove the instance empty from this scene, the items now have an overridden collection
       * instead. */
      ED_object_base_free_and_unlink(bmain, scene, obact);
    }
    else {
      /* Remove the found root ID from the view layer. */
      switch (GS(id_root->name)) {
        case ID_GR: {
          Collection *collection_root = (Collection *)id_root;
          LISTBASE_FOREACH_MUTABLE (
              CollectionParent *, collection_parent, &collection_root->runtime.parents) {
            if (ID_IS_LINKED(collection_parent->collection) ||
                !BKE_view_layer_has_collection(view_layer, collection_parent->collection))
            {
              continue;
            }
            BKE_collection_child_remove(bmain, collection_parent->collection, collection_root);
          }
          break;
        }
        case ID_OB: {
          /* TODO: Not sure how well we can handle this case, when we don't have the collections as
           * reference containers... */
          break;
        }
        default:
          break;
      }
    }
  }

  DEG_id_tag_update(&CTX_data_scene(C)->id, ID_RECALC_BASE_FLAGS | ID_RECALC_COPY_ON_WRITE);
  WM_event_add_notifier(C, NC_WINDOW, NULL);
  WM_event_add_notifier(C, NC_WM | ND_LIB_OVERRIDE_CHANGED, NULL);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  return success ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

/* Set the object to override. */
static int make_override_library_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *obact = ED_object_active_context(C);

  /* Sanity checks. */
  if (!scene || ID_IS_LINKED(scene) || !obact) {
    return OPERATOR_CANCELLED;
  }

  if ((!ID_IS_LINKED(obact) && obact->instance_collection != NULL &&
       ID_IS_OVERRIDABLE_LIBRARY(obact->instance_collection)) ||
      make_override_library_object_overridable_check(bmain, obact))
  {
    return make_override_library_exec(C, op);
  }

  if (!ID_IS_LINKED(obact)) {
    if (ID_IS_OVERRIDE_LIBRARY_REAL(obact)) {
      return make_override_library_exec(C, op);
    }
    BKE_report(op->reports, RPT_ERROR, "Cannot make library override from a local object");
    return OPERATOR_CANCELLED;
  }

  int potential_root_collections_num = 0;
  uint collection_session_uuid = MAIN_ID_SESSION_UUID_UNSET;
  LISTBASE_FOREACH (Collection *, collection, &bmain->collections) {
    /* Only check for directly linked collections. */
    if (!ID_IS_LINKED(&collection->id) || (collection->id.tag & LIB_TAG_INDIRECT) != 0 ||
        !BKE_view_layer_has_collection(view_layer, collection))
    {
      continue;
    }
    if (BKE_collection_has_object_recursive(collection, obact)) {
      if (potential_root_collections_num == 0) {
        collection_session_uuid = collection->id.session_uuid;
      }
      potential_root_collections_num++;
    }
  }

  if (potential_root_collections_num <= 1) {
    RNA_property_int_set(op->ptr, op->type->prop, *((int *)&collection_session_uuid));
    return make_override_library_exec(C, op);
  }

  BKE_reportf(op->reports,
              RPT_ERROR,
              "Too many potential root collections (%d) for the override hierarchy, "
              "please use the Outliner instead",
              potential_root_collections_num);
  return OPERATOR_CANCELLED;
}

static bool make_override_library_poll(bContext *C)
{
  Object *obact = CTX_data_active_object(C);

  /* Object must be directly linked to be overridable. */
  return (
      ED_operator_objectmode(C) && obact != NULL &&
      (ID_IS_LINKED(obact) || ID_IS_OVERRIDE_LIBRARY(obact) ||
       (obact->instance_collection != NULL &&
        ID_IS_OVERRIDABLE_LIBRARY(obact->instance_collection) && !ID_IS_OVERRIDE_LIBRARY(obact))));
}

void OBJECT_OT_make_override_library(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Library Override";
  ot->description =
      "Create a local override of the selected linked objects, and their hierarchy of "
      "dependencies";
  ot->idname = "OBJECT_OT_make_override_library";

  /* api callbacks */
  ot->invoke = make_override_library_invoke;
  ot->exec = make_override_library_exec;
  ot->poll = make_override_library_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  PropertyRNA *prop;
  prop = RNA_def_int(ot->srna,
                     "collection",
                     MAIN_ID_SESSION_UUID_UNSET,
                     INT_MIN,
                     INT_MAX,
                     "Override Collection",
                     "Session UUID of the directly linked collection containing the selected "
                     "object, to make an override from",
                     INT_MIN,
                     INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  ot->prop = prop;
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Reset Library Override Operator
 * \{ */

static bool reset_clear_override_library_poll(bContext *C)
{
  Object *obact = CTX_data_active_object(C);

  /* Object must be local and an override. */
  return (ED_operator_objectmode(C) && obact != NULL && !ID_IS_LINKED(obact) &&
          ID_IS_OVERRIDE_LIBRARY(obact));
}

static int reset_override_library_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);

  /* Make already existing selected liboverrides editable. */
  FOREACH_SELECTED_OBJECT_BEGIN (CTX_data_view_layer(C), CTX_wm_view3d(C), ob_iter) {
    if (ID_IS_OVERRIDE_LIBRARY_REAL(ob_iter) && !ID_IS_LINKED(ob_iter)) {
      BKE_lib_override_library_id_reset(bmain, &ob_iter->id, false);
    }
  }
  FOREACH_SELECTED_OBJECT_END;

  WM_event_add_notifier(C, NC_WINDOW, NULL);
  WM_event_add_notifier(C, NC_WM | ND_LIB_OVERRIDE_CHANGED, NULL);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_reset_override_library(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reset Library Override";
  ot->description = "Reset the selected local overrides to their linked references values";
  ot->idname = "OBJECT_OT_reset_override_library";

  /* api callbacks */
  ot->exec = reset_override_library_exec;
  ot->poll = reset_clear_override_library_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Clear Library Override Operator
 * \{ */

static int clear_override_library_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Scene *scene = CTX_data_scene(C);
  LinkNode *todo_objects = NULL, *todo_object_iter;

  /* Make already existing selected liboverrides editable. */
  FOREACH_SELECTED_OBJECT_BEGIN (view_layer, CTX_wm_view3d(C), ob_iter) {
    if (ID_IS_LINKED(ob_iter)) {
      continue;
    }
    BLI_linklist_prepend_alloca(&todo_objects, ob_iter);
  }
  FOREACH_SELECTED_OBJECT_END;

  for (todo_object_iter = todo_objects; todo_object_iter != NULL;
       todo_object_iter = todo_object_iter->next)
  {
    Object *ob_iter = todo_object_iter->link;
    if (BKE_lib_override_library_is_hierarchy_leaf(bmain, &ob_iter->id)) {
      bool do_remap_active = false;
      BKE_view_layer_synced_ensure(scene, view_layer);
      if (BKE_view_layer_active_object_get(view_layer) == ob_iter) {
        do_remap_active = true;
      }
      BKE_libblock_remap(bmain,
                         &ob_iter->id,
                         ob_iter->id.override_library->reference,
                         ID_REMAP_SKIP_INDIRECT_USAGE);
      if (do_remap_active) {
        Object *ref_object = (Object *)ob_iter->id.override_library->reference;
        Base *basact = BKE_view_layer_base_find(view_layer, ref_object);
        if (basact != NULL) {
          view_layer->basact = basact;
        }
        DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
      }
      BKE_id_delete(bmain, &ob_iter->id);
    }
    else {
      BKE_lib_override_library_id_reset(bmain, &ob_iter->id, true);
    }
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS | ID_RECALC_COPY_ON_WRITE);
  WM_event_add_notifier(C, NC_WINDOW, NULL);
  WM_event_add_notifier(C, NC_WM | ND_LIB_OVERRIDE_CHANGED, NULL);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_clear_override_library(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Library Override";
  ot->description =
      "Delete the selected local overrides and relink their usages to the linked data-blocks if "
      "possible, else reset them and mark them as non editable";
  ot->idname = "OBJECT_OT_clear_override_library";

  /* api callbacks */
  ot->exec = clear_override_library_exec;
  ot->poll = reset_clear_override_library_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Make Single User Operator
 * \{ */

enum {
  MAKE_SINGLE_USER_ALL = 1,
  MAKE_SINGLE_USER_SELECTED = 2,
};

static int make_single_user_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C); /* ok if this is NULL */
  const int flag = (RNA_enum_get(op->ptr, "type") == MAKE_SINGLE_USER_SELECTED) ? SELECT : 0;
  const bool copy_collections = false;
  bool update_deps = false;

  if (RNA_boolean_get(op->ptr, "object")) {
    if (flag == SELECT) {
      BKE_view_layer_selected_objects_tag(scene, view_layer, OB_DONE);
      single_object_users(bmain, scene, v3d, OB_DONE, copy_collections);
    }
    else {
      single_object_users(bmain, scene, v3d, 0, copy_collections);
    }

    /* needed since object relationships may have changed */
    update_deps = true;
  }

  if (RNA_boolean_get(op->ptr, "obdata")) {
    single_obdata_users(bmain, scene, view_layer, v3d, flag);

    /* Needed since some IDs were remapped? (incl. me->texcomesh, see #73797). */
    update_deps = true;
  }

  if (RNA_boolean_get(op->ptr, "material")) {
    single_mat_users(bmain, scene, view_layer, v3d, flag);
  }

  if (RNA_boolean_get(op->ptr, "animation")) {
    single_object_action_users(bmain, scene, view_layer, v3d, flag);
  }

  if (RNA_boolean_get(op->ptr, "obdata_animation")) {
    single_objectdata_action_users(bmain, scene, view_layer, v3d, flag);
  }

  BKE_main_id_newptr_and_tag_clear(bmain);

  WM_event_add_notifier(C, NC_WINDOW, NULL);

  if (update_deps) {
    DEG_relations_tag_update(bmain);
  }

  return OPERATOR_FINISHED;
}

void OBJECT_OT_make_single_user(wmOperatorType *ot)
{
  static const EnumPropertyItem type_items[] = {
      {MAKE_SINGLE_USER_SELECTED, "SELECTED_OBJECTS", 0, "Selected Objects", ""},
      {MAKE_SINGLE_USER_ALL, "ALL", 0, "All", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Make Single User";
  ot->description = "Make linked data local to each object";
  ot->idname = "OBJECT_OT_make_single_user";

  /* Note that the invoke callback is only used from operator search,
   * otherwise this does nothing by default. */

  /* api callbacks */
  ot->invoke = WM_operator_props_popup_confirm;
  ot->exec = make_single_user_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", type_items, MAKE_SINGLE_USER_SELECTED, "Type", "");

  RNA_def_boolean(ot->srna, "object", 0, "Object", "Make single user objects");
  RNA_def_boolean(ot->srna, "obdata", 0, "Object Data", "Make single user object data");
  RNA_def_boolean(ot->srna, "material", 0, "Materials", "Make materials local to each data-block");
  RNA_def_boolean(ot->srna,
                  "animation",
                  0,
                  "Object Animation",
                  "Make object animation data local to each object");
  RNA_def_boolean(ot->srna,
                  "obdata_animation",
                  0,
                  "Object Data Animation",
                  "Make object data (mesh, curve etc.) animation data local to each object");
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Drop Named Material on Object Operator
 * \{ */

char *ED_object_ot_drop_named_material_tooltip(bContext *C, const char *name, const int mval[2])
{
  int mat_slot = 0;
  Object *ob = ED_view3d_give_material_slot_under_cursor(C, mval, &mat_slot);
  if (ob == NULL) {
    return BLI_strdup("");
  }
  mat_slot = max_ii(mat_slot, 1);

  Material *prev_mat = BKE_object_material_get(ob, mat_slot);

  char *result;
  if (prev_mat) {
    const char *tooltip = TIP_("Drop %s on %s (slot %d, replacing %s)");
    result = BLI_sprintfN(tooltip, name, ob->id.name + 2, mat_slot, prev_mat->id.name + 2);
  }
  else {
    const char *tooltip = TIP_("Drop %s on %s (slot %d)");
    result = BLI_sprintfN(tooltip, name, ob->id.name + 2, mat_slot);
  }
  return result;
}

static int drop_named_material_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Main *bmain = CTX_data_main(C);
  int mat_slot = 0;
  Object *ob = ED_view3d_give_material_slot_under_cursor(C, event->mval, &mat_slot);
  mat_slot = max_ii(mat_slot, 1);

  Material *ma = (Material *)WM_operator_properties_id_lookup_from_name_or_session_uuid(
      bmain, op->ptr, ID_MA);

  if (ob == NULL || ma == NULL) {
    return OPERATOR_CANCELLED;
  }

  BKE_object_material_assign(CTX_data_main(C), ob, ma, mat_slot, BKE_MAT_ASSIGN_USERPREF);

  DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);

  WM_event_add_notifier(C, NC_OBJECT | ND_OB_SHADING, ob);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, NULL);
  WM_event_add_notifier(C, NC_MATERIAL | ND_SHADING_LINKS, ma);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_drop_named_material(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Drop Named Material on Object";
  ot->idname = "OBJECT_OT_drop_named_material";

  /* api callbacks */
  ot->invoke = drop_named_material_invoke;
  ot->poll = ED_operator_objectmode_with_view3d_poll_msg;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  /* properties */
  WM_operator_properties_id_lookup(ot, true);
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Drop Geometry Nodes on Object Operator
 * \{ */

char *ED_object_ot_drop_geometry_nodes_tooltip(bContext *C,
                                               PointerRNA *properties,
                                               const int mval[2])
{
  const Object *ob = ED_view3d_give_object_under_cursor(C, mval);
  if (ob == NULL) {
    return BLI_strdup("");
  }

  const uint32_t session_uuid = RNA_int_get(properties, "session_uuid");
  const ID *id = BKE_libblock_find_session_uuid(CTX_data_main(C), ID_NT, session_uuid);
  if (!id) {
    return BLI_strdup("");
  }

  const char *tooltip = TIP_("Add modifier with node group \"%s\" on object \"%s\"");
  return BLI_sprintfN(tooltip, id->name, ob->id.name);
}

static bool check_geometry_node_group_sockets(wmOperator *op, const bNodeTree *tree)
{
  const bNodeSocket *first_input = (const bNodeSocket *)tree->inputs.first;
  if (!first_input) {
    BKE_report(op->reports, RPT_ERROR, "The node group must have a geometry input socket");
    return false;
  }
  if (first_input->type != SOCK_GEOMETRY) {
    BKE_report(op->reports, RPT_ERROR, "The first input must be a geometry socket");
    return false;
  }
  const bNodeSocket *first_output = (const bNodeSocket *)tree->outputs.first;
  if (!first_output) {
    BKE_report(op->reports, RPT_ERROR, "The node group must have a geometry output socket");
    return false;
  }
  if (first_output->type != SOCK_GEOMETRY) {
    BKE_report(op->reports, RPT_ERROR, "The first output must be a geometry socket");
    return false;
  }
  return true;
}

static int drop_geometry_nodes_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = ED_view3d_give_object_under_cursor(C, event->mval);
  if (!ob) {
    return OPERATOR_CANCELLED;
  }

  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);

  const uint32_t uuid = RNA_int_get(op->ptr, "session_uuid");
  bNodeTree *node_tree = (bNodeTree *)BKE_libblock_find_session_uuid(bmain, ID_NT, uuid);
  if (!node_tree) {
    return OPERATOR_CANCELLED;
  }
  if (node_tree->type != NTREE_GEOMETRY) {
    BKE_report(op->reports, RPT_ERROR, "Node group must be a geometry node tree");
    return OPERATOR_CANCELLED;
  }

  if (!check_geometry_node_group_sockets(op, node_tree)) {
    return OPERATOR_CANCELLED;
  }

  NodesModifierData *nmd = (NodesModifierData *)ED_object_modifier_add(
      op->reports, bmain, scene, ob, node_tree->id.name + 2, eModifierType_Nodes);
  if (!nmd) {
    BKE_report(op->reports, RPT_ERROR, "Could not add geometry nodes modifier");
    return OPERATOR_CANCELLED;
  }

  nmd->node_group = node_tree;
  id_us_plus(&node_tree->id);
  MOD_nodes_update_interface(ob, nmd);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, NULL);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_drop_geometry_nodes(wmOperatorType *ot)
{
  ot->name = "Drop Geometry Node Group on Object";
  ot->idname = "OBJECT_OT_drop_geometry_nodes";

  ot->invoke = drop_geometry_nodes_invoke;
  ot->poll = ED_operator_view3d_active;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  PropertyRNA *prop = RNA_def_int(ot->srna,
                                  "session_uuid",
                                  0,
                                  INT32_MIN,
                                  INT32_MAX,
                                  "Session UUID",
                                  "Session UUID of the geometry node group being dropped",
                                  INT32_MIN,
                                  INT32_MAX);
  RNA_def_property_flag(prop, (PropertyFlag)(PROP_HIDDEN | PROP_SKIP_SAVE));
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Unlink Object Operator
 * \{ */

static int object_unlink_data_exec(bContext *C, wmOperator *op)
{
  ID *id;
  PropertyPointerRNA pprop;

  UI_context_active_but_prop_get_templateID(C, &pprop.ptr, &pprop.prop);

  if (pprop.prop == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Incorrect context for running object data unlink");
    return OPERATOR_CANCELLED;
  }

  id = pprop.ptr.owner_id;

  if (GS(id->name) == ID_OB) {
    Object *ob = (Object *)id;
    if (ob->data) {
      ID *id_data = ob->data;

      if (GS(id_data->name) == ID_IM) {
        id_us_min(id_data);
        ob->data = NULL;
      }
      else {
        BKE_report(op->reports, RPT_ERROR, "Can't unlink this object data");
        return OPERATOR_CANCELLED;
      }
    }
  }

  RNA_property_update(C, &pprop.ptr, pprop.prop);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_unlink_data(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Unlink";
  ot->idname = "OBJECT_OT_unlink_data";

  /* api callbacks */
  ot->exec = object_unlink_data_exec;

  /* flags */
  ot->flag = OPTYPE_INTERNAL;
}

/** \} */
