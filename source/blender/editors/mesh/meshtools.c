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
 * The Original Code is Copyright (C) 2004 by Blender Foundation
 * All rights reserved.
 */

/** \file
 * \ingroup edmesh
 *
 * meshtools.c: no editmode (violated already :), mirror & join),
 * tools operating on meshes
 */

#include "MEM_guardedalloc.h"

#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_workspace_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_key.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_mesh_iterators.h"
#include "BKE_mesh_runtime.h"
#include "BKE_multires.h"
#include "BKE_object.h"
#include "BKE_object_deform.h"
#include "BKE_object_facemap.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "DRW_select_buffer.h"

#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_view3d.h"

#include "WM_api.h"
#include "WM_types.h"

/* * ********************** no editmode!!! *********** */

/*********************** JOIN ***************************/

/* join selected meshes into the active mesh, context sensitive
 * return 0 if no join is made (error) and 1 if the join is done */

static void join_mesh_single(Depsgraph *depsgraph,
                             Main *bmain,
                             Scene *scene,
                             Object *ob_dst,
                             Object *ob_src,
                             const float imat[4][4],
                             MVert **mvert_pp,
                             MEdge **medge_pp,
                             MLoop **mloop_pp,
                             MPoly **mpoly_pp,
                             CustomData *vdata,
                             CustomData *edata,
                             CustomData *ldata,
                             CustomData *pdata,
                             int totvert,
                             int totedge,
                             int totloop,
                             int totpoly,
                             Key *key,
                             Key *nkey,
                             Material **matar,
                             int *matmap,
                             int totcol,
                             int *vertofs,
                             int *edgeofs,
                             int *loopofs,
                             int *polyofs)
{
  int a, b;

  Mesh *me = ob_src->data;
  MVert *mvert = *mvert_pp;
  MEdge *medge = *medge_pp;
  MLoop *mloop = *mloop_pp;
  MPoly *mpoly = *mpoly_pp;

  if (me->totvert) {
    /* merge customdata flag */
    ((Mesh *)ob_dst->data)->cd_flag |= me->cd_flag;

    /* standard data */
    CustomData_merge(&me->vdata, vdata, CD_MASK_MESH.vmask, CD_DEFAULT, totvert);
    CustomData_copy_data_named(&me->vdata, vdata, 0, *vertofs, me->totvert);

    /* vertex groups */
    MDeformVert *dvert = CustomData_get(vdata, *vertofs, CD_MDEFORMVERT);
    MDeformVert *dvert_src = CustomData_get(&me->vdata, 0, CD_MDEFORMVERT);

    /* Remap to correct new vgroup indices, if needed. */
    if (dvert_src) {
      BLI_assert(dvert != NULL);

      /* Build src to merged mapping of vgroup indices. */
      int *vgroup_index_map;
      int vgroup_index_map_len;
      vgroup_index_map = BKE_object_defgroup_index_map_create(
          ob_src, ob_dst, &vgroup_index_map_len);
      BKE_object_defgroup_index_map_apply(
          dvert, me->totvert, vgroup_index_map, vgroup_index_map_len);
      if (vgroup_index_map != NULL) {
        MEM_freeN(vgroup_index_map);
      }
    }

    /* if this is the object we're merging into, no need to do anything */
    if (ob_src != ob_dst) {
      float cmat[4][4];

      /* watch this: switch matmul order really goes wrong */
      mul_m4_m4m4(cmat, imat, ob_src->obmat);

      /* transform vertex coordinates into new space */
      for (a = 0, mvert = *mvert_pp; a < me->totvert; a++, mvert++) {
        mul_m4_v3(cmat, mvert->co);
      }

      /* For each shapekey in destination mesh:
       * - if there's a matching one, copy it across
       *   (will need to transform vertices into new space...).
       * - otherwise, just copy own coordinates of mesh
       *   (no need to transform vertex coordinates into new space).
       */
      if (key) {
        /* if this mesh has any shapekeys, check first, otherwise just copy coordinates */
        LISTBASE_FOREACH (KeyBlock *, kb, &key->block) {
          /* get pointer to where to write data for this mesh in shapekey's data array */
          float(*cos)[3] = ((float(*)[3])kb->data) + *vertofs;

          /* check if this mesh has such a shapekey */
          KeyBlock *okb = me->key ? BKE_keyblock_find_name(me->key, kb->name) : NULL;
          if (okb) {
            /* copy this mesh's shapekey to the destination shapekey
             * (need to transform first) */
            float(*ocos)[3] = okb->data;
            for (a = 0; a < me->totvert; a++, cos++, ocos++) {
              copy_v3_v3(*cos, *ocos);
              mul_m4_v3(cmat, *cos);
            }
          }
          else {
            /* copy this mesh's vertex coordinates to the destination shapekey */
            for (a = 0, mvert = *mvert_pp; a < me->totvert; a++, cos++, mvert++) {
              copy_v3_v3(*cos, mvert->co);
            }
          }
        }
      }
    }
    else {
      /* for each shapekey in destination mesh:
       * - if it was an 'original', copy the appropriate data from nkey
       * - otherwise, copy across plain coordinates (no need to transform coordinates)
       */
      if (key) {
        LISTBASE_FOREACH (KeyBlock *, kb, &key->block) {
          /* get pointer to where to write data for this mesh in shapekey's data array */
          float(*cos)[3] = ((float(*)[3])kb->data) + *vertofs;

          /* check if this was one of the original shapekeys */
          KeyBlock *okb = nkey ? BKE_keyblock_find_name(nkey, kb->name) : NULL;
          if (okb) {
            /* copy this mesh's shapekey to the destination shapekey */
            float(*ocos)[3] = okb->data;
            for (a = 0; a < me->totvert; a++, cos++, ocos++) {
              copy_v3_v3(*cos, *ocos);
            }
          }
          else {
            /* copy base-coordinates to the destination shapekey */
            for (a = 0, mvert = *mvert_pp; a < me->totvert; a++, cos++, mvert++) {
              copy_v3_v3(*cos, mvert->co);
            }
          }
        }
      }
    }
  }

  if (me->totedge) {
    CustomData_merge(&me->edata, edata, CD_MASK_MESH.emask, CD_DEFAULT, totedge);
    CustomData_copy_data_named(&me->edata, edata, 0, *edgeofs, me->totedge);

    for (a = 0; a < me->totedge; a++, medge++) {
      medge->v1 += *vertofs;
      medge->v2 += *vertofs;
    }
  }

  if (me->totloop) {
    if (ob_src != ob_dst) {
      MultiresModifierData *mmd;

      multiresModifier_prepare_join(depsgraph, scene, ob_src, ob_dst);

      if ((mmd = get_multires_modifier(scene, ob_src, true))) {
        ED_object_iter_other(
            bmain, ob_src, true, ED_object_multires_update_totlevels_cb, &mmd->totlvl);
      }
    }

    CustomData_merge(&me->ldata, ldata, CD_MASK_MESH.lmask, CD_DEFAULT, totloop);
    CustomData_copy_data_named(&me->ldata, ldata, 0, *loopofs, me->totloop);

    for (a = 0; a < me->totloop; a++, mloop++) {
      mloop->v += *vertofs;
      mloop->e += *edgeofs;
    }
  }

  if (me->totpoly) {
    if (matmap) {
      /* make mapping for materials */
      for (a = 1; a <= ob_src->totcol; a++) {
        Material *ma = BKE_object_material_get(ob_src, a);

        for (b = 0; b < totcol; b++) {
          if (ma == matar[b]) {
            matmap[a - 1] = b;
            break;
          }
        }
      }
    }

    CustomData_merge(&me->pdata, pdata, CD_MASK_MESH.pmask, CD_DEFAULT, totpoly);
    CustomData_copy_data_named(&me->pdata, pdata, 0, *polyofs, me->totpoly);

    for (a = 0; a < me->totpoly; a++, mpoly++) {
      mpoly->loopstart += *loopofs;
      mpoly->mat_nr = matmap ? matmap[mpoly->mat_nr] : 0;
    }

    /* Face maps. */
    int *fmap = CustomData_get(pdata, *polyofs, CD_FACEMAP);
    int *fmap_src = CustomData_get(&me->pdata, 0, CD_FACEMAP);

    /* Remap to correct new face-map indices, if needed. */
    if (fmap_src) {
      BLI_assert(fmap != NULL);
      int *fmap_index_map;
      int fmap_index_map_len;
      fmap_index_map = BKE_object_facemap_index_map_create(ob_src, ob_dst, &fmap_index_map_len);
      BKE_object_facemap_index_map_apply(fmap, me->totpoly, fmap_index_map, fmap_index_map_len);
      if (fmap_index_map != NULL) {
        MEM_freeN(fmap_index_map);
      }
    }
  }

  /* these are used for relinking (cannot be set earlier, or else reattaching goes wrong) */
  *vertofs += me->totvert;
  *mvert_pp += me->totvert;
  *edgeofs += me->totedge;
  *medge_pp += me->totedge;
  *loopofs += me->totloop;
  *mloop_pp += me->totloop;
  *polyofs += me->totpoly;
  *mpoly_pp += me->totpoly;
}

int ED_mesh_join_objects_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  Material **matar = NULL, *ma;
  Mesh *me;
  MVert *mvert = NULL;
  MEdge *medge = NULL;
  MPoly *mpoly = NULL;
  MLoop *mloop = NULL;
  Key *key, *nkey = NULL;
  KeyBlock *kb, *kbn;
  float imat[4][4];
  int a, b, totcol, totmat = 0, totedge = 0, totvert = 0;
  int totloop = 0, totpoly = 0, vertofs, *matmap = NULL;
  int i, haskey = 0, edgeofs, loopofs, polyofs;
  bool ok = false, join_parent = false;
  bDeformGroup *dg, *odg;
  CustomData vdata, edata, fdata, ldata, pdata;

  if (ob->mode & OB_MODE_EDIT) {
    BKE_report(op->reports, RPT_WARNING, "Cannot join while in edit mode");
    return OPERATOR_CANCELLED;
  }

  /* ob is the object we are adding geometry to */
  if (!ob || ob->type != OB_MESH) {
    BKE_report(op->reports, RPT_WARNING, "Active object is not a mesh");
    return OPERATOR_CANCELLED;
  }

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  /* count & check */
  CTX_DATA_BEGIN (C, Object *, ob_iter, selected_editable_objects) {
    if (ob_iter->type == OB_MESH) {
      me = ob_iter->data;

      totvert += me->totvert;
      totedge += me->totedge;
      totloop += me->totloop;
      totpoly += me->totpoly;
      totmat += ob_iter->totcol;

      if (ob_iter == ob) {
        ok = true;
      }

      if ((ob->parent != NULL) && (ob_iter == ob->parent)) {
        join_parent = true;
      }

      /* check for shapekeys */
      if (me->key) {
        haskey++;
      }
    }
  }
  CTX_DATA_END;

  /* Apply parent transform if the active object's parent was joined to it.
   * Note: This doesn't apply recursive parenting. */
  if (join_parent) {
    ob->parent = NULL;
    BKE_object_apply_mat4_ex(ob, ob->obmat, ob->parent, ob->parentinv, false);
  }

  /* that way the active object is always selected */
  if (ok == false) {
    BKE_report(op->reports, RPT_WARNING, "Active object is not a selected mesh");
    return OPERATOR_CANCELLED;
  }

  /* Only join meshes if there are verts to join,
   * there aren't too many, and we only had one mesh selected. */
  me = (Mesh *)ob->data;
  key = me->key;

  if (totvert == 0 || totvert == me->totvert) {
    BKE_report(op->reports, RPT_WARNING, "No mesh data to join");
    return OPERATOR_CANCELLED;
  }

  if (totvert > MESH_MAX_VERTS) {
    BKE_reportf(op->reports,
                RPT_WARNING,
                "Joining results in %d vertices, limit is %ld",
                totvert,
                MESH_MAX_VERTS);
    return OPERATOR_CANCELLED;
  }

  /* remove tessface to ensure we don't hold references to invalid faces */
  BKE_mesh_tessface_clear(me);

  /* new material indices and material array */
  if (totmat) {
    matar = MEM_callocN(sizeof(*matar) * totmat, "join_mesh matar");
    matmap = MEM_callocN(sizeof(*matmap) * totmat, "join_mesh matmap");
  }
  totcol = ob->totcol;

  /* obact materials in new main array, is nicer start! */
  for (a = 0; a < ob->totcol; a++) {
    matar[a] = BKE_object_material_get(ob, a + 1);
    id_us_plus((ID *)matar[a]);
    /* increase id->us : will be lowered later */
  }

  /* - if destination mesh had shapekeys, move them somewhere safe, and set up placeholders
   *   with arrays that are large enough to hold shapekey data for all meshes
   * - if destination mesh didn't have shapekeys, but we encountered some in the meshes we're
   *   joining, set up a new keyblock and assign to the mesh
   */
  if (key) {
    /* make a duplicate copy that will only be used here... (must remember to free it!) */
    nkey = BKE_key_copy(bmain, key);

    /* for all keys in old block, clear data-arrays */
    for (kb = key->block.first; kb; kb = kb->next) {
      if (kb->data) {
        MEM_freeN(kb->data);
      }
      kb->data = MEM_callocN(sizeof(float) * 3 * totvert, "join_shapekey");
      kb->totelem = totvert;
    }
  }
  else if (haskey) {
    /* add a new key-block and add to the mesh */
    key = me->key = BKE_key_add(bmain, (ID *)me);
    key->type = KEY_RELATIVE;
  }

  /* First pass over objects: Copying materials, vertex-groups & face-maps across. */
  CTX_DATA_BEGIN (C, Object *, ob_iter, selected_editable_objects) {
    /* only act if a mesh, and not the one we're joining to */
    if ((ob != ob_iter) && (ob_iter->type == OB_MESH)) {
      me = ob_iter->data;

      /* Join this object's vertex groups to the base one's */
      for (dg = ob_iter->defbase.first; dg; dg = dg->next) {
        /* See if this group exists in the object (if it doesn't, add it to the end) */
        if (!BKE_object_defgroup_find_name(ob, dg->name)) {
          odg = MEM_mallocN(sizeof(bDeformGroup), "join deformGroup");
          memcpy(odg, dg, sizeof(bDeformGroup));
          BLI_addtail(&ob->defbase, odg);
        }
      }
      if (ob->defbase.first && ob->actdef == 0) {
        ob->actdef = 1;
      }

      /* Join this object's face maps to the base one's. */
      LISTBASE_FOREACH (bFaceMap *, fmap, &ob_iter->fmaps) {
        /* See if this group exists in the object (if it doesn't, add it to the end) */
        if (BKE_object_facemap_find_name(ob, fmap->name) == NULL) {
          bFaceMap *fmap_new = MEM_mallocN(sizeof(bFaceMap), "join faceMap");
          memcpy(fmap_new, fmap, sizeof(bFaceMap));
          BLI_addtail(&ob->fmaps, fmap_new);
        }
      }
      if (ob->fmaps.first && ob->actfmap == 0) {
        ob->actfmap = 1;
      }

      if (me->totvert) {
        /* Add this object's materials to the base one's if they don't exist already
         * (but only if limits not exceeded yet) */
        if (totcol < MAXMAT) {
          for (a = 1; a <= ob_iter->totcol; a++) {
            ma = BKE_object_material_get(ob_iter, a);

            for (b = 0; b < totcol; b++) {
              if (ma == matar[b]) {
                break;
              }
            }
            if (b == totcol) {
              matar[b] = ma;
              if (ma) {
                id_us_plus(&ma->id);
              }
              totcol++;
            }
            if (totcol >= MAXMAT) {
              break;
            }
          }
        }

        /* if this mesh has shapekeys,
         * check if destination mesh already has matching entries too */
        if (me->key && key) {
          /* for remapping KeyBlock.relative */
          int *index_map = MEM_mallocN(sizeof(int) * me->key->totkey, __func__);
          KeyBlock **kb_map = MEM_mallocN(sizeof(KeyBlock *) * me->key->totkey, __func__);

          for (kb = me->key->block.first, i = 0; kb; kb = kb->next, i++) {
            BLI_assert(i < me->key->totkey);

            kbn = BKE_keyblock_find_name(key, kb->name);
            /* if key doesn't exist in destination mesh, add it */
            if (kbn) {
              index_map[i] = BLI_findindex(&key->block, kbn);
            }
            else {
              index_map[i] = key->totkey;

              kbn = BKE_keyblock_add(key, kb->name);

              BKE_keyblock_copy_settings(kbn, kb);

              /* adjust settings to fit (allocate a new data-array) */
              kbn->data = MEM_callocN(sizeof(float) * 3 * totvert, "joined_shapekey");
              kbn->totelem = totvert;
            }

            kb_map[i] = kbn;
          }

          /* remap relative index values */
          for (kb = me->key->block.first, i = 0; kb; kb = kb->next, i++) {
            /* sanity check, should always be true */
            if (LIKELY(kb->relative < me->key->totkey)) {
              kb_map[i]->relative = index_map[kb->relative];
            }
          }

          MEM_freeN(index_map);
          MEM_freeN(kb_map);
        }
      }
    }
  }
  CTX_DATA_END;

  /* setup new data for destination mesh */
  CustomData_reset(&vdata);
  CustomData_reset(&edata);
  CustomData_reset(&fdata);
  CustomData_reset(&ldata);
  CustomData_reset(&pdata);

  mvert = CustomData_add_layer(&vdata, CD_MVERT, CD_CALLOC, NULL, totvert);
  medge = CustomData_add_layer(&edata, CD_MEDGE, CD_CALLOC, NULL, totedge);
  mloop = CustomData_add_layer(&ldata, CD_MLOOP, CD_CALLOC, NULL, totloop);
  mpoly = CustomData_add_layer(&pdata, CD_MPOLY, CD_CALLOC, NULL, totpoly);

  vertofs = 0;
  edgeofs = 0;
  loopofs = 0;
  polyofs = 0;

  /* inverse transform for all selected meshes in this object */
  invert_m4_m4(imat, ob->obmat);

  /* Add back active mesh first.
   * This allows to keep things similar as they were, as much as possible
   * (i.e. data from active mesh will remain first ones in new result of the merge,
   * in same order for CD layers, etc). See also T50084.
   */
  join_mesh_single(depsgraph,
                   bmain,
                   scene,
                   ob,
                   ob,
                   imat,
                   &mvert,
                   &medge,
                   &mloop,
                   &mpoly,
                   &vdata,
                   &edata,
                   &ldata,
                   &pdata,
                   totvert,
                   totedge,
                   totloop,
                   totpoly,
                   key,
                   nkey,
                   matar,
                   matmap,
                   totcol,
                   &vertofs,
                   &edgeofs,
                   &loopofs,
                   &polyofs);

  CTX_DATA_BEGIN (C, Object *, ob_iter, selected_editable_objects) {
    if (ob_iter == ob) {
      continue;
    }
    /* only join if this is a mesh */
    if (ob_iter->type == OB_MESH) {
      join_mesh_single(depsgraph,
                       bmain,
                       scene,
                       ob,
                       ob_iter,
                       imat,
                       &mvert,
                       &medge,
                       &mloop,
                       &mpoly,
                       &vdata,
                       &edata,
                       &ldata,
                       &pdata,
                       totvert,
                       totedge,
                       totloop,
                       totpoly,
                       key,
                       nkey,
                       matar,
                       matmap,
                       totcol,
                       &vertofs,
                       &edgeofs,
                       &loopofs,
                       &polyofs);

      /* free base, now that data is merged */
      if (ob_iter != ob) {
        ED_object_base_free_and_unlink(bmain, scene, ob_iter);
      }
    }
  }
  CTX_DATA_END;

  /* return to mesh we're merging to */
  me = ob->data;

  CustomData_free(&me->vdata, me->totvert);
  CustomData_free(&me->edata, me->totedge);
  CustomData_free(&me->ldata, me->totloop);
  CustomData_free(&me->pdata, me->totpoly);

  me->totvert = totvert;
  me->totedge = totedge;
  me->totloop = totloop;
  me->totpoly = totpoly;

  me->vdata = vdata;
  me->edata = edata;
  me->ldata = ldata;
  me->pdata = pdata;

  /* tessface data removed above, no need to update */
  BKE_mesh_update_customdata_pointers(me, false);

  /* update normals in case objects with non-uniform scale are joined */
  BKE_mesh_calc_normals(me);

  /* old material array */
  for (a = 1; a <= ob->totcol; a++) {
    ma = ob->mat[a - 1];
    if (ma) {
      id_us_min(&ma->id);
    }
  }
  for (a = 1; a <= me->totcol; a++) {
    ma = me->mat[a - 1];
    if (ma) {
      id_us_min(&ma->id);
    }
  }
  MEM_SAFE_FREE(ob->mat);
  MEM_SAFE_FREE(ob->matbits);
  MEM_SAFE_FREE(me->mat);

  if (totcol) {
    me->mat = matar;
    ob->mat = MEM_callocN(sizeof(*ob->mat) * totcol, "join obmatar");
    ob->matbits = MEM_callocN(sizeof(*ob->matbits) * totcol, "join obmatbits");
    MEM_freeN(matmap);
  }

  ob->totcol = me->totcol = totcol;

  /* other mesh users */
  BKE_objects_materials_test_all(bmain, (ID *)me);

  /* free temp copy of destination shapekeys (if applicable) */
  if (nkey) {
    /* We can assume nobody is using that ID currently. */
    BKE_id_free_ex(bmain, nkey, LIB_ID_FREE_NO_UI_USER, false);
  }

  /* ensure newly inserted keys are time sorted */
  if (key && (key->type != KEY_RELATIVE)) {
    BKE_key_sort(key);
  }

  /* Due to dependnecy cycle some other object might access old derived data. */
  BKE_object_free_derived_caches(ob);

  DEG_relations_tag_update(bmain); /* removed objects, need to rebuild dag */

  DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);

  return OPERATOR_FINISHED;
}

/* -------------------------------------------------------------------- */
/** \name Join as Shapes
 * \{ */

/* Append selected meshes vertex locations as shapes of the active mesh,
 * return 0 if no join is made (error) and 1 of the join is done */

int ED_mesh_shapes_join_objects_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob_active = CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Mesh *me = (Mesh *)ob_active->data;
  Mesh *selme = NULL;
  Mesh *me_deformed = NULL;
  Key *key = me->key;
  KeyBlock *kb;
  bool ok = false, nonequal_verts = false;

  CTX_DATA_BEGIN (C, Object *, ob_iter, selected_editable_objects) {
    if (ob_iter == ob_active) {
      continue;
    }

    if (ob_iter->type == OB_MESH) {
      selme = (Mesh *)ob_iter->data;

      if (selme->totvert == me->totvert) {
        ok = true;
      }
      else {
        nonequal_verts = 1;
      }
    }
  }
  CTX_DATA_END;

  if (!ok) {
    if (nonequal_verts) {
      BKE_report(op->reports, RPT_WARNING, "Selected meshes must have equal numbers of vertices");
    }
    else {
      BKE_report(op->reports,
                 RPT_WARNING,
                 "No additional selected meshes with equal vertex count to join");
    }
    return OPERATOR_CANCELLED;
  }

  if (key == NULL) {
    key = me->key = BKE_key_add(bmain, (ID *)me);
    key->type = KEY_RELATIVE;

    /* first key added, so it was the basis. initialize it with the existing mesh */
    kb = BKE_keyblock_add(key, NULL);
    BKE_keyblock_convert_from_mesh(me, key, kb);
  }

  /* now ready to add new keys from selected meshes */
  CTX_DATA_BEGIN (C, Object *, ob_iter, selected_editable_objects) {
    if (ob_iter == ob_active) {
      continue;
    }

    if (ob_iter->type == OB_MESH) {
      selme = (Mesh *)ob_iter->data;

      if (selme->totvert == me->totvert) {
        Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
        Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob_iter);

        me_deformed = mesh_get_eval_deform(depsgraph, scene_eval, ob_eval, &CD_MASK_BAREMESH);

        if (!me_deformed) {
          continue;
        }

        kb = BKE_keyblock_add(key, ob_iter->id.name + 2);

        BKE_mesh_runtime_eval_to_meshkey(me_deformed, me, kb);
      }
    }
  }
  CTX_DATA_END;

  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);

  return OPERATOR_FINISHED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Topology Mirror API
 * \{ */

static MirrTopoStore_t mesh_topo_store = {NULL, -1. - 1, -1};

BLI_INLINE void mesh_mirror_topo_table_get_meshes(Object *ob,
                                                  Mesh *me_eval,
                                                  Mesh **r_me_mirror,
                                                  BMEditMesh **r_em_mirror)
{
  Mesh *me_mirror = NULL;
  BMEditMesh *em_mirror = NULL;

  Mesh *me = ob->data;
  if (me_eval != NULL) {
    me_mirror = me_eval;
  }
  else if (me->edit_mesh != NULL) {
    em_mirror = me->edit_mesh;
  }
  else {
    me_mirror = me;
  }

  *r_me_mirror = me_mirror;
  *r_em_mirror = em_mirror;
}

/**
 * Mode is 's' start, or 'e' end, or 'u' use
 * if end, ob can be NULL.
 * \note This is supposed return -1 on error,
 * which callers are currently checking for, but is not used so far.
 */
void ED_mesh_mirror_topo_table_begin(Object *ob, Mesh *me_eval)
{
  Mesh *me_mirror;
  BMEditMesh *em_mirror;
  mesh_mirror_topo_table_get_meshes(ob, me_eval, &me_mirror, &em_mirror);

  ED_mesh_mirrtopo_init(em_mirror, me_mirror, &mesh_topo_store, false);
}

void ED_mesh_mirror_topo_table_end(Object *UNUSED(ob))
{
  /* TODO: store this in object/object-data (keep unused argument for now). */
  ED_mesh_mirrtopo_free(&mesh_topo_store);
}

static int ed_mesh_mirror_topo_table_update(Object *ob, Mesh *me_eval)
{
  Mesh *me_mirror;
  BMEditMesh *em_mirror;
  mesh_mirror_topo_table_get_meshes(ob, me_eval, &me_mirror, &em_mirror);

  if (ED_mesh_mirrtopo_recalc_check(em_mirror, me_mirror, &mesh_topo_store)) {
    ED_mesh_mirror_topo_table_begin(ob, me_eval);
  }
  return 0;
}

/** \} */

static int mesh_get_x_mirror_vert_spatial(Object *ob, Mesh *me_eval, int index)
{
  Mesh *me = ob->data;
  MVert *mvert = me_eval ? me_eval->mvert : me->mvert;
  float vec[3];

  mvert = &mvert[index];
  vec[0] = -mvert->co[0];
  vec[1] = mvert->co[1];
  vec[2] = mvert->co[2];

  return ED_mesh_mirror_spatial_table_lookup(ob, NULL, me_eval, vec);
}

static int mesh_get_x_mirror_vert_topo(Object *ob, Mesh *mesh, int index)
{
  if (ed_mesh_mirror_topo_table_update(ob, mesh) == -1) {
    return -1;
  }

  return mesh_topo_store.index_lookup[index];
}

int mesh_get_x_mirror_vert(Object *ob, Mesh *me_eval, int index, const bool use_topology)
{
  if (use_topology) {
    return mesh_get_x_mirror_vert_topo(ob, me_eval, index);
  }
  else {
    return mesh_get_x_mirror_vert_spatial(ob, me_eval, index);
  }
}

static BMVert *editbmesh_get_x_mirror_vert_spatial(Object *ob, BMEditMesh *em, const float co[3])
{
  float vec[3];
  int i;

  /* ignore nan verts */
  if ((isfinite(co[0]) == false) || (isfinite(co[1]) == false) || (isfinite(co[2]) == false)) {
    return NULL;
  }

  vec[0] = -co[0];
  vec[1] = co[1];
  vec[2] = co[2];

  i = ED_mesh_mirror_spatial_table_lookup(ob, em, NULL, vec);
  if (i != -1) {
    return BM_vert_at_index(em->bm, i);
  }
  return NULL;
}

static BMVert *editbmesh_get_x_mirror_vert_topo(Object *ob,
                                                struct BMEditMesh *em,
                                                BMVert *eve,
                                                int index)
{
  intptr_t poinval;
  if (ed_mesh_mirror_topo_table_update(ob, NULL) == -1) {
    return NULL;
  }

  if (index == -1) {
    BMIter iter;
    BMVert *v;

    index = 0;
    BM_ITER_MESH (v, &iter, em->bm, BM_VERTS_OF_MESH) {
      if (v == eve) {
        break;
      }
      index++;
    }

    if (index == em->bm->totvert) {
      return NULL;
    }
  }

  poinval = mesh_topo_store.index_lookup[index];

  if (poinval != -1) {
    return (BMVert *)(poinval);
  }
  return NULL;
}

BMVert *editbmesh_get_x_mirror_vert(Object *ob,
                                    struct BMEditMesh *em,
                                    BMVert *eve,
                                    const float co[3],
                                    int index,
                                    const bool use_topology)
{
  if (use_topology) {
    return editbmesh_get_x_mirror_vert_topo(ob, em, eve, index);
  }
  else {
    return editbmesh_get_x_mirror_vert_spatial(ob, em, co);
  }
}

/**
 * Wrapper for object-mode/edit-mode.
 *
 * call #BM_mesh_elem_table_ensure first for editmesh.
 */
int ED_mesh_mirror_get_vert(Object *ob, int index)
{
  Mesh *me = ob->data;
  BMEditMesh *em = me->edit_mesh;
  bool use_topology = (me->editflag & ME_EDIT_MIRROR_TOPO) != 0;
  int index_mirr;

  if (em) {
    BMVert *eve, *eve_mirr;
    eve = BM_vert_at_index(em->bm, index);
    eve_mirr = editbmesh_get_x_mirror_vert(ob, em, eve, eve->co, index, use_topology);
    index_mirr = eve_mirr ? BM_elem_index_get(eve_mirr) : -1;
  }
  else {
    index_mirr = mesh_get_x_mirror_vert(ob, NULL, index, use_topology);
  }

  return index_mirr;
}

#if 0

static float *editmesh_get_mirror_uv(
    BMEditMesh *em, int axis, float *uv, float *mirrCent, float *face_cent)
{
  float vec[2];
  float cent_vec[2];
  float cent[2];

  /* ignore nan verts */
  if (isnan(uv[0]) || !isfinite(uv[0]) || isnan(uv[1]) || !isfinite(uv[1])) {
    return NULL;
  }

  if (axis) {
    vec[0] = uv[0];
    vec[1] = -((uv[1]) - mirrCent[1]) + mirrCent[1];

    cent_vec[0] = face_cent[0];
    cent_vec[1] = -((face_cent[1]) - mirrCent[1]) + mirrCent[1];
  }
  else {
    vec[0] = -((uv[0]) - mirrCent[0]) + mirrCent[0];
    vec[1] = uv[1];

    cent_vec[0] = -((face_cent[0]) - mirrCent[0]) + mirrCent[0];
    cent_vec[1] = face_cent[1];
  }

  /* TODO - Optimize */
  {
    BMIter iter;
    BMFace *efa;

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      uv_poly_center(efa, cent, cd_loop_uv_offset);

      if ((fabsf(cent[0] - cent_vec[0]) < 0.001f) && (fabsf(cent[1] - cent_vec[1]) < 0.001f)) {
        BMIter liter;
        BMLoop *l;

        BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
          MLoopUV *luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
          if ((fabsf(luv->uv[0] - vec[0]) < 0.001f) && (fabsf(luv->uv[1] - vec[1]) < 0.001f)) {
            return luv->uv;
          }
        }
      }
    }
  }

  return NULL;
}

#endif

static uint mirror_facehash(const void *ptr)
{
  const MFace *mf = ptr;
  uint v0, v1;

  if (mf->v4) {
    v0 = MIN4(mf->v1, mf->v2, mf->v3, mf->v4);
    v1 = MAX4(mf->v1, mf->v2, mf->v3, mf->v4);
  }
  else {
    v0 = MIN3(mf->v1, mf->v2, mf->v3);
    v1 = MAX3(mf->v1, mf->v2, mf->v3);
  }

  return ((v0 * 39) ^ (v1 * 31));
}

static int mirror_facerotation(MFace *a, MFace *b)
{
  if (b->v4) {
    if (a->v1 == b->v1 && a->v2 == b->v2 && a->v3 == b->v3 && a->v4 == b->v4) {
      return 0;
    }
    else if (a->v4 == b->v1 && a->v1 == b->v2 && a->v2 == b->v3 && a->v3 == b->v4) {
      return 1;
    }
    else if (a->v3 == b->v1 && a->v4 == b->v2 && a->v1 == b->v3 && a->v2 == b->v4) {
      return 2;
    }
    else if (a->v2 == b->v1 && a->v3 == b->v2 && a->v4 == b->v3 && a->v1 == b->v4) {
      return 3;
    }
  }
  else {
    if (a->v1 == b->v1 && a->v2 == b->v2 && a->v3 == b->v3) {
      return 0;
    }
    else if (a->v3 == b->v1 && a->v1 == b->v2 && a->v2 == b->v3) {
      return 1;
    }
    else if (a->v2 == b->v1 && a->v3 == b->v2 && a->v1 == b->v3) {
      return 2;
    }
  }

  return -1;
}

static bool mirror_facecmp(const void *a, const void *b)
{
  return (mirror_facerotation((MFace *)a, (MFace *)b) == -1);
}

/* This is a Mesh-based copy of mesh_get_x_mirror_faces() */
int *mesh_get_x_mirror_faces(Object *ob, BMEditMesh *em, Mesh *me_eval)
{
  Mesh *me = ob->data;
  MVert *mv, *mvert;
  MFace mirrormf, *mf, *hashmf, *mface;
  GHash *fhash;
  int *mirrorverts, *mirrorfaces;

  BLI_assert(em == NULL); /* Does not work otherwise, currently... */

  const bool use_topology = (me->editflag & ME_EDIT_MIRROR_TOPO) != 0;
  const int totvert = me_eval ? me_eval->totvert : me->totvert;
  const int totface = me_eval ? me_eval->totface : me->totface;
  int a;

  mirrorverts = MEM_callocN(sizeof(int) * totvert, "MirrorVerts");
  mirrorfaces = MEM_callocN(sizeof(int) * 2 * totface, "MirrorFaces");

  mvert = me_eval ? me_eval->mvert : me->mvert;
  mface = me_eval ? me_eval->mface : me->mface;

  ED_mesh_mirror_spatial_table_begin(ob, em, me_eval);

  for (a = 0, mv = mvert; a < totvert; a++, mv++) {
    mirrorverts[a] = mesh_get_x_mirror_vert(ob, me_eval, a, use_topology);
  }

  ED_mesh_mirror_spatial_table_end(ob);

  fhash = BLI_ghash_new_ex(mirror_facehash, mirror_facecmp, "mirror_facehash gh", me->totface);
  for (a = 0, mf = mface; a < totface; a++, mf++) {
    BLI_ghash_insert(fhash, mf, mf);
  }

  for (a = 0, mf = mface; a < totface; a++, mf++) {
    mirrormf.v1 = mirrorverts[mf->v3];
    mirrormf.v2 = mirrorverts[mf->v2];
    mirrormf.v3 = mirrorverts[mf->v1];
    mirrormf.v4 = (mf->v4) ? mirrorverts[mf->v4] : 0;

    /* make sure v4 is not 0 if a quad */
    if (mf->v4 && mirrormf.v4 == 0) {
      SWAP(uint, mirrormf.v1, mirrormf.v3);
      SWAP(uint, mirrormf.v2, mirrormf.v4);
    }

    hashmf = BLI_ghash_lookup(fhash, &mirrormf);
    if (hashmf) {
      mirrorfaces[a * 2] = hashmf - mface;
      mirrorfaces[a * 2 + 1] = mirror_facerotation(&mirrormf, hashmf);
    }
    else {
      mirrorfaces[a * 2] = -1;
    }
  }

  BLI_ghash_free(fhash, NULL, NULL);
  MEM_freeN(mirrorverts);

  return mirrorfaces;
}

/* selection, vertex and face */
/* returns 0 if not found, otherwise 1 */

/**
 * Face selection in object mode,
 * currently only weight-paint and vertex-paint use this.
 *
 * \return boolean true == Found
 */
bool ED_mesh_pick_face(bContext *C, Object *ob, const int mval[2], uint dist_px, uint *r_index)
{
  ViewContext vc;
  Mesh *me = ob->data;

  BLI_assert(me && GS(me->id.name) == ID_ME);

  if (!me || me->totpoly == 0) {
    return false;
  }

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ED_view3d_viewcontext_init(C, &vc, depsgraph);
  ED_view3d_select_id_validate(&vc);

  if (dist_px) {
    /* sample rect to increase chances of selecting, so that when clicking
     * on an edge in the backbuf, we can still select a face */
    *r_index = DRW_select_buffer_find_nearest_to_point(
        vc.depsgraph, vc.region, vc.v3d, mval, 1, me->totpoly + 1, &dist_px);
  }
  else {
    /* sample only on the exact position */
    *r_index = DRW_select_buffer_sample_point(vc.depsgraph, vc.region, vc.v3d, mval);
  }

  if ((*r_index) == 0 || (*r_index) > (uint)me->totpoly) {
    return false;
  }

  (*r_index)--;

  return true;
}

static void ed_mesh_pick_face_vert__mpoly_find(
    /* context */
    struct ARegion *region,
    const float mval[2],
    /* mesh data (evaluated) */
    const MPoly *mp,
    const MVert *mvert,
    const MLoop *mloop,
    /* return values */
    float *r_len_best,
    int *r_v_idx_best)
{
  const MLoop *ml;
  int j = mp->totloop;
  for (ml = &mloop[mp->loopstart]; j--; ml++) {
    float sco[2];
    const int v_idx = ml->v;
    const float *co = mvert[v_idx].co;
    if (ED_view3d_project_float_object(region, co, sco, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) {
      const float len_test = len_manhattan_v2v2(mval, sco);
      if (len_test < *r_len_best) {
        *r_len_best = len_test;
        *r_v_idx_best = v_idx;
      }
    }
  }
}
/**
 * Use when the back buffer stores face index values. but we want a vert.
 * This gets the face then finds the closest vertex to mval.
 */
bool ED_mesh_pick_face_vert(
    bContext *C, Object *ob, const int mval[2], uint dist_px, uint *r_index)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  uint poly_index;
  Mesh *me = ob->data;

  BLI_assert(me && GS(me->id.name) == ID_ME);

  if (ED_mesh_pick_face(C, ob, mval, dist_px, &poly_index)) {
    Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
    Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
    struct ARegion *region = CTX_wm_region(C);

    /* derived mesh to find deformed locations */
    Mesh *me_eval = mesh_get_eval_final(
        depsgraph, scene_eval, ob_eval, &CD_MASK_BAREMESH_ORIGINDEX);

    int v_idx_best = ORIGINDEX_NONE;

    /* find the vert closest to 'mval' */
    const float mval_f[2] = {UNPACK2(mval)};
    float len_best = FLT_MAX;

    MPoly *me_eval_mpoly;
    MLoop *me_eval_mloop;
    MVert *me_eval_mvert;
    uint me_eval_mpoly_len;
    const int *index_mp_to_orig;

    me_eval_mpoly = me_eval->mpoly;
    me_eval_mloop = me_eval->mloop;
    me_eval_mvert = me_eval->mvert;

    me_eval_mpoly_len = me_eval->totpoly;

    index_mp_to_orig = CustomData_get_layer(&me_eval->pdata, CD_ORIGINDEX);

    /* tag all verts using this face */
    if (index_mp_to_orig) {
      uint i;

      for (i = 0; i < me_eval_mpoly_len; i++) {
        if (index_mp_to_orig[i] == poly_index) {
          ed_mesh_pick_face_vert__mpoly_find(region,
                                             mval_f,
                                             &me_eval_mpoly[i],
                                             me_eval_mvert,
                                             me_eval_mloop,
                                             &len_best,
                                             &v_idx_best);
        }
      }
    }
    else {
      if (poly_index < me_eval_mpoly_len) {
        ed_mesh_pick_face_vert__mpoly_find(region,
                                           mval_f,
                                           &me_eval_mpoly[poly_index],
                                           me_eval_mvert,
                                           me_eval_mloop,
                                           &len_best,
                                           &v_idx_best);
      }
    }

    /* map 'dm -> me' r_index if possible */
    if (v_idx_best != ORIGINDEX_NONE) {
      const int *index_mv_to_orig;
      index_mv_to_orig = CustomData_get_layer(&me_eval->vdata, CD_ORIGINDEX);
      if (index_mv_to_orig) {
        v_idx_best = index_mv_to_orig[v_idx_best];
      }
    }

    if ((v_idx_best != ORIGINDEX_NONE) && (v_idx_best < me->totvert)) {
      *r_index = v_idx_best;
      return true;
    }
  }

  return false;
}

/**
 * Vertex selection in object mode,
 * currently only weight paint uses this.
 *
 * \return boolean true == Found
 */
typedef struct VertPickData {
  const MVert *mvert;
  const float *mval_f; /* [2] */
  ARegion *region;

  /* runtime */
  float len_best;
  int v_idx_best;
} VertPickData;

static void ed_mesh_pick_vert__mapFunc(void *userData,
                                       int index,
                                       const float co[3],
                                       const float UNUSED(no_f[3]),
                                       const short UNUSED(no_s[3]))
{
  VertPickData *data = userData;
  if ((data->mvert[index].flag & ME_HIDE) == 0) {
    float sco[2];

    if (ED_view3d_project_float_object(data->region, co, sco, V3D_PROJ_TEST_CLIP_DEFAULT) ==
        V3D_PROJ_RET_OK) {
      const float len = len_manhattan_v2v2(data->mval_f, sco);
      if (len < data->len_best) {
        data->len_best = len;
        data->v_idx_best = index;
      }
    }
  }
}
bool ED_mesh_pick_vert(
    bContext *C, Object *ob, const int mval[2], uint dist_px, bool use_zbuf, uint *r_index)
{
  ViewContext vc;
  Mesh *me = ob->data;

  BLI_assert(me && GS(me->id.name) == ID_ME);

  if (!me || me->totvert == 0) {
    return false;
  }

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ED_view3d_viewcontext_init(C, &vc, depsgraph);
  ED_view3d_select_id_validate(&vc);

  if (use_zbuf) {
    if (dist_px > 0) {
      /* sample rect to increase chances of selecting, so that when clicking
       * on an face in the backbuf, we can still select a vert */
      *r_index = DRW_select_buffer_find_nearest_to_point(
          vc.depsgraph, vc.region, vc.v3d, mval, 1, me->totvert + 1, &dist_px);
    }
    else {
      /* sample only on the exact position */
      *r_index = DRW_select_buffer_sample_point(vc.depsgraph, vc.region, vc.v3d, mval);
    }

    if ((*r_index) == 0 || (*r_index) > (uint)me->totvert) {
      return false;
    }

    (*r_index)--;
  }
  else {
    Scene *scene_eval = DEG_get_evaluated_scene(vc.depsgraph);
    Object *ob_eval = DEG_get_evaluated_object(vc.depsgraph, ob);

    /* derived mesh to find deformed locations */
    Mesh *me_eval = mesh_get_eval_final(vc.depsgraph, scene_eval, ob_eval, &CD_MASK_BAREMESH);
    ARegion *region = vc.region;
    RegionView3D *rv3d = region->regiondata;

    /* find the vert closest to 'mval' */
    const float mval_f[2] = {(float)mval[0], (float)mval[1]};

    VertPickData data = {NULL};

    ED_view3d_init_mats_rv3d(ob, rv3d);

    if (me_eval == NULL) {
      return false;
    }

    /* setup data */
    data.mvert = me->mvert;
    data.region = region;
    data.mval_f = mval_f;
    data.len_best = FLT_MAX;
    data.v_idx_best = -1;

    BKE_mesh_foreach_mapped_vert(me_eval, ed_mesh_pick_vert__mapFunc, &data, MESH_FOREACH_NOP);

    if (data.v_idx_best == -1) {
      return false;
    }

    *r_index = data.v_idx_best;
  }

  return true;
}

MDeformVert *ED_mesh_active_dvert_get_em(Object *ob, BMVert **r_eve)
{
  if (ob->mode & OB_MODE_EDIT && ob->type == OB_MESH && ob->defbase.first) {
    Mesh *me = ob->data;
    BMesh *bm = me->edit_mesh->bm;
    const int cd_dvert_offset = CustomData_get_offset(&bm->vdata, CD_MDEFORMVERT);

    if (cd_dvert_offset != -1) {
      BMVert *eve = BM_mesh_active_vert_get(bm);

      if (eve) {
        if (r_eve) {
          *r_eve = eve;
        }
        return BM_ELEM_CD_GET_VOID_P(eve, cd_dvert_offset);
      }
    }
  }

  if (r_eve) {
    *r_eve = NULL;
  }
  return NULL;
}

MDeformVert *ED_mesh_active_dvert_get_ob(Object *ob, int *r_index)
{
  Mesh *me = ob->data;
  int index = BKE_mesh_mselect_active_get(me, ME_VSEL);
  if (r_index) {
    *r_index = index;
  }
  if (index == -1 || me->dvert == NULL) {
    return NULL;
  }
  else {
    return me->dvert + index;
  }
}

MDeformVert *ED_mesh_active_dvert_get_only(Object *ob)
{
  if (ob->type == OB_MESH) {
    if (ob->mode & OB_MODE_EDIT) {
      return ED_mesh_active_dvert_get_em(ob, NULL);
    }
    else {
      return ED_mesh_active_dvert_get_ob(ob, NULL);
    }
  }
  else {
    return NULL;
  }
}

void EDBM_mesh_stats_multi(struct Object **objects,
                           const uint objects_len,
                           int totelem[3],
                           int totelem_sel[3])
{
  if (totelem) {
    totelem[0] = 0;
    totelem[1] = 0;
    totelem[2] = 0;
  }
  if (totelem_sel) {
    totelem_sel[0] = 0;
    totelem_sel[1] = 0;
    totelem_sel[2] = 0;
  }

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMesh *bm = em->bm;
    if (totelem) {
      totelem[0] += bm->totvert;
      totelem[1] += bm->totedge;
      totelem[2] += bm->totface;
    }
    if (totelem_sel) {
      totelem_sel[0] += bm->totvertsel;
      totelem_sel[1] += bm->totedgesel;
      totelem_sel[2] += bm->totfacesel;
    }
  }
}

void EDBM_mesh_elem_index_ensure_multi(Object **objects, const uint objects_len, const char htype)
{
  int elem_offset[4] = {0, 0, 0, 0};
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMesh *bm = em->bm;
    BM_mesh_elem_index_ensure_ex(bm, htype, elem_offset);
  }
}
