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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

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
#include "DNA_gpencil_types.h"
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
#include "BKE_displist.h"
#include "BKE_editmesh.h"
#include "BKE_fcurve.h"
#include "BKE_gpencil.h"
#include "BKE_hair.h"
#include "BKE_idprop.h"
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
#include "ED_gpencil.h"
#include "ED_keyframing.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_view3d.h"

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
  BMVert *eve;
  BMIter iter;
  Nurb *nu;
  BezTriple *bezt;
  BPoint *bp;
  Object *par;
  int a, v1 = 0, v2 = 0, v3 = 0, v4 = 0, nr = 1;

  /* we need 1 to 3 selected vertices */

  if (obedit->type == OB_MESH) {
    Mesh *me = obedit->data;
    BMEditMesh *em;

    EDBM_mesh_load(bmain, obedit);
    EDBM_mesh_make(obedit, scene->toolsettings->selectmode, true);

    DEG_id_tag_update(obedit->data, 0);

    em = me->edit_mesh;

    EDBM_mesh_normals_update(em);
    BKE_editmesh_looptri_calc(em);

    /* Make sure the evaluated mesh is updated.
     *
     * Most reliable way is to update the tagged objects, which will ensure
     * proper copy-on-write update, but also will make sure all dependent
     * objects are also up to date. */
    BKE_scene_graph_update_tagged(depsgraph, bmain);

    BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
      if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
        if (v1 == 0) {
          v1 = nr;
        }
        else if (v2 == 0) {
          v2 = nr;
        }
        else if (v3 == 0) {
          v3 = nr;
        }
        else if (v4 == 0) {
          v4 = nr;
        }
        else {
          break;
        }
      }
      nr++;
    }
  }
  else if (ELEM(obedit->type, OB_SURF, OB_CURVE)) {
    ListBase *editnurb = object_editcurve_get(obedit);

    nu = editnurb->first;
    while (nu) {
      if (nu->type == CU_BEZIER) {
        bezt = nu->bezt;
        a = nu->pntsu;
        while (a--) {
          if (BEZT_ISSEL_ANY_HIDDENHANDLES(v3d, bezt)) {
            if (v1 == 0) {
              v1 = nr;
            }
            else if (v2 == 0) {
              v2 = nr;
            }
            else if (v3 == 0) {
              v3 = nr;
            }
            else if (v4 == 0) {
              v4 = nr;
            }
            else {
              break;
            }
          }
          nr++;
          bezt++;
        }
      }
      else {
        bp = nu->bp;
        a = nu->pntsu * nu->pntsv;
        while (a--) {
          if (bp->f1 & SELECT) {
            if (v1 == 0) {
              v1 = nr;
            }
            else if (v2 == 0) {
              v2 = nr;
            }
            else if (v3 == 0) {
              v3 = nr;
            }
            else if (v4 == 0) {
              v4 = nr;
            }
            else {
              break;
            }
          }
          nr++;
          bp++;
        }
      }
      nu = nu->next;
    }
  }
  else if (obedit->type == OB_LATTICE) {
    Lattice *lt = obedit->data;

    a = lt->editlatt->latt->pntsu * lt->editlatt->latt->pntsv * lt->editlatt->latt->pntsw;
    bp = lt->editlatt->latt->def;
    while (a--) {
      if (bp->f1 & SELECT) {
        if (v1 == 0) {
          v1 = nr;
        }
        else if (v2 == 0) {
          v2 = nr;
        }
        else if (v3 == 0) {
          v3 = nr;
        }
        else if (v4 == 0) {
          v4 = nr;
        }
        else {
          break;
        }
      }
      nr++;
      bp++;
    }
  }

  if (v4 || !((v1 && v2 == 0 && v3 == 0) || (v1 && v2 && v3))) {
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

        ob->parent = BASACT(view_layer)->object;
        if (v3) {
          ob->partype = PARVERT3;
          ob->par1 = v1 - 1;
          ob->par2 = v2 - 1;
          ob->par3 = v3 - 1;

          /* inverse parent matrix */
          BKE_object_workob_calc_parent(depsgraph, scene, ob, &workob);
          invert_m4_m4(ob->parentinv, workob.obmat);
        }
        else {
          ob->partype = PARVERT1;
          ob->par1 = v1 - 1;

          /* inverse parent matrix */
          BKE_object_workob_calc_parent(depsgraph, scene, ob, &workob);
          invert_m4_m4(ob->parentinv, workob.obmat);
        }
      }
    }
  }
  CTX_DATA_END;

  DEG_relations_tag_update(bmain);

  WM_event_add_notifier(C, NC_OBJECT, NULL);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_parent_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Vertex Parent";
  ot->description = "Parent selected objects to the selected vertices";
  ot->idname = "OBJECT_OT_vertex_parent_set";

  /* api callbacks */
  ot->invoke = WM_operator_confirm;
  ot->poll = vertex_parent_set_poll;
  ot->exec = vertex_parent_set_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Make Proxy Operator
 * \{ */

/* set the object to proxify */
static int make_proxy_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Scene *scene = CTX_data_scene(C);
  Object *ob = ED_object_active_context(C);

  /* sanity checks */
  if (!scene || ID_IS_LINKED(scene) || !ob) {
    return OPERATOR_CANCELLED;
  }

  /* Get object to work on - use a menu if we need to... */
  if (ob->instance_collection && ID_IS_LINKED(ob->instance_collection)) {
    /* gives menu with list of objects in group */
    /* proxy_group_objects_menu(C, op, ob, ob->instance_collection); */
    WM_enum_search_invoke(C, op, event);
    return OPERATOR_CANCELLED;
  }
  else if (ID_IS_LINKED(ob)) {
    uiPopupMenu *pup = UI_popup_menu_begin(C, IFACE_("OK?"), ICON_QUESTION);
    uiLayout *layout = UI_popup_menu_layout(pup);

    /* create operator menu item with relevant properties filled in */
    PointerRNA opptr_dummy;
    uiItemFullO_ptr(
        layout, op->type, op->type->name, ICON_NONE, NULL, WM_OP_EXEC_REGION_WIN, 0, &opptr_dummy);

    /* present the menu and be done... */
    UI_popup_menu_end(C, pup);

    /* this invoke just calls another instance of this operator... */
    return OPERATOR_INTERFACE;
  }
  else {
    /* error.. cannot continue */
    BKE_report(
        op->reports, RPT_ERROR, "Can only make proxy for a referenced object or collection");
    return OPERATOR_CANCELLED;
  }
}

static int make_proxy_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob, *gob = ED_object_active_context(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  if (gob->instance_collection != NULL) {
    const ListBase instance_collection_objects = BKE_collection_object_cache_get(
        gob->instance_collection);
    Base *base = BLI_findlink(&instance_collection_objects, RNA_enum_get(op->ptr, "object"));
    ob = base->object;
  }
  else {
    ob = gob;
    gob = NULL;
  }

  if (ob) {
    Object *newob;
    char name[MAX_ID_NAME + 4];

    BLI_snprintf(name, sizeof(name), "%s_proxy", ((ID *)(gob ? gob : ob))->name + 2);

    /* Add new object for the proxy */
    newob = BKE_object_add_from(bmain, scene, view_layer, OB_EMPTY, name, gob ? gob : ob);

    /* set layers OK */
    BKE_object_make_proxy(bmain, newob, ob, gob);

    /* Set back pointer immediately so dependency graph knows that this is
     * is a proxy and will act accordingly. Otherwise correctness of graph
     * will depend on order of bases.
     *
     * TODO(sergey): We really need to get rid of this bi-directional links
     * in proxies with something like library overrides.
     */
    if (newob->proxy != NULL) {
      newob->proxy->proxy_from = newob;
    }
    else {
      BKE_report(op->reports, RPT_ERROR, "Unable to assign proxy");
    }

    /* depsgraph flushes are needed for the new data */
    DEG_relations_tag_update(bmain);
    DEG_id_tag_update(&newob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, newob);
  }
  else {
    BKE_report(op->reports, RPT_ERROR, "No object to make proxy for");
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_FINISHED;
}

/* Generic itemf's for operators that take library args */
static const EnumPropertyItem *proxy_collection_object_itemf(bContext *C,
                                                             PointerRNA *UNUSED(ptr),
                                                             PropertyRNA *UNUSED(prop),
                                                             bool *r_free)
{
  EnumPropertyItem item_tmp = {0}, *item = NULL;
  int totitem = 0;
  int i = 0;
  Object *ob = ED_object_active_context(C);

  if (!ob || !ob->instance_collection) {
    return DummyRNA_DEFAULT_items;
  }

  /* find the object to affect */
  FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (ob->instance_collection, object) {
    item_tmp.identifier = item_tmp.name = object->id.name + 2;
    item_tmp.value = i++;
    RNA_enum_item_add(&item, &totitem, &item_tmp);
  }
  FOREACH_COLLECTION_OBJECT_RECURSIVE_END;

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

void OBJECT_OT_proxy_make(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Make Proxy";
  ot->idname = "OBJECT_OT_proxy_make";
  ot->description = "Add empty object to become local replacement data of a library-linked object";

  /* callbacks */
  ot->invoke = make_proxy_invoke;
  ot->exec = make_proxy_exec;
  ot->poll = ED_operator_object_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  /* XXX, relies on hard coded ID at the moment */
  prop = RNA_def_enum(ot->srna,
                      "object",
                      DummyRNA_DEFAULT_items,
                      0,
                      "Proxy Object",
                      "Name of lib-linked/collection object to make a proxy for");
  RNA_def_enum_funcs(prop, proxy_collection_object_itemf);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
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
  if (ELEM(par->type, OB_ARMATURE, OB_LATTICE, OB_CURVE)) {
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
      else if ((md->type == eModifierType_Curve) && (par->type == OB_CURVE)) {
        CurveModifierData *cmd = (CurveModifierData *)md;
        if (cmd->object == par) {
          free = true;
        }
      }

      /* free modifier if match */
      if (free) {
        BLI_remlink(&ob->modifiers, md);
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
      break;
    }
    case CLEAR_PARENT_KEEP_TRANSFORM: {
      /* remove parent, and apply the parented transform
       * result as object's local transforms */
      ob->parent = NULL;
      BKE_object_apply_mat4(ob, ob->obmat, true, false);
      break;
    }
    case CLEAR_PARENT_INVERSE: {
      /* object stays parented, but the parent inverse
       * (i.e. offset from parent to retain binding state)
       * is cleared. In other words: nothing to do here! */
      break;
    }
  }

  /* Always clear parentinv matrix for sake of consistency, see T41950. */
  unit_m4(ob->parentinv);

  DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
}

/* note, poll should check for editable scene */
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
  /* Always clear parentinv matrix for sake of consistency, see T41950. */
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
  BLI_strncpy(ob->parsubstr, substr, sizeof(ob->parsubstr));
}

/* Operator Property */
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
                          Object *ob,
                          Object *par,
                          int partype,
                          const bool xmirror,
                          const bool keep_transform,
                          const int vert_par[3])
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  bPoseChannel *pchan = NULL;
  bPoseChannel *pchan_eval = NULL;
  const bool pararm = ELEM(
      partype, PAR_ARMATURE, PAR_ARMATURE_NAME, PAR_ARMATURE_ENVELOPE, PAR_ARMATURE_AUTO);
  Object *parent_eval = DEG_get_evaluated_object(depsgraph, par);

  DEG_id_tag_update(&par->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  /* preconditions */
  if (partype == PAR_FOLLOW || partype == PAR_PATH_CONST) {
    if (par->type != OB_CURVE) {
      return 0;
    }
    else {
      Curve *cu = par->data;
      Curve *cu_eval = parent_eval->data;
      if ((cu->flag & CU_PATH) == 0) {
        cu->flag |= CU_PATH | CU_FOLLOW;
        cu_eval->flag |= CU_PATH | CU_FOLLOW;
        /* force creation of path data */
        BKE_displist_make_curveTypes(depsgraph, scene, par, false, false);
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
    }
  }
  else if (ELEM(partype, PAR_BONE, PAR_BONE_RELATIVE)) {
    pchan = BKE_pose_channel_active(par);
    pchan_eval = BKE_pose_channel_active(parent_eval);

    if (pchan == NULL) {
      BKE_report(reports, RPT_ERROR, "No active bone");
      return false;
    }
  }

  if (ob != par) {
    if (BKE_object_parent_loop_check(par, ob)) {
      BKE_report(reports, RPT_ERROR, "Loop in parents");
      return false;
    }
    else {
      Object workob;

      /* apply transformation of previous parenting */
      if (keep_transform) {
        /* was removed because of bug [#23577],
         * but this can be handy in some cases too [#32616], so make optional */
        BKE_object_apply_mat4(ob, ob->obmat, false, false);
      }

      /* set the parent (except for follow-path constraint option) */
      if (partype != PAR_PATH_CONST) {
        ob->parent = par;
        /* Always clear parentinv matrix for sake of consistency, see T41950. */
        unit_m4(ob->parentinv);
        DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
      }

      /* handle types */
      if (pchan) {
        BLI_strncpy(ob->parsubstr, pchan->name, sizeof(ob->parsubstr));
      }
      else {
        ob->parsubstr[0] = 0;
      }

      if (partype == PAR_PATH_CONST) {
        /* don't do anything here, since this is not technically "parenting" */
      }
      else if (ELEM(partype, PAR_CURVE, PAR_LATTICE) || (pararm)) {
        /* partype is now set to PAROBJECT so that invisible 'virtual'
         * modifiers don't need to be created.
         * NOTE: the old (2.4x) method was to set ob->partype = PARSKEL,
         * creating the virtual modifiers.
         */
        ob->partype = PAROBJECT;     /* note, dna define, not operator property */
        /* ob->partype = PARSKEL; */ /* note, dna define, not operator property */

        /* BUT, to keep the deforms, we need a modifier,
         * and then we need to set the object that it uses
         * - We need to ensure that the modifier we're adding doesn't already exist,
         *   so we check this by assuming that the parent is selected too.
         */
        /* XXX currently this should only happen for meshes, curves, surfaces,
         * and lattices - this stuff isn't available for metas yet */
        if (ELEM(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_LATTICE)) {
          ModifierData *md;

          switch (partype) {
            case PAR_CURVE: /* curve deform */
              if (BKE_modifiers_is_deformed_by_curve(ob) != par) {
                md = ED_object_modifier_add(reports, bmain, scene, ob, NULL, eModifierType_Curve);
                if (md) {
                  ((CurveModifierData *)md)->object = par;
                }
                if (par->runtime.curve_cache && par->runtime.curve_cache->path == NULL) {
                  DEG_id_tag_update(&par->id, ID_RECALC_GEOMETRY);
                }
              }
              break;
            case PAR_LATTICE: /* lattice deform */
              if (BKE_modifiers_is_deformed_by_lattice(ob) != par) {
                md = ED_object_modifier_add(
                    reports, bmain, scene, ob, NULL, eModifierType_Lattice);
                if (md) {
                  ((LatticeModifierData *)md)->object = par;
                }
              }
              break;
            default: /* armature deform */
              if (BKE_modifiers_is_deformed_by_armature(ob) != par) {
                md = ED_object_modifier_add(
                    reports, bmain, scene, ob, NULL, eModifierType_Armature);
                if (md) {
                  ((ArmatureModifierData *)md)->object = par;
                }
              }
              break;
          }
        }
      }
      else if (partype == PAR_BONE) {
        ob->partype = PARBONE; /* note, dna define, not operator property */
        if (pchan->bone) {
          pchan->bone->flag &= ~BONE_RELATIVE_PARENTING;
          pchan_eval->bone->flag &= ~BONE_RELATIVE_PARENTING;
        }
      }
      else if (partype == PAR_BONE_RELATIVE) {
        ob->partype = PARBONE; /* note, dna define, not operator property */
        if (pchan->bone) {
          pchan->bone->flag |= BONE_RELATIVE_PARENTING;
          pchan_eval->bone->flag |= BONE_RELATIVE_PARENTING;
        }
      }
      else if (partype == PAR_VERTEX) {
        ob->partype = PARVERT1;
        ob->par1 = vert_par[0];
      }
      else if (partype == PAR_VERTEX_TRI) {
        ob->partype = PARVERT3;
        copy_v3_v3_int(&ob->par1, vert_par);
      }
      else {
        ob->partype = PAROBJECT; /* note, dna define, not operator property */
      }

      /* constraint */
      if (partype == PAR_PATH_CONST) {
        bConstraint *con;
        bFollowPathConstraint *data;
        float cmat[4][4], vec[3];

        con = BKE_constraint_add_for_object(ob, "AutoPath", CONSTRAINT_TYPE_FOLLOWPATH);

        data = con->data;
        data->tar = par;

        BKE_constraint_target_matrix_get(
            depsgraph, scene, con, 0, CONSTRAINT_OBTYPE_OBJECT, NULL, cmat, scene->r.cfra);
        sub_v3_v3v3(vec, ob->obmat[3], cmat[3]);

        copy_v3_v3(ob->loc, vec);
      }
      else if (pararm && (ob->type == OB_MESH) && (par->type == OB_ARMATURE)) {
        if (partype == PAR_ARMATURE_NAME) {
          ED_object_vgroup_calc_from_armature(
              reports, depsgraph, scene, ob, par, ARM_GROUPS_NAME, false);
        }
        else if (partype == PAR_ARMATURE_ENVELOPE) {
          ED_object_vgroup_calc_from_armature(
              reports, depsgraph, scene, ob, par, ARM_GROUPS_ENVELOPE, xmirror);
        }
        else if (partype == PAR_ARMATURE_AUTO) {
          WM_cursor_wait(1);
          ED_object_vgroup_calc_from_armature(
              reports, depsgraph, scene, ob, par, ARM_GROUPS_AUTO, xmirror);
          WM_cursor_wait(0);
        }
        /* get corrected inverse */
        ob->partype = PAROBJECT;
        BKE_object_workob_calc_parent(depsgraph, scene, ob, &workob);

        invert_m4_m4(ob->parentinv, workob.obmat);
      }
      else if (pararm && (ob->type == OB_GPENCIL) && (par->type == OB_ARMATURE)) {
        if (partype == PAR_ARMATURE) {
          ED_gpencil_add_armature(C, reports, ob, par);
        }
        else if (partype == PAR_ARMATURE_NAME) {
          ED_gpencil_add_armature_weights(C, reports, ob, par, GP_PAR_ARMATURE_NAME);
        }
        else if ((partype == PAR_ARMATURE_AUTO) || (partype == PAR_ARMATURE_ENVELOPE)) {
          WM_cursor_wait(1);
          ED_gpencil_add_armature_weights(C, reports, ob, par, GP_PAR_ARMATURE_AUTO);
          WM_cursor_wait(0);
        }
        /* get corrected inverse */
        ob->partype = PAROBJECT;
        BKE_object_workob_calc_parent(depsgraph, scene, ob, &workob);

        invert_m4_m4(ob->parentinv, workob.obmat);
      }
      else if ((ob->type == OB_GPENCIL) && (par->type == OB_LATTICE)) {
        /* Add Lattice modifier */
        if (partype == PAR_LATTICE) {
          ED_gpencil_add_lattice_modifier(C, reports, ob, par);
        }
        /* get corrected inverse */
        ob->partype = PAROBJECT;
        BKE_object_workob_calc_parent(depsgraph, scene, ob, &workob);

        invert_m4_m4(ob->parentinv, workob.obmat);
      }
      else {
        /* calculate inverse parent matrix */
        BKE_object_workob_calc_parent(depsgraph, scene, ob, &workob);
        invert_m4_m4(ob->parentinv, workob.obmat);
      }

      DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    }
  }

  return true;
}

static void parent_set_vert_find(KDTree_3d *tree, Object *child, int vert_par[3], bool is_tri)
{
  const float *co_find = child->obmat[3];
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

static int parent_set_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *par = ED_object_active_context(C);
  int partype = RNA_enum_get(op->ptr, "type");
  const bool xmirror = RNA_boolean_get(op->ptr, "xmirror");
  const bool keep_transform = RNA_boolean_get(op->ptr, "keep_transform");
  bool ok = true;

  /* vertex parent (kdtree) */
  const bool is_vert_par = ELEM(partype, PAR_VERTEX, PAR_VERTEX_TRI);
  const bool is_tri = partype == PAR_VERTEX_TRI;
  int tree_tot;
  struct KDTree_3d *tree = NULL;
  int vert_par[3] = {0, 0, 0};
  const int *vert_par_p = is_vert_par ? vert_par : NULL;

  if (is_vert_par) {
    tree = BKE_object_as_kdtree(par, &tree_tot);
    BLI_assert(tree != NULL);

    if (tree_tot < (is_tri ? 3 : 1)) {
      BKE_report(op->reports, RPT_ERROR, "Not enough vertices for vertex-parent");
      ok = false;
    }
  }

  if (ok) {
    /* Non vertex-parent */
    CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
      if (is_vert_par) {
        parent_set_vert_find(tree, ob, vert_par, is_tri);
      }

      if (!ED_object_parent_set(
              op->reports, C, scene, ob, par, partype, xmirror, keep_transform, vert_par_p)) {
        ok = false;
        break;
      }
    }
    CTX_DATA_END;
  }

  if (is_vert_par) {
    BLI_kdtree_3d_free(tree);
  }

  if (!ok) {
    return OPERATOR_CANCELLED;
  }

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

  uiItemO(
      layout, IFACE_("Object (Without Inverse)"), ICON_NONE, "OBJECT_OT_parent_no_inverse_set");

  struct {
    bool mesh, gpencil;
  } has_children_of_type = {0};

  CTX_DATA_BEGIN (C, Object *, child, selected_editable_objects) {
    if (child == parent) {
      continue;
    }
    if (child->type == OB_MESH) {
      has_children_of_type.mesh = true;
    }
    if (child->type == OB_GPENCIL) {
      has_children_of_type.gpencil = true;
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
  else if (parent->type == OB_CURVE) {
    uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_CURVE);
    uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_FOLLOW);
    uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_PATH_CONST);
  }
  else if (parent->type == OB_LATTICE) {
    uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_LATTICE);
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
    else {
      return false;
    }
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

  DEG_id_tag_update(&par->id, ID_RECALC_TRANSFORM);

  /* context iterator */
  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    if (ob != par) {
      if (BKE_object_parent_loop_check(par, ob)) {
        BKE_report(op->reports, RPT_ERROR, "Loop in parents");
      }
      else {
        /* clear inverse matrix and also the object location */
        unit_m4(ob->parentinv);
        memset(ob->loc, 0, 3 * sizeof(float));

        /* set recalc flags */
        DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

        /* set parenting type for object - object only... */
        ob->parent = par;
        ob->partype = PAROBJECT; /* note, dna define, not operator property */
      }
    }
  }
  CTX_DATA_END;

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_parent_no_inverse_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Parent without Inverse";
  ot->description = "Set the object's parenting without setting the inverse parent correction";
  ot->idname = "OBJECT_OT_parent_no_inverse_set";

  /* api callbacks */
  ot->invoke = WM_operator_confirm;
  ot->exec = parent_noinv_set_exec;
  ot->poll = ED_operator_object_active_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
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

/* note, poll should check for editable scene */
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
      BKE_object_apply_mat4(ob, ob->obmat, true, true);
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
    {CREATE_TRACK_TRACKTO, "TRACKTO", 0, "Track To Constraint", ""},
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

  if (ID_IS_LINKED(scene_to)) {
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

  /* one day multiple scenes will be visible, then we should have some update function for them */
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
      if (OB_TYPE_SUPPORT_MATERIAL(ob_src->type) && OB_TYPE_SUPPORT_MATERIAL(ob_dst->type)) {
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
      if ((ob_src->data != ob_dst->data) && (ob_src->type == OB_FONT) &&
          (ob_dst->type == OB_FONT)) {
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
              if (ID_IS_LINKED(obdata_id)) {
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

            if (ID_IS_LINKED(obdata_id)) {
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
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, CTX_wm_view3d(C));
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
      {MAKE_LINKS_OBDATA, "OBDATA", 0, "Object Data", ""},
      {MAKE_LINKS_MATERIALS, "MATERIAL", 0, "Materials", ""},
      {MAKE_LINKS_ANIMDATA, "ANIMATION", 0, "Animation Data", ""},
      {MAKE_LINKS_GROUP, "GROUPS", 0, "Collection", ""},
      {MAKE_LINKS_DUPLICOLLECTION, "DUPLICOLLECTION", 0, "Instance Collection", ""},
      {MAKE_LINKS_MODIFIERS, "MODIFIERS", 0, "Modifiers", ""},
      {MAKE_LINKS_FONTS, "FONTS", 0, "Fonts", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Link Data";
  ot->description = "Apply active object links to other selected objects";
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

static void libblock_relink_collection(Collection *collection, const bool do_collection)
{
  if (do_collection) {
    BKE_libblock_relink_to_newid(&collection->id);
  }

  for (CollectionObject *cob = collection->gobject.first; cob != NULL; cob = cob->next) {
    BKE_libblock_relink_to_newid(&cob->ob->id);
  }

  LISTBASE_FOREACH (CollectionChild *, child, &collection->children) {
    libblock_relink_collection(child->collection, true);
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
    collection = ID_NEW_SET(collection, BKE_collection_copy(bmain, NULL, collection));
  }

  /* We do not remap to new objects here, this is done in separate step. */
  LISTBASE_FOREACH (CollectionObject *, cob, &collection->gobject) {
    Object *ob = cob->ob;
    /* an object may be in more than one collection */
    if ((ob->id.newid == NULL) && ((ob->flag & flag) == flag)) {
      if (!ID_IS_LINKED(ob) && BKE_object_scenes_users_get(bmain, ob) > 1) {
        ID_NEW_SET(ob, BKE_object_copy(bmain, ob));
      }
    }
  }

  /* Since master collection has already be duplicated as part of scene copy,
   * we do not duplicate it here.
   * However, this means its children need to be re-added manually here,
   * otherwise their parent lists are empty (which will lead to crashes, see T63101). */
  CollectionChild *child_next, *child = collection->children.first;
  CollectionChild *orig_child_last = collection->children.last;
  for (; child != NULL; child = child_next) {
    child_next = child->next;
    Collection *collection_child_new = single_object_users_collection(
        bmain, scene, child->collection, flag, copy_collections, false);

    if (is_master_collection && copy_collections && child->collection != collection_child_new) {
      /* We do not want a collection sync here, our collections are in a complete uninitialized
       * state currently. With current code, that would lead to a memory leak - because of reasons.
       * It would be a useless loss of computing anyway, since caller has to fully refresh
       * view-layers/collections caching at the end. */
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
  BKE_libblock_relink_to_newid(&scene->id);

  /* Collection and object pointers in collections */
  libblock_relink_collection(scene->master_collection, false);

  /* We also have to handle runtime things in UI. */
  if (v3d) {
    ID_NEW_REMAP(v3d->camera);
  }

  /* Making single user may affect other scenes if they share
   * with current one some collections in their ViewLayer. */
  BKE_main_collection_sync_remap(bmain);
}

/* not an especially efficient function, only added so the single user
 * button can be functional.*/
void ED_object_single_user(Main *bmain, Scene *scene, Object *ob)
{
  FOREACH_SCENE_OBJECT_BEGIN (scene, ob_iter) {
    ob_iter->flag &= ~OB_DONE;
  }
  FOREACH_SCENE_OBJECT_END;

  /* tag only the one object */
  ob->flag |= OB_DONE;

  single_object_users(bmain, scene, NULL, OB_DONE, false);
  BKE_main_id_clear_newpoins(bmain);
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
    if (!ID_IS_LINKED(ob)) {
      id = ob->data;

      if (id && id->us > 1 && !ID_IS_LINKED(id)) {
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);

        switch (ob->type) {
          case OB_LAMP:
            ob->data = la = ID_NEW_SET(ob->data, BKE_light_copy(bmain, ob->data));
            break;
          case OB_CAMERA:
            cam = ob->data = ID_NEW_SET(ob->data, BKE_camera_copy(bmain, ob->data));
            ID_NEW_REMAP(cam->dof.focus_object);
            break;
          case OB_MESH:
            /* Needed to remap texcomesh below. */
            me = ob->data = ID_NEW_SET(ob->data, BKE_mesh_copy(bmain, ob->data));
            if (me->key) { /* We do not need to set me->key->id.newid here... */
              BKE_animdata_copy_id_action(bmain, (ID *)me->key, false);
            }
            break;
          case OB_MBALL:
            ob->data = ID_NEW_SET(ob->data, BKE_mball_copy(bmain, ob->data));
            break;
          case OB_CURVE:
          case OB_SURF:
          case OB_FONT:
            ob->data = cu = ID_NEW_SET(ob->data, BKE_curve_copy(bmain, ob->data));
            ID_NEW_REMAP(cu->bevobj);
            ID_NEW_REMAP(cu->taperobj);
            if (cu->key) { /* We do not need to set cu->key->id.newid here... */
              BKE_animdata_copy_id_action(bmain, (ID *)cu->key, false);
            }
            break;
          case OB_LATTICE:
            ob->data = lat = ID_NEW_SET(ob->data, BKE_lattice_copy(bmain, ob->data));
            if (lat->key) { /* We do not need to set lat->key->id.newid here... */
              BKE_animdata_copy_id_action(bmain, (ID *)lat->key, false);
            }
            break;
          case OB_ARMATURE:
            DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
            ob->data = ID_NEW_SET(ob->data, BKE_armature_copy(bmain, ob->data));
            BKE_pose_rebuild(bmain, ob, ob->data, true);
            break;
          case OB_SPEAKER:
            ob->data = ID_NEW_SET(ob->data, BKE_speaker_copy(bmain, ob->data));
            break;
          case OB_LIGHTPROBE:
            ob->data = ID_NEW_SET(ob->data, BKE_lightprobe_copy(bmain, ob->data));
            break;
          case OB_GPENCIL:
            ob->data = ID_NEW_SET(ob->data, BKE_gpencil_copy(bmain, ob->data));
            break;
          case OB_HAIR:
            ob->data = ID_NEW_SET(ob->data, BKE_hair_copy(bmain, ob->data));
            break;
          case OB_POINTCLOUD:
            ob->data = ID_NEW_SET(ob->data, BKE_pointcloud_copy(bmain, ob->data));
            break;
          case OB_VOLUME:
            ob->data = ID_NEW_SET(ob->data, BKE_volume_copy(bmain, ob->data));
            break;
          default:
            printf("ERROR %s: can't copy %s\n", __func__, id->name);
            BLI_assert(!"This should never happen.");

            /* We need to end the FOREACH_OBJECT_FLAG_BEGIN iterator to prevent memory leak. */
            BKE_scene_objects_iterator_end(&iter_macro);
            return;
        }

        /* Copy animation data after object data became local,
         * otherwise old and new object data will share the same
         * AnimData structure, which is not what we want.
         *                                             (sergey)
         */
        BKE_animdata_copy_id_action(bmain, (ID *)ob->data, false);

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

static void single_object_action_users(
    Main *bmain, Scene *scene, ViewLayer *view_layer, View3D *v3d, const int flag)
{
  FOREACH_OBJECT_FLAG_BEGIN (scene, view_layer, v3d, flag, ob) {
    if (!ID_IS_LINKED(ob)) {
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      BKE_animdata_copy_id_action(bmain, &ob->id, false);
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
    if (!ID_IS_LINKED(ob)) {
      for (a = 1; a <= ob->totcol; a++) {
        ma = BKE_object_material_get(ob, a);
        if (ma) {
          /* do not test for LIB_TAG_NEW or use newid:
           * this functions guaranteed delivers single_users! */

          if (ma->id.us > 1) {
            man = BKE_material_copy(bmain, ma);
            BKE_animdata_copy_id_action(bmain, &man->id, false);

            man->id.us = 0;
            BKE_object_material_assign(bmain, ob, man, a, BKE_MAT_ASSIGN_USERPREF);
          }
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

    /* If data is also gonna to become local, mark data we're interested in
     * as gonna-to-be-local.
     */
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
 * otherwise they're lost on reload, see T40595.
 */
static bool make_local_all__instance_indirect_unused(Main *bmain,
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

  /* Note: we (ab)use LIB_TAG_PRE_EXISTING to cherry pick which ID to make local... */
  if (mode == MAKE_LOCAL_ALL) {
    ViewLayer *view_layer = CTX_data_view_layer(C);
    Collection *collection = CTX_data_collection(C);

    BKE_main_id_tag_all(bmain, LIB_TAG_PRE_EXISTING, false);

    /* De-select so the user can differentiate newly instanced from existing objects. */
    BKE_view_layer_base_deselect_all(view_layer);

    if (make_local_all__instance_indirect_unused(bmain, view_layer, collection)) {
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

  BKE_library_make_local(bmain, NULL, NULL, true, false); /* NULL is all libs */

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

static void make_override_library_tag_object(Object *obact, Object *ob)
{
  if (ob == obact) {
    return;
  }

  if (!ID_IS_LINKED(ob)) {
    return;
  }

  /* Note: all this is very case-by-case bad handling, ultimately we'll want a real full
   * 'automatic', generic handling of all this,
   * will probably require adding some override-aware stuff to library_query code... */

  if (obact->type == OB_ARMATURE && ob->modifiers.first != NULL) {
    for (ModifierData *md = ob->modifiers.first; md != NULL; md = md->next) {
      if (md->type == eModifierType_Armature) {
        ArmatureModifierData *amd = (ArmatureModifierData *)md;
        if (amd->object == obact) {
          ob->id.tag |= LIB_TAG_DOIT;
          break;
        }
      }
    }
  }
  else if (ob->parent == obact) {
    ob->id.tag |= LIB_TAG_DOIT;
  }

  if (ob->id.tag & LIB_TAG_DOIT) {
    printf("Indirectly overriding %s for %s\n", ob->id.name, obact->id.name);
  }
}

static void make_override_library_tag_collections(Collection *collection)
{
  collection->id.tag |= LIB_TAG_DOIT;
  for (CollectionChild *coll_child = collection->children.first; coll_child != NULL;
       coll_child = coll_child->next) {
    make_override_library_tag_collections(coll_child->collection);
  }
}

/* Set the object to override. */
static int make_override_library_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Scene *scene = CTX_data_scene(C);
  Object *obact = ED_object_active_context(C);

  /* Sanity checks. */
  if (!scene || ID_IS_LINKED(scene) || !obact) {
    return OPERATOR_CANCELLED;
  }

  /* Get object to work on - use a menu if we need to... */
  if (!ID_IS_LINKED(obact) && obact->instance_collection != NULL &&
      ID_IS_LINKED(obact->instance_collection)) {
    /* Gives menu with list of objects in group. */
    WM_enum_search_invoke(C, op, event);
    return OPERATOR_CANCELLED;
  }
  else if (ID_IS_LINKED(obact)) {
    uiPopupMenu *pup = UI_popup_menu_begin(C, IFACE_("OK?"), ICON_QUESTION);
    uiLayout *layout = UI_popup_menu_layout(pup);

    /* Create operator menu item with relevant properties filled in. */
    PointerRNA opptr_dummy;
    uiItemFullO_ptr(
        layout, op->type, op->type->name, ICON_NONE, NULL, WM_OP_EXEC_REGION_WIN, 0, &opptr_dummy);

    /* Present the menu and be done... */
    UI_popup_menu_end(C, pup);

    /* This invoke just calls another instance of this operator... */
    return OPERATOR_INTERFACE;
  }
  else {
    /* Error.. cannot continue. */
    BKE_report(op->reports,
               RPT_ERROR,
               "Can only make library override for a referenced object or collection");
    return OPERATOR_CANCELLED;
  }
}

static int make_override_library_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *obact = CTX_data_active_object(C);

  bool success = false;

  if (!ID_IS_LINKED(obact) && obact->instance_collection != NULL &&
      ID_IS_LINKED(obact->instance_collection)) {
    if (!ID_IS_OVERRIDABLE_LIBRARY(obact->instance_collection)) {
      BKE_reportf(op->reports,
                  RPT_ERROR_INVALID_INPUT,
                  "Collection '%s' (instantiated by the active object) is not overridable",
                  obact->instance_collection->id.name + 2);
      return OPERATOR_CANCELLED;
    }

    BKE_main_id_tag_all(bmain, LIB_TAG_DOIT, false);

    Object *obcollection = obact;
    Collection *collection = obcollection->instance_collection;

    const ListBase dup_collection_objects = BKE_collection_object_cache_get(collection);
    Base *base = BLI_findlink(&dup_collection_objects, RNA_enum_get(op->ptr, "object"));
    obact = base->object;

    /* First, we make a library override of the linked collection itself, and all its children. */
    make_override_library_tag_collections(collection);

    /* Then, we make library override of the whole set of objects in the Collection. */
    FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (collection, ob) {
      ob->id.tag |= LIB_TAG_DOIT;
    }
    FOREACH_COLLECTION_OBJECT_RECURSIVE_END;

    /* Then, we remove (untag) bone shape objects, you shall never want to override those
     * (hopefully)... */
    FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (collection, ob) {
      if (ob->type == OB_ARMATURE && ob->pose != NULL) {
        for (bPoseChannel *pchan = ob->pose->chanbase.first; pchan != NULL; pchan = pchan->next) {
          if (pchan->custom != NULL) {
            pchan->custom->id.tag &= ~LIB_TAG_DOIT;
          }
        }
      }
    }
    FOREACH_COLLECTION_OBJECT_RECURSIVE_END;

    success = BKE_lib_override_library_create_from_tag(bmain);

    /* Instantiate our newly overridden objects in scene, if not yet done. */
    Scene *scene = CTX_data_scene(C);
    ViewLayer *view_layer = CTX_data_view_layer(C);
    Collection *new_collection = (Collection *)collection->id.newid;

    BKE_collection_add_from_object(bmain, scene, obcollection, new_collection);

    FOREACH_COLLECTION_OBJECT_RECURSIVE_BEGIN (new_collection, new_ob) {
      if (new_ob != NULL && new_ob->id.override_library != NULL) {
        if ((base = BKE_view_layer_base_find(view_layer, new_ob)) == NULL) {
          BKE_collection_object_add_from(bmain, scene, obcollection, new_ob);
          base = BKE_view_layer_base_find(view_layer, new_ob);
          DEG_id_tag_update_ex(bmain, &new_ob->id, ID_RECALC_TRANSFORM | ID_RECALC_BASE_FLAGS);
        }

        if (new_ob == (Object *)obact->id.newid) {
          /* TODO: is setting active needed? */
          BKE_view_layer_base_select_and_set_active(view_layer, base);
        }
        /* We still want to store all objects' current override status (i.e. change of parent). */
        BKE_lib_override_library_operations_create(bmain, &new_ob->id, true);
      }
    }
    FOREACH_COLLECTION_OBJECT_RECURSIVE_END;

    /* Remove the instance empty from this scene, the items now have an overridden collection
     * instead. */
    ED_object_base_free_and_unlink(bmain, scene, obcollection);

    /* Also, we'd likely want to lock by default things like
     * transformations of implicitly overridden objects? */

    DEG_id_tag_update(&scene->id, 0);

    /* Cleanup. */
    BKE_main_id_clear_newpoins(bmain);
    BKE_main_id_tag_all(bmain, LIB_TAG_DOIT, false);
  }
  else if (!ID_IS_OVERRIDABLE_LIBRARY(obact)) {
    BKE_reportf(op->reports,
                RPT_ERROR_INVALID_INPUT,
                "Active object '%s' is not overridable",
                obact->id.name + 2);
    return OPERATOR_CANCELLED;
  }
  /* Else, poll func ensures us that ID_IS_LINKED(obact) is true. */
  else if (obact->type == OB_ARMATURE) {
    BKE_main_id_tag_all(bmain, LIB_TAG_DOIT, false);

    obact->id.tag |= LIB_TAG_DOIT;

    for (Object *ob = bmain->objects.first; ob != NULL; ob = ob->id.next) {
      make_override_library_tag_object(obact, ob);
    }

    success = BKE_lib_override_library_create_from_tag(bmain);

    /* Also, we'd likely want to lock by default things like
     * transformations of implicitly overridden objects? */

    /* Cleanup. */
    BKE_main_id_clear_newpoins(bmain);
    BKE_main_id_tag_all(bmain, LIB_TAG_DOIT, false);
  }
  /* TODO: probably more cases where we want to do automated smart things in the future! */
  else {
    /* For now, remapp all local usages of linked ID to local override one here. */
    BKE_main_id_tag_all(bmain, LIB_TAG_DOIT, true);
    success = (BKE_lib_override_library_create_from_id(bmain, &obact->id, true) != NULL);
    BKE_main_id_tag_all(bmain, LIB_TAG_DOIT, false);
  }

  WM_event_add_notifier(C, NC_WINDOW, NULL);

  return success ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static bool make_override_library_poll(bContext *C)
{
  Object *obact = CTX_data_active_object(C);

  /* Object must be directly linked to be overridable. */
  return (BKE_lib_override_library_is_enabled() && ED_operator_objectmode(C) && obact != NULL &&
          ((ID_IS_LINKED(obact) && obact->id.tag & LIB_TAG_EXTERN) ||
           (!ID_IS_LINKED(obact) && obact->instance_collection != NULL &&
            ID_IS_LINKED(obact->instance_collection))));
}

void OBJECT_OT_make_override_library(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Library Override";
  ot->description = "Make a local override of this library linked data-block";
  ot->idname = "OBJECT_OT_make_override_library";

  /* api callbacks */
  ot->invoke = make_override_library_invoke;
  ot->exec = make_override_library_exec;
  ot->poll = make_override_library_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  PropertyRNA *prop;
  prop = RNA_def_enum(ot->srna,
                      "object",
                      DummyRNA_DEFAULT_items,
                      0,
                      "Override Object",
                      "Name of lib-linked/collection object to make an override from");
  RNA_def_enum_funcs(prop, proxy_collection_object_itemf);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
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
      BKE_view_layer_selected_objects_tag(view_layer, OB_DONE);
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

    /* Needed since some IDs were remapped? (incl. me->texcomesh, see T73797). */
    update_deps = true;
  }

  if (RNA_boolean_get(op->ptr, "material")) {
    single_mat_users(bmain, scene, view_layer, v3d, flag);
  }

  if (RNA_boolean_get(op->ptr, "animation")) {
    single_object_action_users(bmain, scene, view_layer, v3d, flag);
  }

  BKE_main_id_clear_newpoins(bmain);

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
  RNA_def_boolean(
      ot->srna, "animation", 0, "Object Animation", "Make animation data local to each object");
}

/** \} */

/* ------------------------------------------------------------------- */
/** \name Drop Named Material on Object Operator
 * \{ */

static int drop_named_material_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Main *bmain = CTX_data_main(C);
  Base *base = ED_view3d_give_base_under_cursor(C, event->mval);
  Material *ma;
  char name[MAX_ID_NAME - 2];

  RNA_string_get(op->ptr, "name", name);
  ma = (Material *)BKE_libblock_find_name(bmain, ID_MA, name);
  if (base == NULL || ma == NULL) {
    return OPERATOR_CANCELLED;
  }

  BKE_object_material_assign(CTX_data_main(C), base->object, ma, 1, BKE_MAT_ASSIGN_USERPREF);

  DEG_id_tag_update(&base->object->id, ID_RECALC_TRANSFORM);

  WM_event_add_notifier(C, NC_OBJECT | ND_OB_SHADING, base->object);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, CTX_wm_view3d(C));
  WM_event_add_notifier(C, NC_MATERIAL | ND_SHADING_LINKS, ma);

  return OPERATOR_FINISHED;
}

/* used for dropbox */
/* assigns to object under cursor, only first material slot */
void OBJECT_OT_drop_named_material(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Drop Named Material on Object";
  ot->idname = "OBJECT_OT_drop_named_material";

  /* api callbacks */
  ot->invoke = drop_named_material_invoke;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  /* properties */
  RNA_def_string(ot->srna, "name", "Material", MAX_ID_NAME - 2, "Name", "Material name to assign");
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

/**
 * \note Only for empty-image objects, this operator is needed
 */
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
