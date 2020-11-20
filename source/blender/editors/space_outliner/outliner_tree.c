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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spoutliner
 */

#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_camera_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_hair_types.h"
#include "DNA_key_types.h"
#include "DNA_light_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_shader_fx_types.h"
#include "DNA_simulation_types.h"
#include "DNA_speaker_types.h"
#include "DNA_volume_types.h"
#include "DNA_world_types.h"

#include "BLI_blenlib.h"
#include "BLI_fnmatch.h"
#include "BLI_listbase.h"
#include "BLI_mempool.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_armature.h"
#include "BKE_fcurve_driver.h"
#include "BKE_idtype.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_outliner_treehash.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "SEQ_sequencer.h"

#include "UI_interface.h"

#include "outliner_intern.h"
#include "tree/tree_display.h"

#ifdef WIN32
#  include "BLI_math_base.h" /* M_PI */
#endif

/* prototypes */
static TreeElement *outliner_add_collection_recursive(SpaceOutliner *space_outliner,
                                                      Collection *collection,
                                                      TreeElement *ten);
static int outliner_exclude_filter_get(const SpaceOutliner *space_outliner);

/* ********************************************************* */
/* Persistent Data */

static void outliner_storage_cleanup(SpaceOutliner *space_outliner)
{
  BLI_mempool *ts = space_outliner->treestore;

  if (ts) {
    TreeStoreElem *tselem;
    int unused = 0;

    /* each element used once, for ID blocks with more users to have each a treestore */
    BLI_mempool_iter iter;

    BLI_mempool_iternew(ts, &iter);
    while ((tselem = BLI_mempool_iterstep(&iter))) {
      tselem->used = 0;
    }

    /* cleanup only after reading file or undo step, and always for
     * RNA data-blocks view in order to save memory */
    if (space_outliner->storeflag & SO_TREESTORE_CLEANUP) {
      space_outliner->storeflag &= ~SO_TREESTORE_CLEANUP;

      BLI_mempool_iternew(ts, &iter);
      while ((tselem = BLI_mempool_iterstep(&iter))) {
        if (tselem->id == NULL) {
          unused++;
        }
      }

      if (unused) {
        if (BLI_mempool_len(ts) == unused) {
          BLI_mempool_destroy(ts);
          space_outliner->treestore = NULL;
          if (space_outliner->treehash) {
            BKE_outliner_treehash_free(space_outliner->treehash);
            space_outliner->treehash = NULL;
          }
        }
        else {
          TreeStoreElem *tsenew;
          BLI_mempool *new_ts = BLI_mempool_create(
              sizeof(TreeStoreElem), BLI_mempool_len(ts) - unused, 512, BLI_MEMPOOL_ALLOW_ITER);
          BLI_mempool_iternew(ts, &iter);
          while ((tselem = BLI_mempool_iterstep(&iter))) {
            if (tselem->id) {
              tsenew = BLI_mempool_alloc(new_ts);
              *tsenew = *tselem;
            }
          }
          BLI_mempool_destroy(ts);
          space_outliner->treestore = new_ts;
          if (space_outliner->treehash) {
            /* update hash table to fix broken pointers */
            BKE_outliner_treehash_rebuild_from_treestore(space_outliner->treehash,
                                                         space_outliner->treestore);
          }
        }
      }
    }
    else if (space_outliner->treehash) {
      BKE_outliner_treehash_clear_used(space_outliner->treehash);
    }
  }
}

static void check_persistent(
    SpaceOutliner *space_outliner, TreeElement *te, ID *id, short type, short nr)
{
  TreeStoreElem *tselem;

  if (space_outliner->treestore == NULL) {
    /* if treestore was not created in readfile.c, create it here */
    space_outliner->treestore = BLI_mempool_create(
        sizeof(TreeStoreElem), 1, 512, BLI_MEMPOOL_ALLOW_ITER);
  }
  if (space_outliner->treehash == NULL) {
    space_outliner->treehash = BKE_outliner_treehash_create_from_treestore(
        space_outliner->treestore);
  }

  /* find any unused tree element in treestore and mark it as used
   * (note that there may be multiple unused elements in case of linked objects) */
  tselem = BKE_outliner_treehash_lookup_unused(space_outliner->treehash, type, nr, id);
  if (tselem) {
    te->store_elem = tselem;
    tselem->used = 1;
    return;
  }

  /* add 1 element to treestore */
  tselem = BLI_mempool_alloc(space_outliner->treestore);
  tselem->type = type;
  tselem->nr = type ? nr : 0;
  tselem->id = id;
  tselem->used = 0;
  tselem->flag = TSE_CLOSED;
  te->store_elem = tselem;
  BKE_outliner_treehash_add_element(space_outliner->treehash, tselem);
}

/* ********************************************************* */
/* Tree Management */

void outliner_free_tree(ListBase *tree)
{
  for (TreeElement *element = tree->first, *element_next; element; element = element_next) {
    element_next = element->next;
    outliner_free_tree_element(element, tree);
  }
}

void outliner_cleanup_tree(SpaceOutliner *space_outliner)
{
  outliner_free_tree(&space_outliner->tree);
  outliner_storage_cleanup(space_outliner);
}

/**
 * Free \a element and its sub-tree and remove its link in \a parent_subtree.
 *
 * \note Does not remove the #TreeStoreElem of \a element!
 * \param parent_subtree: Sub-tree of the parent element, so the list containing \a element.
 */
void outliner_free_tree_element(TreeElement *element, ListBase *parent_subtree)
{
  BLI_assert(BLI_findindex(parent_subtree, element) > -1);
  BLI_remlink(parent_subtree, element);

  outliner_free_tree(&element->subtree);

  if (element->flag & TE_FREE_NAME) {
    MEM_freeN((void *)element->name);
  }
  MEM_freeN(element);
}

/* ********************************************************* */

/* -------------------------------------------------------- */

bool outliner_requires_rebuild_on_select_or_active_change(const SpaceOutliner *space_outliner)
{
  int exclude_flags = outliner_exclude_filter_get(space_outliner);
  /* Need to rebuild tree to re-apply filter if select/active changed while filtering based on
   * select/active. */
  return exclude_flags & (SO_FILTER_OB_STATE_SELECTED | SO_FILTER_OB_STATE_ACTIVE);
}

/**
 * Check if a display mode needs a full rebuild if the open/collapsed state changes.
 * Element types in these modes don't actually add children if collapsed, so the rebuild is needed.
 */
bool outliner_requires_rebuild_on_open_change(const SpaceOutliner *space_outliner)
{
  return ELEM(space_outliner->outlinevis, SO_DATA_API);
}

/* special handling of hierarchical non-lib data */
static void outliner_add_bone(SpaceOutliner *space_outliner,
                              ListBase *lb,
                              ID *id,
                              Bone *curBone,
                              TreeElement *parent,
                              int *a)
{
  TreeElement *te = outliner_add_element(space_outliner, lb, id, parent, TSE_BONE, *a);

  (*a)++;
  te->name = curBone->name;
  te->directdata = curBone;

  for (curBone = curBone->childbase.first; curBone; curBone = curBone->next) {
    outliner_add_bone(space_outliner, &te->subtree, id, curBone, te, a);
  }
}

static bool outliner_animdata_test(AnimData *adt)
{
  if (adt) {
    return (adt->action || adt->drivers.first || adt->nla_tracks.first);
  }
  return false;
}

#ifdef WITH_FREESTYLE
static void outliner_add_line_styles(SpaceOutliner *space_outliner,
                                     ListBase *lb,
                                     Scene *sce,
                                     TreeElement *te)
{
  ViewLayer *view_layer;
  FreestyleLineSet *lineset;

  for (view_layer = sce->view_layers.first; view_layer; view_layer = view_layer->next) {
    for (lineset = view_layer->freestyle_config.linesets.first; lineset; lineset = lineset->next) {
      FreestyleLineStyle *linestyle = lineset->linestyle;
      if (linestyle) {
        linestyle->id.tag |= LIB_TAG_DOIT;
      }
    }
  }
  for (view_layer = sce->view_layers.first; view_layer; view_layer = view_layer->next) {
    for (lineset = view_layer->freestyle_config.linesets.first; lineset; lineset = lineset->next) {
      FreestyleLineStyle *linestyle = lineset->linestyle;
      if (linestyle) {
        if (!(linestyle->id.tag & LIB_TAG_DOIT)) {
          continue;
        }
        linestyle->id.tag &= ~LIB_TAG_DOIT;
        outliner_add_element(space_outliner, lb, linestyle, te, 0, 0);
      }
    }
  }
}
#endif

static void outliner_add_scene_contents(SpaceOutliner *space_outliner,
                                        ListBase *lb,
                                        Scene *sce,
                                        TreeElement *te)
{
  /* View layers */
  TreeElement *ten = outliner_add_element(space_outliner, lb, sce, te, TSE_R_LAYER_BASE, 0);
  ten->name = IFACE_("View Layers");

  ViewLayer *view_layer;
  for (view_layer = sce->view_layers.first; view_layer; view_layer = view_layer->next) {
    TreeElement *tenlay = outliner_add_element(
        space_outliner, &ten->subtree, sce, ten, TSE_R_LAYER, 0);
    tenlay->name = view_layer->name;
    tenlay->directdata = view_layer;
  }

  /* World */
  outliner_add_element(space_outliner, lb, sce->world, te, 0, 0);

  /* Collections */
  ten = outliner_add_element(space_outliner, lb, &sce->id, te, TSE_SCENE_COLLECTION_BASE, 0);
  ten->name = IFACE_("Scene Collection");
  outliner_add_collection_recursive(space_outliner, sce->master_collection, ten);

  /* Objects */
  ten = outliner_add_element(space_outliner, lb, sce, te, TSE_SCENE_OBJECTS_BASE, 0);
  ten->name = IFACE_("Objects");
  FOREACH_SCENE_OBJECT_BEGIN (sce, ob) {
    outliner_add_element(space_outliner, &ten->subtree, ob, ten, 0, 0);
  }
  FOREACH_SCENE_OBJECT_END;
  outliner_make_object_parent_hierarchy(&ten->subtree);

  /* Animation Data */
  if (outliner_animdata_test(sce->adt)) {
    outliner_add_element(space_outliner, lb, sce, te, TSE_ANIM_DATA, 0);
  }
}

/* Can be inlined if necessary. */
static void outliner_add_object_contents(SpaceOutliner *space_outliner,
                                         TreeElement *te,
                                         TreeStoreElem *tselem,
                                         Object *ob)
{
  if (outliner_animdata_test(ob->adt)) {
    outliner_add_element(space_outliner, &te->subtree, ob, te, TSE_ANIM_DATA, 0);
  }

  outliner_add_element(space_outliner,
                       &te->subtree,
                       ob->poselib,
                       te,
                       0,
                       0); /* XXX FIXME.. add a special type for this. */

  if (ob->proxy && !ID_IS_LINKED(ob)) {
    outliner_add_element(space_outliner, &te->subtree, ob->proxy, te, TSE_PROXY, 0);
  }

  outliner_add_element(space_outliner, &te->subtree, ob->data, te, 0, 0);

  if (ob->pose) {
    bArmature *arm = ob->data;
    bPoseChannel *pchan;
    TreeElement *tenla = outliner_add_element(
        space_outliner, &te->subtree, ob, te, TSE_POSE_BASE, 0);

    tenla->name = IFACE_("Pose");

    /* channels undefined in editmode, but we want the 'tenla' pose icon itself */
    if ((arm->edbo == NULL) && (ob->mode & OB_MODE_POSE)) {
      TreeElement *ten;
      int a = 0, const_index = 1000; /* ensure unique id for bone constraints */

      for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next, a++) {
        ten = outliner_add_element(
            space_outliner, &tenla->subtree, ob, tenla, TSE_POSE_CHANNEL, a);
        ten->name = pchan->name;
        ten->directdata = pchan;
        pchan->temp = (void *)ten;

        if (pchan->constraints.first) {
          /* Object *target; */
          bConstraint *con;
          TreeElement *ten1;
          TreeElement *tenla1 = outliner_add_element(
              space_outliner, &ten->subtree, ob, ten, TSE_CONSTRAINT_BASE, 0);
          /* char *str; */

          tenla1->name = IFACE_("Constraints");
          for (con = pchan->constraints.first; con; con = con->next, const_index++) {
            ten1 = outliner_add_element(
                space_outliner, &tenla1->subtree, ob, tenla1, TSE_CONSTRAINT, const_index);
#if 0 /* disabled as it needs to be reworked for recoded constraints system */
            target = get_constraint_target(con, &str);
            if (str && str[0]) {
              ten1->name = str;
            }
            else if (target) {
              ten1->name = target->id.name + 2;
            }
            else {
              ten1->name = con->name;
            }
#endif
            ten1->name = con->name;
            ten1->directdata = con;
            /* possible add all other types links? */
          }
        }
      }
      /* make hierarchy */
      ten = tenla->subtree.first;
      while (ten) {
        TreeElement *nten = ten->next, *par;
        tselem = TREESTORE(ten);
        if (tselem->type == TSE_POSE_CHANNEL) {
          pchan = (bPoseChannel *)ten->directdata;
          if (pchan->parent) {
            BLI_remlink(&tenla->subtree, ten);
            par = (TreeElement *)pchan->parent->temp;
            BLI_addtail(&par->subtree, ten);
            ten->parent = par;
          }
        }
        ten = nten;
      }
    }

    /* Pose Groups */
    if (ob->pose->agroups.first) {
      bActionGroup *agrp;
      TreeElement *ten_bonegrp = outliner_add_element(
          space_outliner, &te->subtree, ob, te, TSE_POSEGRP_BASE, 0);
      int a = 0;

      ten_bonegrp->name = IFACE_("Bone Groups");
      for (agrp = ob->pose->agroups.first; agrp; agrp = agrp->next, a++) {
        TreeElement *ten;
        ten = outliner_add_element(
            space_outliner, &ten_bonegrp->subtree, ob, ten_bonegrp, TSE_POSEGRP, a);
        ten->name = agrp->name;
        ten->directdata = agrp;
      }
    }
  }

  for (int a = 0; a < ob->totcol; a++) {
    outliner_add_element(space_outliner, &te->subtree, ob->mat[a], te, 0, a);
  }

  if (ob->constraints.first) {
    /* Object *target; */
    bConstraint *con;
    TreeElement *ten;
    TreeElement *tenla = outliner_add_element(
        space_outliner, &te->subtree, ob, te, TSE_CONSTRAINT_BASE, 0);
    /* char *str; */
    int a;

    tenla->name = IFACE_("Constraints");
    for (con = ob->constraints.first, a = 0; con; con = con->next, a++) {
      ten = outliner_add_element(space_outliner, &tenla->subtree, ob, tenla, TSE_CONSTRAINT, a);
#if 0 /* disabled due to constraints system targets recode... code here needs review */
      target = get_constraint_target(con, &str);
      if (str && str[0]) {
        ten->name = str;
      }
      else if (target) {
        ten->name = target->id.name + 2;
      }
      else {
        ten->name = con->name;
      }
#endif
      ten->name = con->name;
      ten->directdata = con;
      /* possible add all other types links? */
    }
  }

  if (ob->modifiers.first) {
    ModifierData *md;
    TreeElement *ten_mod = outliner_add_element(
        space_outliner, &te->subtree, ob, te, TSE_MODIFIER_BASE, 0);
    int index;

    ten_mod->name = IFACE_("Modifiers");
    for (index = 0, md = ob->modifiers.first; md; index++, md = md->next) {
      TreeElement *ten = outliner_add_element(
          space_outliner, &ten_mod->subtree, ob, ten_mod, TSE_MODIFIER, index);
      ten->name = md->name;
      ten->directdata = md;

      if (md->type == eModifierType_Lattice) {
        outliner_add_element(space_outliner,
                             &ten->subtree,
                             ((LatticeModifierData *)md)->object,
                             ten,
                             TSE_LINKED_OB,
                             0);
      }
      else if (md->type == eModifierType_Curve) {
        outliner_add_element(space_outliner,
                             &ten->subtree,
                             ((CurveModifierData *)md)->object,
                             ten,
                             TSE_LINKED_OB,
                             0);
      }
      else if (md->type == eModifierType_Armature) {
        outliner_add_element(space_outliner,
                             &ten->subtree,
                             ((ArmatureModifierData *)md)->object,
                             ten,
                             TSE_LINKED_OB,
                             0);
      }
      else if (md->type == eModifierType_Hook) {
        outliner_add_element(space_outliner,
                             &ten->subtree,
                             ((HookModifierData *)md)->object,
                             ten,
                             TSE_LINKED_OB,
                             0);
      }
      else if (md->type == eModifierType_ParticleSystem) {
        ParticleSystem *psys = ((ParticleSystemModifierData *)md)->psys;
        TreeElement *ten_psys;

        ten_psys = outliner_add_element(space_outliner, &ten->subtree, ob, te, TSE_LINKED_PSYS, 0);
        ten_psys->directdata = psys;
        ten_psys->name = psys->part->id.name + 2;
      }
    }
  }

  /* Grease Pencil modifiers. */
  if (!BLI_listbase_is_empty(&ob->greasepencil_modifiers)) {
    TreeElement *ten_mod = outliner_add_element(
        space_outliner, &te->subtree, ob, te, TSE_MODIFIER_BASE, 0);

    ten_mod->name = IFACE_("Modifiers");
    int index;
    LISTBASE_FOREACH_INDEX (GpencilModifierData *, md, &ob->greasepencil_modifiers, index) {
      TreeElement *ten = outliner_add_element(
          space_outliner, &ten_mod->subtree, ob, ten_mod, TSE_MODIFIER, index);
      ten->name = md->name;
      ten->directdata = md;

      if (md->type == eGpencilModifierType_Armature) {
        outliner_add_element(space_outliner,
                             &ten->subtree,
                             ((ArmatureGpencilModifierData *)md)->object,
                             ten,
                             TSE_LINKED_OB,
                             0);
      }
      else if (md->type == eGpencilModifierType_Hook) {
        outliner_add_element(space_outliner,
                             &ten->subtree,
                             ((HookGpencilModifierData *)md)->object,
                             ten,
                             TSE_LINKED_OB,
                             0);
      }
      else if (md->type == eGpencilModifierType_Lattice) {
        outliner_add_element(space_outliner,
                             &ten->subtree,
                             ((LatticeGpencilModifierData *)md)->object,
                             ten,
                             TSE_LINKED_OB,
                             0);
      }
    }
  }

  /* Grease Pencil effects. */
  if (!BLI_listbase_is_empty(&ob->shader_fx)) {
    TreeElement *ten_fx = outliner_add_element(
        space_outliner, &te->subtree, ob, te, TSE_GPENCIL_EFFECT_BASE, 0);

    ten_fx->name = IFACE_("Effects");
    int index;
    LISTBASE_FOREACH_INDEX (ShaderFxData *, fx, &ob->shader_fx, index) {
      TreeElement *ten = outliner_add_element(
          space_outliner, &ten_fx->subtree, ob, ten_fx, TSE_GPENCIL_EFFECT, index);
      ten->name = fx->name;
      ten->directdata = fx;

      if (fx->type == eShaderFxType_Swirl) {
        outliner_add_element(space_outliner,
                             &ten->subtree,
                             ((SwirlShaderFxData *)fx)->object,
                             ten,
                             TSE_LINKED_OB,
                             0);
      }
    }
  }

  /* vertex groups */
  if (ob->defbase.first) {
    bDeformGroup *defgroup;
    TreeElement *ten;
    TreeElement *tenla = outliner_add_element(
        space_outliner, &te->subtree, ob, te, TSE_DEFGROUP_BASE, 0);
    int a;

    tenla->name = IFACE_("Vertex Groups");
    for (defgroup = ob->defbase.first, a = 0; defgroup; defgroup = defgroup->next, a++) {
      ten = outliner_add_element(space_outliner, &tenla->subtree, ob, tenla, TSE_DEFGROUP, a);
      ten->name = defgroup->name;
      ten->directdata = defgroup;
    }
  }

  /* duplicated group */
  if (ob->instance_collection && (ob->transflag & OB_DUPLICOLLECTION)) {
    outliner_add_element(space_outliner, &te->subtree, ob->instance_collection, te, 0, 0);
  }
}

/* Can be inlined if necessary. */
static void outliner_add_id_contents(SpaceOutliner *space_outliner,
                                     TreeElement *te,
                                     TreeStoreElem *tselem,
                                     ID *id)
{
  /* tuck pointer back in object, to construct hierarchy */
  if (GS(id->name) == ID_OB) {
    id->newid = (ID *)te;
  }

  /* expand specific data always */
  switch (GS(id->name)) {
    case ID_LI: {
      te->name = ((Library *)id)->filepath;
      break;
    }
    case ID_SCE: {
      outliner_add_scene_contents(space_outliner, &te->subtree, (Scene *)id, te);
      break;
    }
    case ID_OB: {
      outliner_add_object_contents(space_outliner, te, tselem, (Object *)id);
      break;
    }
    case ID_ME: {
      Mesh *me = (Mesh *)id;
      int a;

      if (outliner_animdata_test(me->adt)) {
        outliner_add_element(space_outliner, &te->subtree, me, te, TSE_ANIM_DATA, 0);
      }

      outliner_add_element(space_outliner, &te->subtree, me->key, te, 0, 0);
      for (a = 0; a < me->totcol; a++) {
        outliner_add_element(space_outliner, &te->subtree, me->mat[a], te, 0, a);
      }
      /* could do tfaces with image links, but the images are not grouped nicely.
       * would require going over all tfaces, sort images in use. etc... */
      break;
    }
    case ID_CU: {
      Curve *cu = (Curve *)id;
      int a;

      if (outliner_animdata_test(cu->adt)) {
        outliner_add_element(space_outliner, &te->subtree, cu, te, TSE_ANIM_DATA, 0);
      }

      for (a = 0; a < cu->totcol; a++) {
        outliner_add_element(space_outliner, &te->subtree, cu->mat[a], te, 0, a);
      }
      break;
    }
    case ID_MB: {
      MetaBall *mb = (MetaBall *)id;
      int a;

      if (outliner_animdata_test(mb->adt)) {
        outliner_add_element(space_outliner, &te->subtree, mb, te, TSE_ANIM_DATA, 0);
      }

      for (a = 0; a < mb->totcol; a++) {
        outliner_add_element(space_outliner, &te->subtree, mb->mat[a], te, 0, a);
      }
      break;
    }
    case ID_MA: {
      Material *ma = (Material *)id;

      if (outliner_animdata_test(ma->adt)) {
        outliner_add_element(space_outliner, &te->subtree, ma, te, TSE_ANIM_DATA, 0);
      }
      break;
    }
    case ID_TE: {
      Tex *tex = (Tex *)id;

      if (outliner_animdata_test(tex->adt)) {
        outliner_add_element(space_outliner, &te->subtree, tex, te, TSE_ANIM_DATA, 0);
      }
      outliner_add_element(space_outliner, &te->subtree, tex->ima, te, 0, 0);
      break;
    }
    case ID_CA: {
      Camera *ca = (Camera *)id;

      if (outliner_animdata_test(ca->adt)) {
        outliner_add_element(space_outliner, &te->subtree, ca, te, TSE_ANIM_DATA, 0);
      }
      break;
    }
    case ID_CF: {
      CacheFile *cache_file = (CacheFile *)id;

      if (outliner_animdata_test(cache_file->adt)) {
        outliner_add_element(space_outliner, &te->subtree, cache_file, te, TSE_ANIM_DATA, 0);
      }

      break;
    }
    case ID_LA: {
      Light *la = (Light *)id;

      if (outliner_animdata_test(la->adt)) {
        outliner_add_element(space_outliner, &te->subtree, la, te, TSE_ANIM_DATA, 0);
      }
      break;
    }
    case ID_SPK: {
      Speaker *spk = (Speaker *)id;

      if (outliner_animdata_test(spk->adt)) {
        outliner_add_element(space_outliner, &te->subtree, spk, te, TSE_ANIM_DATA, 0);
      }
      break;
    }
    case ID_LP: {
      LightProbe *prb = (LightProbe *)id;

      if (outliner_animdata_test(prb->adt)) {
        outliner_add_element(space_outliner, &te->subtree, prb, te, TSE_ANIM_DATA, 0);
      }
      break;
    }
    case ID_WO: {
      World *wrld = (World *)id;

      if (outliner_animdata_test(wrld->adt)) {
        outliner_add_element(space_outliner, &te->subtree, wrld, te, TSE_ANIM_DATA, 0);
      }
      break;
    }
    case ID_KE: {
      Key *key = (Key *)id;

      if (outliner_animdata_test(key->adt)) {
        outliner_add_element(space_outliner, &te->subtree, key, te, TSE_ANIM_DATA, 0);
      }
      break;
    }
    case ID_AC: {
      /* XXX do we want to be exposing the F-Curves here? */
      /* bAction *act = (bAction *)id; */
      break;
    }
    case ID_AR: {
      bArmature *arm = (bArmature *)id;
      int a = 0;

      if (outliner_animdata_test(arm->adt)) {
        outliner_add_element(space_outliner, &te->subtree, arm, te, TSE_ANIM_DATA, 0);
      }

      if (arm->edbo) {
        EditBone *ebone;
        TreeElement *ten;

        for (ebone = arm->edbo->first; ebone; ebone = ebone->next, a++) {
          ten = outliner_add_element(space_outliner, &te->subtree, id, te, TSE_EBONE, a);
          ten->directdata = ebone;
          ten->name = ebone->name;
          ebone->temp.p = ten;
        }
        /* make hierarchy */
        ten = arm->edbo->first ? ((EditBone *)arm->edbo->first)->temp.p : NULL;
        while (ten) {
          TreeElement *nten = ten->next, *par;
          ebone = (EditBone *)ten->directdata;
          if (ebone->parent) {
            BLI_remlink(&te->subtree, ten);
            par = ebone->parent->temp.p;
            BLI_addtail(&par->subtree, ten);
            ten->parent = par;
          }
          ten = nten;
        }
      }
      else {
        /* do not extend Armature when we have posemode */
        tselem = TREESTORE(te->parent);
        if (GS(tselem->id->name) == ID_OB && ((Object *)tselem->id)->mode & OB_MODE_POSE) {
          /* pass */
        }
        else {
          Bone *curBone;
          for (curBone = arm->bonebase.first; curBone; curBone = curBone->next) {
            outliner_add_bone(space_outliner, &te->subtree, id, curBone, te, &a);
          }
        }
      }
      break;
    }
    case ID_LS: {
      FreestyleLineStyle *linestyle = (FreestyleLineStyle *)id;
      int a;

      if (outliner_animdata_test(linestyle->adt)) {
        outliner_add_element(space_outliner, &te->subtree, linestyle, te, TSE_ANIM_DATA, 0);
      }

      for (a = 0; a < MAX_MTEX; a++) {
        if (linestyle->mtex[a]) {
          outliner_add_element(space_outliner, &te->subtree, linestyle->mtex[a]->tex, te, 0, a);
        }
      }
      break;
    }
    case ID_GD: {
      bGPdata *gpd = (bGPdata *)id;
      bGPDlayer *gpl;
      int a = 0;

      if (outliner_animdata_test(gpd->adt)) {
        outliner_add_element(space_outliner, &te->subtree, gpd, te, TSE_ANIM_DATA, 0);
      }

      /* TODO: base element for layers? */
      for (gpl = gpd->layers.last; gpl; gpl = gpl->prev) {
        outliner_add_element(space_outliner, &te->subtree, gpl, te, TSE_GP_LAYER, a);
        a++;
      }
      break;
    }
    case ID_GR: {
      /* Don't expand for instances, creates too many elements. */
      if (!(te->parent && te->parent->idcode == ID_OB)) {
        Collection *collection = (Collection *)id;
        outliner_add_collection_recursive(space_outliner, collection, te);
      }
      break;
    }
    case ID_HA: {
      Hair *hair = (Hair *)id;
      if (outliner_animdata_test(hair->adt)) {
        outliner_add_element(space_outliner, &te->subtree, hair, te, TSE_ANIM_DATA, 0);
      }
      break;
    }
    case ID_PT: {
      PointCloud *pointcloud = (PointCloud *)id;
      if (outliner_animdata_test(pointcloud->adt)) {
        outliner_add_element(space_outliner, &te->subtree, pointcloud, te, TSE_ANIM_DATA, 0);
      }
      break;
    }
    case ID_VO: {
      Volume *volume = (Volume *)id;
      if (outliner_animdata_test(volume->adt)) {
        outliner_add_element(space_outliner, &te->subtree, volume, te, TSE_ANIM_DATA, 0);
      }
      break;
    }
    case ID_SIM: {
      Simulation *simulation = (Simulation *)id;
      if (outliner_animdata_test(simulation->adt)) {
        outliner_add_element(space_outliner, &te->subtree, simulation, te, TSE_ANIM_DATA, 0);
      }
      break;
    }
    default:
      break;
  }
}

/**
 * TODO: this function needs to be split up! It's getting a bit too large...
 *
 * \note: "ID" is not always a real ID
 * \note: If child items are only added to the tree if the item is open, the TSE_ type _must_ be
 *        added to #outliner_element_needs_rebuild_on_open_change().
 */
TreeElement *outliner_add_element(SpaceOutliner *space_outliner,
                                  ListBase *lb,
                                  void *idv,
                                  TreeElement *parent,
                                  short type,
                                  short index)
{
  TreeElement *te;
  TreeStoreElem *tselem;
  ID *id = idv;

  if (ELEM(type, TSE_RNA_STRUCT, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM)) {
    id = ((PointerRNA *)idv)->owner_id;
    if (!id) {
      id = ((PointerRNA *)idv)->data;
    }
  }
  else if (type == TSE_GP_LAYER) {
    /* idv is the layer its self */
    id = TREESTORE(parent)->id;
  }

  /* exceptions */
  if (type == TSE_ID_BASE) {
    /* pass */
  }
  else if (id == NULL) {
    return NULL;
  }

  if (type == 0) {
    /* Zero type means real ID, ensure we do not get non-outliner ID types here... */
    BLI_assert(TREESTORE_ID_TYPE(id));
  }

  te = MEM_callocN(sizeof(TreeElement), "tree elem");
  /* add to the visual tree */
  BLI_addtail(lb, te);
  /* add to the storage */
  check_persistent(space_outliner, te, id, type, index);
  tselem = TREESTORE(te);

  /* if we are searching for something expand to see child elements */
  if (SEARCHING_OUTLINER(space_outliner)) {
    tselem->flag |= TSE_CHILDSEARCH;
  }

  te->parent = parent;
  te->index = index; /* For data arrays. */
  if (ELEM(type, TSE_SEQUENCE, TSE_SEQ_STRIP, TSE_SEQUENCE_DUP)) {
    /* pass */
  }
  else if (ELEM(type, TSE_RNA_STRUCT, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM)) {
    /* pass */
  }
  else if (type == TSE_ANIM_DATA) {
    /* pass */
  }
  else if (type == TSE_GP_LAYER) {
    /* pass */
  }
  else if (ELEM(type, TSE_LAYER_COLLECTION, TSE_SCENE_COLLECTION_BASE, TSE_VIEW_COLLECTION_BASE)) {
    /* pass */
  }
  else if (type == TSE_ID_BASE) {
    /* pass */
  }
  else {
    /* do here too, for blend file viewer, own ID_LI then shows file name */
    if (GS(id->name) == ID_LI) {
      te->name = ((Library *)id)->filepath;
    }
    else {
      te->name = id->name + 2; /* Default, can be overridden by Library or non-ID data. */
    }
    te->idcode = GS(id->name);
  }

  if (type == 0) {
    TreeStoreElem *tsepar = parent ? TREESTORE(parent) : NULL;

    /* ID data-block. */
    if (tsepar == NULL || tsepar->type != TSE_ID_BASE || space_outliner->filter_id_type) {
      outliner_add_id_contents(space_outliner, te, tselem, id);
    }
  }
  else if (type == TSE_ANIM_DATA) {
    IdAdtTemplate *iat = (IdAdtTemplate *)idv;
    AnimData *adt = (AnimData *)iat->adt;

    /* this element's info */
    te->name = IFACE_("Animation");
    te->directdata = adt;

    /* Action */
    outliner_add_element(space_outliner, &te->subtree, adt->action, te, 0, 0);

    /* Drivers */
    if (adt->drivers.first) {
      TreeElement *ted = outliner_add_element(
          space_outliner, &te->subtree, adt, te, TSE_DRIVER_BASE, 0);
      ID *lastadded = NULL;
      FCurve *fcu;

      ted->name = IFACE_("Drivers");

      for (fcu = adt->drivers.first; fcu; fcu = fcu->next) {
        if (fcu->driver && fcu->driver->variables.first) {
          ChannelDriver *driver = fcu->driver;
          DriverVar *dvar;

          for (dvar = driver->variables.first; dvar; dvar = dvar->next) {
            /* loop over all targets used here */
            DRIVER_TARGETS_USED_LOOPER_BEGIN (dvar) {
              if (lastadded != dtar->id) {
                /* XXX this lastadded check is rather lame, and also fails quite badly... */
                outliner_add_element(
                    space_outliner, &ted->subtree, dtar->id, ted, TSE_LINKED_OB, 0);
                lastadded = dtar->id;
              }
            }
            DRIVER_TARGETS_LOOPER_END;
          }
        }
      }
    }

    /* NLA Data */
    if (adt->nla_tracks.first) {
      TreeElement *tenla = outliner_add_element(space_outliner, &te->subtree, adt, te, TSE_NLA, 0);
      NlaTrack *nlt;
      int a = 0;

      tenla->name = IFACE_("NLA Tracks");

      for (nlt = adt->nla_tracks.first; nlt; nlt = nlt->next) {
        TreeElement *tenlt = outliner_add_element(
            space_outliner, &tenla->subtree, nlt, tenla, TSE_NLA_TRACK, a);
        NlaStrip *strip;
        TreeElement *ten;
        int b = 0;

        tenlt->name = nlt->name;

        for (strip = nlt->strips.first; strip; strip = strip->next, b++) {
          ten = outliner_add_element(
              space_outliner, &tenlt->subtree, strip->act, tenlt, TSE_NLA_ACTION, b);
          if (ten) {
            ten->directdata = strip;
          }
        }
      }
    }
  }
  else if (type == TSE_GP_LAYER) {
    bGPDlayer *gpl = (bGPDlayer *)idv;

    te->name = gpl->info;
    te->directdata = gpl;
  }
  else if (type == TSE_SEQUENCE) {
    Sequence *seq = (Sequence *)idv;
    Sequence *p;

    /*
     * The idcode is a little hack, but the outliner
     * only check te->idcode if te->type is equal to zero,
     * so this is "safe".
     */
    te->idcode = seq->type;
    te->directdata = seq;
    te->name = seq->name + 2;

    if (!(seq->type & SEQ_TYPE_EFFECT)) {
      /*
       * This work like the sequence.
       * If the sequence have a name (not default name)
       * show it, in other case put the filename.
       */

      if (seq->type == SEQ_TYPE_META) {
        p = seq->seqbase.first;
        while (p) {
          outliner_add_element(space_outliner, &te->subtree, (void *)p, te, TSE_SEQUENCE, index);
          p = p->next;
        }
      }
      else {
        outliner_add_element(
            space_outliner, &te->subtree, (void *)seq->strip, te, TSE_SEQ_STRIP, index);
      }
    }
  }
  else if (type == TSE_SEQ_STRIP) {
    Strip *strip = (Strip *)idv;

    if (strip->dir[0] != '\0') {
      te->name = strip->dir;
    }
    else {
      te->name = IFACE_("Strip None");
    }
    te->directdata = strip;
  }
  else if (type == TSE_SEQUENCE_DUP) {
    Sequence *seq = (Sequence *)idv;

    te->idcode = seq->type;
    te->directdata = seq;
    te->name = seq->strip->stripdata->name;
  }
  else if (ELEM(type, TSE_RNA_STRUCT, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM)) {
    PointerRNA pptr, propptr, *ptr = (PointerRNA *)idv;
    PropertyRNA *prop, *iterprop;
    PropertyType proptype;

    /* Don't display arrays larger, weak but index is stored as a short,
     * also the outliner isn't intended for editing such large data-sets. */
    BLI_STATIC_ASSERT(sizeof(te->index) == 2, "Index is no longer short!")
    const int tot_limit = SHRT_MAX;

    int a, tot;

    /* we do lazy build, for speed and to avoid infinite recursion */

    if (ptr->data == NULL) {
      te->name = IFACE_("(empty)");
    }
    else if (type == TSE_RNA_STRUCT) {
      /* struct */
      te->name = RNA_struct_name_get_alloc(ptr, NULL, 0, NULL);

      if (te->name) {
        te->flag |= TE_FREE_NAME;
      }
      else {
        te->name = RNA_struct_ui_name(ptr->type);
      }

      /* If searching don't expand RNA entries */
      if (SEARCHING_OUTLINER(space_outliner) && BLI_strcasecmp("RNA", te->name) == 0) {
        tselem->flag &= ~TSE_CHILDSEARCH;
      }

      iterprop = RNA_struct_iterator_property(ptr->type);
      tot = RNA_property_collection_length(ptr, iterprop);
      CLAMP_MAX(tot, tot_limit);

      /* auto open these cases */
      if (!parent || (RNA_property_type(parent->directdata)) == PROP_POINTER) {
        if (!tselem->used) {
          tselem->flag &= ~TSE_CLOSED;
        }
      }

      if (TSELEM_OPEN(tselem, space_outliner)) {
        for (a = 0; a < tot; a++) {
          RNA_property_collection_lookup_int(ptr, iterprop, a, &propptr);
          if (!(RNA_property_flag(propptr.data) & PROP_HIDDEN)) {
            outliner_add_element(
                space_outliner, &te->subtree, (void *)ptr, te, TSE_RNA_PROPERTY, a);
          }
        }
      }
      else if (tot) {
        te->flag |= TE_LAZY_CLOSED;
      }

      te->rnaptr = *ptr;
    }
    else if (type == TSE_RNA_PROPERTY) {
      /* property */
      iterprop = RNA_struct_iterator_property(ptr->type);
      RNA_property_collection_lookup_int(ptr, iterprop, index, &propptr);

      prop = propptr.data;
      proptype = RNA_property_type(prop);

      te->name = RNA_property_ui_name(prop);
      te->directdata = prop;
      te->rnaptr = *ptr;

      /* If searching don't expand RNA entries */
      if (SEARCHING_OUTLINER(space_outliner) && BLI_strcasecmp("RNA", te->name) == 0) {
        tselem->flag &= ~TSE_CHILDSEARCH;
      }

      if (proptype == PROP_POINTER) {
        pptr = RNA_property_pointer_get(ptr, prop);

        if (pptr.data) {
          if (TSELEM_OPEN(tselem, space_outliner)) {
            outliner_add_element(
                space_outliner, &te->subtree, (void *)&pptr, te, TSE_RNA_STRUCT, -1);
          }
          else {
            te->flag |= TE_LAZY_CLOSED;
          }
        }
      }
      else if (proptype == PROP_COLLECTION) {
        tot = RNA_property_collection_length(ptr, prop);
        CLAMP_MAX(tot, tot_limit);

        if (TSELEM_OPEN(tselem, space_outliner)) {
          for (a = 0; a < tot; a++) {
            RNA_property_collection_lookup_int(ptr, prop, a, &pptr);
            outliner_add_element(
                space_outliner, &te->subtree, (void *)&pptr, te, TSE_RNA_STRUCT, a);
          }
        }
        else if (tot) {
          te->flag |= TE_LAZY_CLOSED;
        }
      }
      else if (ELEM(proptype, PROP_BOOLEAN, PROP_INT, PROP_FLOAT)) {
        tot = RNA_property_array_length(ptr, prop);
        CLAMP_MAX(tot, tot_limit);

        if (TSELEM_OPEN(tselem, space_outliner)) {
          for (a = 0; a < tot; a++) {
            outliner_add_element(
                space_outliner, &te->subtree, (void *)ptr, te, TSE_RNA_ARRAY_ELEM, a);
          }
        }
        else if (tot) {
          te->flag |= TE_LAZY_CLOSED;
        }
      }
    }
    else if (type == TSE_RNA_ARRAY_ELEM) {
      char c;

      prop = parent->directdata;

      te->directdata = prop;
      te->rnaptr = *ptr;
      te->index = index;

      c = RNA_property_array_item_char(prop, index);

      te->name = MEM_callocN(sizeof(char[20]), "OutlinerRNAArrayName");
      if (c) {
        sprintf((char *)te->name, "  %c", c);
      }
      else {
        sprintf((char *)te->name, "  %d", index + 1);
      }
      te->flag |= TE_FREE_NAME;
    }
  }
  else if (type == TSE_KEYMAP) {
    wmKeyMap *km = (wmKeyMap *)idv;
    wmKeyMapItem *kmi;
    char opname[OP_MAX_TYPENAME];

    te->directdata = idv;
    te->name = km->idname;

    if (TSELEM_OPEN(tselem, space_outliner)) {
      int a = 0;

      for (kmi = km->items.first; kmi; kmi = kmi->next, a++) {
        const char *key = WM_key_event_string(kmi->type, false);

        if (key[0]) {
          wmOperatorType *ot = NULL;

          if (kmi->propvalue) {
            /* pass */
          }
          else {
            ot = WM_operatortype_find(kmi->idname, 0);
          }

          if (ot || kmi->propvalue) {
            TreeElement *ten = outliner_add_element(
                space_outliner, &te->subtree, kmi, te, TSE_KEYMAP_ITEM, a);

            ten->directdata = kmi;

            if (kmi->propvalue) {
              ten->name = IFACE_("Modal map, not yet");
            }
            else {
              WM_operator_py_idname(opname, ot->idname);
              ten->name = BLI_strdup(opname);
              ten->flag |= TE_FREE_NAME;
            }
          }
        }
      }
    }
    else {
      te->flag |= TE_LAZY_CLOSED;
    }
  }

  return te;
}

/* ======================================================= */
/* Sequencer mode tree building */

/* Helped function to put duplicate sequence in the same tree. */
static int need_add_seq_dup(Sequence *seq)
{
  Sequence *p;

  if ((!seq->strip) || (!seq->strip->stripdata)) {
    return 1;
  }

  /*
   * First check backward, if we found a duplicate
   * sequence before this, don't need it, just return.
   */
  p = seq->prev;
  while (p) {
    if ((!p->strip) || (!p->strip->stripdata)) {
      p = p->prev;
      continue;
    }

    if (STREQ(p->strip->stripdata->name, seq->strip->stripdata->name)) {
      return 2;
    }
    p = p->prev;
  }

  p = seq->next;
  while (p) {
    if ((!p->strip) || (!p->strip->stripdata)) {
      p = p->next;
      continue;
    }

    if (STREQ(p->strip->stripdata->name, seq->strip->stripdata->name)) {
      return 0;
    }
    p = p->next;
  }
  return 1;
}

static void outliner_add_seq_dup(SpaceOutliner *space_outliner,
                                 Sequence *seq,
                                 TreeElement *te,
                                 short index)
{
  /* TreeElement *ch; */ /* UNUSED */
  Sequence *p;

  p = seq;
  while (p) {
    if ((!p->strip) || (!p->strip->stripdata) || (p->strip->stripdata->name[0] == '\0')) {
      p = p->next;
      continue;
    }

    if (STREQ(p->strip->stripdata->name, seq->strip->stripdata->name)) {
      /* ch = */ /* UNUSED */ outliner_add_element(
          space_outliner, &te->subtree, (void *)p, te, TSE_SEQUENCE, index);
    }
    p = p->next;
  }
}

/* ----------------------------------------------- */

static void outliner_add_orphaned_datablocks(Main *mainvar, SpaceOutliner *space_outliner)
{
  TreeElement *ten;
  ListBase *lbarray[MAX_LIBARRAY];
  int a, tot;
  short filter_id_type = (space_outliner->filter & SO_FILTER_ID_TYPE) ?
                             space_outliner->filter_id_type :
                             0;

  if (filter_id_type) {
    lbarray[0] = which_libbase(mainvar, space_outliner->filter_id_type);
    tot = 1;
  }
  else {
    tot = set_listbasepointers(mainvar, lbarray);
  }

  for (a = 0; a < tot; a++) {
    if (lbarray[a] && lbarray[a]->first) {
      ID *id = lbarray[a]->first;

      /* check if there are any data-blocks of this type which are orphans */
      for (; id; id = id->next) {
        if (ID_REAL_USERS(id) <= 0) {
          break;
        }
      }

      if (id) {
        /* header for this type of data-block */
        if (filter_id_type) {
          ten = NULL;
        }
        else {
          ten = outliner_add_element(
              space_outliner, &space_outliner->tree, lbarray[a], NULL, TSE_ID_BASE, 0);
          ten->directdata = lbarray[a];
          ten->name = outliner_idcode_to_plural(GS(id->name));
        }

        /* add the orphaned data-blocks - these will not be added with any subtrees attached */
        for (id = lbarray[a]->first; id; id = id->next) {
          if (ID_REAL_USERS(id) <= 0) {
            outliner_add_element(
                space_outliner, (ten) ? &ten->subtree : &space_outliner->tree, id, ten, 0, 0);
          }
        }
      }
    }
  }
}

BLI_INLINE void outliner_add_collection_init(TreeElement *te, Collection *collection)
{
  te->name = BKE_collection_ui_name_get(collection);
  te->directdata = collection;
}

BLI_INLINE void outliner_add_collection_objects(SpaceOutliner *space_outliner,
                                                ListBase *tree,
                                                Collection *collection,
                                                TreeElement *parent)
{
  LISTBASE_FOREACH (CollectionObject *, cob, &collection->gobject) {
    outliner_add_element(space_outliner, tree, cob->ob, parent, 0, 0);
  }
}

static TreeElement *outliner_add_collection_recursive(SpaceOutliner *space_outliner,
                                                      Collection *collection,
                                                      TreeElement *ten)
{
  outliner_add_collection_init(ten, collection);

  LISTBASE_FOREACH (CollectionChild *, child, &collection->children) {
    outliner_add_element(space_outliner, &ten->subtree, &child->collection->id, ten, 0, 0);
  }

  if (space_outliner->outlinevis != SO_SCENES) {
    outliner_add_collection_objects(space_outliner, &ten->subtree, collection, ten);
  }

  return ten;
}

/* ======================================================= */
/* Generic Tree Building helpers - order these are called is top to bottom */

/* Hierarchy --------------------------------------------- */

/* make sure elements are correctly nested */
void outliner_make_object_parent_hierarchy(ListBase *lb)
{
  TreeElement *te, *ten, *tep;
  TreeStoreElem *tselem;

  /* build hierarchy */
  /* XXX also, set extents here... */
  te = lb->first;
  while (te) {
    ten = te->next;
    tselem = TREESTORE(te);

    if (tselem->type == 0 && te->idcode == ID_OB) {
      Object *ob = (Object *)tselem->id;
      if (ob->parent && ob->parent->id.newid) {
        BLI_remlink(lb, te);
        tep = (TreeElement *)ob->parent->id.newid;
        BLI_addtail(&tep->subtree, te);
        te->parent = tep;
      }
    }
    te = ten;
  }
}

/* Sorting ------------------------------------------------------ */

typedef struct tTreeSort {
  TreeElement *te;
  ID *id;
  const char *name;
  short idcode;
} tTreeSort;

/* alphabetical comparator, trying to put objects first */
static int treesort_alpha_ob(const void *v1, const void *v2)
{
  const tTreeSort *x1 = v1, *x2 = v2;
  int comp;

  /* first put objects last (hierarchy) */
  comp = (x1->idcode == ID_OB);
  if (x2->idcode == ID_OB) {
    comp += 2;
  }

  if (comp == 1) {
    return 1;
  }
  if (comp == 2) {
    return -1;
  }
  if (comp == 3) {
    /* Among objects first come the ones in the collection, followed by the ones not on it.
     * This way we can have the dashed lines in a separate style connecting the former. */
    if ((x1->te->flag & TE_CHILD_NOT_IN_COLLECTION) !=
        (x2->te->flag & TE_CHILD_NOT_IN_COLLECTION)) {
      return (x1->te->flag & TE_CHILD_NOT_IN_COLLECTION) ? 1 : -1;
    }

    comp = BLI_strcasecmp_natural(x1->name, x2->name);

    if (comp > 0) {
      return 1;
    }
    if (comp < 0) {
      return -1;
    }
    return 0;
  }
  return 0;
}

/* Move children that are not in the collection to the end of the list. */
static int treesort_child_not_in_collection(const void *v1, const void *v2)
{
  const tTreeSort *x1 = v1, *x2 = v2;

  /* Among objects first come the ones in the collection, followed by the ones not on it.
   * This way we can have the dashed lines in a separate style connecting the former. */
  if ((x1->te->flag & TE_CHILD_NOT_IN_COLLECTION) != (x2->te->flag & TE_CHILD_NOT_IN_COLLECTION)) {
    return (x1->te->flag & TE_CHILD_NOT_IN_COLLECTION) ? 1 : -1;
  }
  return 0;
}

/* alphabetical comparator */
static int treesort_alpha(const void *v1, const void *v2)
{
  const tTreeSort *x1 = v1, *x2 = v2;
  int comp;

  comp = BLI_strcasecmp_natural(x1->name, x2->name);

  if (comp > 0) {
    return 1;
  }
  if (comp < 0) {
    return -1;
  }
  return 0;
}

/* this is nice option for later? doesn't look too useful... */
#if 0
static int treesort_obtype_alpha(const void *v1, const void *v2)
{
  const tTreeSort *x1 = v1, *x2 = v2;

  /* first put objects last (hierarchy) */
  if (x1->idcode == ID_OB && x2->idcode != ID_OB) {
    return 1;
  }
  else if (x2->idcode == ID_OB && x1->idcode != ID_OB) {
    return -1;
  }
  else {
    /* 2nd we check ob type */
    if (x1->idcode == ID_OB && x2->idcode == ID_OB) {
      if (((Object *)x1->id)->type > ((Object *)x2->id)->type) {
        return 1;
      }
      else if (((Object *)x1->id)->type > ((Object *)x2->id)->type) {
        return -1;
      }
      else {
        return 0;
      }
    }
    else {
      int comp = BLI_strcasecmp_natural(x1->name, x2->name);

      if (comp > 0) {
        return 1;
      }
      else if (comp < 0) {
        return -1;
      }
      return 0;
    }
  }
}
#endif

/* sort happens on each subtree individual */
static void outliner_sort(ListBase *lb)
{
  TreeElement *te;
  TreeStoreElem *tselem;

  te = lb->last;
  if (te == NULL) {
    return;
  }
  tselem = TREESTORE(te);

  /* sorting rules; only object lists, ID lists, or deformgroups */
  if (ELEM(tselem->type, TSE_DEFGROUP, TSE_ID_BASE) ||
      (tselem->type == 0 && te->idcode == ID_OB)) {
    int totelem = BLI_listbase_count(lb);

    if (totelem > 1) {
      tTreeSort *tear = MEM_mallocN(totelem * sizeof(tTreeSort), "tree sort array");
      tTreeSort *tp = tear;
      int skip = 0;

      for (te = lb->first; te; te = te->next, tp++) {
        tselem = TREESTORE(te);
        tp->te = te;
        tp->name = te->name;
        tp->idcode = te->idcode;

        if (tselem->type && tselem->type != TSE_DEFGROUP) {
          tp->idcode = 0; /* Don't sort this. */
        }
        if (tselem->type == TSE_ID_BASE) {
          tp->idcode = 1; /* Do sort this. */
        }

        tp->id = tselem->id;
      }

      /* just sort alphabetically */
      if (tear->idcode == 1) {
        qsort(tear, totelem, sizeof(tTreeSort), treesort_alpha);
      }
      else {
        /* keep beginning of list */
        for (tp = tear, skip = 0; skip < totelem; skip++, tp++) {
          if (tp->idcode) {
            break;
          }
        }

        if (skip < totelem) {
          qsort(tear + skip, totelem - skip, sizeof(tTreeSort), treesort_alpha_ob);
        }
      }

      BLI_listbase_clear(lb);
      tp = tear;
      while (totelem--) {
        BLI_addtail(lb, tp->te);
        tp++;
      }
      MEM_freeN(tear);
    }
  }

  LISTBASE_FOREACH (TreeElement *, te_iter, lb) {
    outliner_sort(&te_iter->subtree);
  }
}

static void outliner_collections_children_sort(ListBase *lb)
{
  TreeElement *te;
  TreeStoreElem *tselem;

  te = lb->last;
  if (te == NULL) {
    return;
  }
  tselem = TREESTORE(te);

  /* Sorting rules: only object lists. */
  if (tselem->type == 0 && te->idcode == ID_OB) {
    int totelem = BLI_listbase_count(lb);

    if (totelem > 1) {
      tTreeSort *tear = MEM_mallocN(totelem * sizeof(tTreeSort), "tree sort array");
      tTreeSort *tp = tear;

      for (te = lb->first; te; te = te->next, tp++) {
        tselem = TREESTORE(te);
        tp->te = te;
        tp->name = te->name;
        tp->idcode = te->idcode;
        tp->id = tselem->id;
      }

      qsort(tear, totelem, sizeof(tTreeSort), treesort_child_not_in_collection);

      BLI_listbase_clear(lb);
      tp = tear;
      while (totelem--) {
        BLI_addtail(lb, tp->te);
        tp++;
      }
      MEM_freeN(tear);
    }
  }

  LISTBASE_FOREACH (TreeElement *, te_iter, lb) {
    outliner_collections_children_sort(&te_iter->subtree);
  }
}

/* Filtering ----------------------------------------------- */

typedef struct OutlinerTreeElementFocus {
  TreeStoreElem *tselem;
  int ys;
} OutlinerTreeElementFocus;

/**
 * Bring the outliner scrolling back to where it was in relation to the original focus element
 * Caller is expected to handle redrawing of ARegion.
 */
static void outliner_restore_scrolling_position(SpaceOutliner *space_outliner,
                                                ARegion *region,
                                                OutlinerTreeElementFocus *focus)
{
  View2D *v2d = &region->v2d;

  if (focus->tselem != NULL) {
    outliner_set_coordinates(region, space_outliner);

    TreeElement *te_new = outliner_find_tree_element(&space_outliner->tree, focus->tselem);

    if (te_new != NULL) {
      int ys_new = te_new->ys;
      int ys_old = focus->ys;

      float y_move = MIN2(ys_new - ys_old, -v2d->cur.ymax);
      BLI_rctf_translate(&v2d->cur, 0, y_move);
    }
    else {
      return;
    }
  }
}

static bool test_collection_callback(TreeElement *te)
{
  return outliner_is_collection_tree_element(te);
}

static bool test_object_callback(TreeElement *te)
{
  TreeStoreElem *tselem = TREESTORE(te);
  return ((tselem->type == 0) && (te->idcode == ID_OB));
}

/**
 * See if TreeElement or any of its children pass the callback_test.
 */
static TreeElement *outliner_find_first_desired_element_at_y_recursive(
    const SpaceOutliner *space_outliner,
    TreeElement *te,
    const float limit,
    bool (*callback_test)(TreeElement *))
{
  if (callback_test(te)) {
    return te;
  }

  if (TSELEM_OPEN(te->store_elem, space_outliner)) {
    TreeElement *te_iter, *te_sub;
    for (te_iter = te->subtree.first; te_iter; te_iter = te_iter->next) {
      te_sub = outliner_find_first_desired_element_at_y_recursive(
          space_outliner, te_iter, limit, callback_test);
      if (te_sub != NULL) {
        return te_sub;
      }
    }
  }

  return NULL;
}

/**
 * Find the first element that passes a test starting from a reference vertical coordinate
 *
 * If the element that is in the position is not what we are looking for, keep looking for its
 * children, siblings, and eventually, aunts, cousins, distant families, ... etc.
 *
 * Basically we keep going up and down the outliner tree from that point forward, until we find
 * what we are looking for. If we are past the visible range and we can't find a valid element
 * we return NULL.
 */
static TreeElement *outliner_find_first_desired_element_at_y(const SpaceOutliner *space_outliner,
                                                             const float view_co,
                                                             const float view_co_limit)
{
  TreeElement *te, *te_sub;
  te = outliner_find_item_at_y(space_outliner, &space_outliner->tree, view_co);

  bool (*callback_test)(TreeElement *);
  if ((space_outliner->outlinevis == SO_VIEW_LAYER) &&
      (space_outliner->filter & SO_FILTER_NO_COLLECTION)) {
    callback_test = test_object_callback;
  }
  else {
    callback_test = test_collection_callback;
  }

  while (te != NULL) {
    te_sub = outliner_find_first_desired_element_at_y_recursive(
        space_outliner, te, view_co_limit, callback_test);
    if (te_sub != NULL) {
      /* Skip the element if it was not visible to start with. */
      if (te->ys + UI_UNIT_Y > view_co_limit) {
        return te_sub;
      }
      return NULL;
    }

    if (te->next) {
      te = te->next;
      continue;
    }

    if (te->parent == NULL) {
      break;
    }

    while (te->parent) {
      if (te->parent->next) {
        te = te->parent->next;
        break;
      }
      te = te->parent;
    }
  }

  return NULL;
}

/**
 * Store information of current outliner scrolling status to be restored later.
 *
 * Finds the top-most collection visible in the outliner and populates the
 * #OutlinerTreeElementFocus struct to retrieve this element later to make sure it is in the same
 * original position as before filtering.
 */
static void outliner_store_scrolling_position(SpaceOutliner *space_outliner,
                                              ARegion *region,
                                              OutlinerTreeElementFocus *focus)
{
  TreeElement *te;
  float limit = region->v2d.cur.ymin;

  outliner_set_coordinates(region, space_outliner);

  te = outliner_find_first_desired_element_at_y(space_outliner, region->v2d.cur.ymax, limit);

  if (te != NULL) {
    focus->tselem = TREESTORE(te);
    focus->ys = te->ys;
  }
  else {
    focus->tselem = NULL;
  }
}

static int outliner_exclude_filter_get(const SpaceOutliner *space_outliner)
{
  int exclude_filter = space_outliner->filter & ~SO_FILTER_OB_STATE;

  if (space_outliner->search_string[0] != 0) {
    exclude_filter |= SO_FILTER_SEARCH;
  }
  else {
    exclude_filter &= ~SO_FILTER_SEARCH;
  }

  /* Let's have this for the collection options at first. */
  if (!SUPPORT_FILTER_OUTLINER(space_outliner)) {
    return (exclude_filter & SO_FILTER_SEARCH);
  }

  if (space_outliner->filter & SO_FILTER_NO_OBJECT) {
    exclude_filter |= SO_FILTER_OB_TYPE;
  }

  switch (space_outliner->filter_state) {
    case SO_FILTER_OB_VISIBLE:
      exclude_filter |= SO_FILTER_OB_STATE_VISIBLE;
      break;
    case SO_FILTER_OB_SELECTED:
      exclude_filter |= SO_FILTER_OB_STATE_SELECTED;
      break;
    case SO_FILTER_OB_ACTIVE:
      exclude_filter |= SO_FILTER_OB_STATE_ACTIVE;
      break;
    case SO_FILTER_OB_SELECTABLE:
      exclude_filter |= SO_FILTER_OB_STATE_SELECTABLE;
      break;
  }

  return exclude_filter;
}

static bool outliner_element_visible_get(ViewLayer *view_layer,
                                         TreeElement *te,
                                         const int exclude_filter)
{
  if ((exclude_filter & SO_FILTER_ANY) == 0) {
    return true;
  }

  TreeStoreElem *tselem = TREESTORE(te);
  if ((tselem->type == 0) && (te->idcode == ID_OB)) {
    if ((exclude_filter & SO_FILTER_OB_TYPE) == SO_FILTER_OB_TYPE) {
      return false;
    }

    Object *ob = (Object *)tselem->id;
    Base *base = (Base *)te->directdata;
    BLI_assert((base == NULL) || (base->object == ob));

    if (exclude_filter & SO_FILTER_OB_TYPE) {
      switch (ob->type) {
        case OB_MESH:
          if (exclude_filter & SO_FILTER_NO_OB_MESH) {
            return false;
          }
          break;
        case OB_ARMATURE:
          if (exclude_filter & SO_FILTER_NO_OB_ARMATURE) {
            return false;
          }
          break;
        case OB_EMPTY:
          if (exclude_filter & SO_FILTER_NO_OB_EMPTY) {
            return false;
          }
          break;
        case OB_LAMP:
          if (exclude_filter & SO_FILTER_NO_OB_LAMP) {
            return false;
          }
          break;
        case OB_CAMERA:
          if (exclude_filter & SO_FILTER_NO_OB_CAMERA) {
            return false;
          }
          break;
        default:
          if (exclude_filter & SO_FILTER_NO_OB_OTHERS) {
            return false;
          }
          break;
      }
    }

    if (exclude_filter & SO_FILTER_OB_STATE) {
      if (base == NULL) {
        base = BKE_view_layer_base_find(view_layer, ob);

        if (base == NULL) {
          return false;
        }
      }

      bool is_visible = true;
      if (exclude_filter & SO_FILTER_OB_STATE_VISIBLE) {
        if ((base->flag & BASE_VISIBLE_VIEWLAYER) == 0) {
          is_visible = false;
        }
      }
      else if (exclude_filter & SO_FILTER_OB_STATE_SELECTED) {
        if ((base->flag & BASE_SELECTED) == 0) {
          is_visible = false;
        }
      }
      else if (exclude_filter & SO_FILTER_OB_STATE_SELECTABLE) {
        if ((base->flag & BASE_SELECTABLE) == 0) {
          is_visible = false;
        }
      }
      else {
        BLI_assert(exclude_filter & SO_FILTER_OB_STATE_ACTIVE);
        if (base != BASACT(view_layer)) {
          is_visible = false;
        }
      }

      if (exclude_filter & SO_FILTER_OB_STATE_INVERSE) {
        is_visible = !is_visible;
      }

      return is_visible;
    }

    if ((te->parent != NULL) && (TREESTORE(te->parent)->type == 0) &&
        (te->parent->idcode == ID_OB)) {
      if (exclude_filter & SO_FILTER_NO_CHILDREN) {
        return false;
      }
    }
  }
  else if (te->parent != NULL && TREESTORE(te->parent)->type == 0 && te->parent->idcode == ID_OB) {
    if (exclude_filter & SO_FILTER_NO_OB_CONTENT) {
      return false;
    }
  }

  return true;
}

static bool outliner_filter_has_name(TreeElement *te, const char *name, int flags)
{
  int fn_flag = 0;

  if ((flags & SO_FIND_CASE_SENSITIVE) == 0) {
    fn_flag |= FNM_CASEFOLD;
  }

  return fnmatch(name, te->name, fn_flag) == 0;
}

static bool outliner_element_is_collection_or_object(TreeElement *te)
{
  TreeStoreElem *tselem = TREESTORE(te);

  if ((tselem->type == 0) && (te->idcode == ID_OB)) {
    return true;
  }

  /* Collection instance datablocks should not be extracted. */
  if (outliner_is_collection_tree_element(te) && !(te->parent && te->parent->idcode == ID_OB)) {
    return true;
  }

  return false;
}

static TreeElement *outliner_extract_children_from_subtree(TreeElement *element,
                                                           ListBase *parent_subtree)
{
  TreeElement *te_next = element->next;

  if (outliner_element_is_collection_or_object(element)) {
    TreeElement *te_prev = NULL;
    for (TreeElement *te = element->subtree.last; te; te = te_prev) {
      te_prev = te->prev;

      if (!outliner_element_is_collection_or_object(te)) {
        continue;
      }

      te_next = te;
      BLI_remlink(&element->subtree, te);
      BLI_insertlinkafter(parent_subtree, element->prev, te);
      te->parent = element->parent;
    }
  }

  outliner_free_tree_element(element, parent_subtree);
  return te_next;
}

static int outliner_filter_subtree(SpaceOutliner *space_outliner,
                                   ViewLayer *view_layer,
                                   ListBase *lb,
                                   const char *search_string,
                                   const int exclude_filter)
{
  TreeElement *te, *te_next;
  TreeStoreElem *tselem;

  for (te = lb->first; te; te = te_next) {
    te_next = te->next;
    if ((outliner_element_visible_get(view_layer, te, exclude_filter) == false)) {
      /* Don't free the tree, but extract the children from the parent and add to this tree. */
      /* This also needs filtering the subtree prior (see T69246). */
      outliner_filter_subtree(
          space_outliner, view_layer, &te->subtree, search_string, exclude_filter);
      te_next = outliner_extract_children_from_subtree(te, lb);
      continue;
    }
    if ((exclude_filter & SO_FILTER_SEARCH) == 0) {
      /* Filter subtree too. */
      outliner_filter_subtree(
          space_outliner, view_layer, &te->subtree, search_string, exclude_filter);
      continue;
    }

    if (!outliner_filter_has_name(te, search_string, space_outliner->search_flags)) {
      /* item isn't something we're looking for, but...
       * - if the subtree is expanded, check if there are any matches that can be easily found
       *     so that searching for "cu" in the default scene will still match the Cube
       * - otherwise, we can't see within the subtree and the item doesn't match,
       *     so these can be safely ignored (i.e. the subtree can get freed)
       */
      tselem = TREESTORE(te);

      /* flag as not a found item */
      tselem->flag &= ~TSE_SEARCHMATCH;

      if ((!TSELEM_OPEN(tselem, space_outliner)) ||
          outliner_filter_subtree(
              space_outliner, view_layer, &te->subtree, search_string, exclude_filter) == 0) {
        outliner_free_tree_element(te, lb);
      }
    }
    else {
      tselem = TREESTORE(te);

      /* flag as a found item - we can then highlight it */
      tselem->flag |= TSE_SEARCHMATCH;

      /* filter subtree too */
      outliner_filter_subtree(
          space_outliner, view_layer, &te->subtree, search_string, exclude_filter);
    }
  }

  /* if there are still items in the list, that means that there were still some matches */
  return (BLI_listbase_is_empty(lb) == false);
}

static void outliner_filter_tree(SpaceOutliner *space_outliner, ViewLayer *view_layer)
{
  char search_buff[sizeof(((struct SpaceOutliner *)NULL)->search_string) + 2];
  char *search_string;

  const int exclude_filter = outliner_exclude_filter_get(space_outliner);

  if (exclude_filter == 0) {
    return;
  }

  if (space_outliner->search_flags & SO_FIND_COMPLETE) {
    search_string = space_outliner->search_string;
  }
  else {
    /* Implicitly add heading/trailing wildcards if needed. */
    BLI_strncpy_ensure_pad(search_buff, space_outliner->search_string, '*', sizeof(search_buff));
    search_string = search_buff;
  }

  outliner_filter_subtree(
      space_outliner, view_layer, &space_outliner->tree, search_string, exclude_filter);
}

/* ======================================================= */
/* Main Tree Building API */

/* Main entry point for building the tree data-structure that the outliner represents. */
/* TODO: split each mode into its own function? */
void outliner_build_tree(Main *mainvar,
                         Scene *scene,
                         ViewLayer *view_layer,
                         SpaceOutliner *space_outliner,
                         ARegion *region)
{
  TreeElement *te = NULL, *ten;
  TreeStoreElem *tselem;
  /* on first view, we open scenes */
  int show_opened = !space_outliner->treestore || !BLI_mempool_len(space_outliner->treestore);

  /* Are we looking for something - we want to tag parents to filter child matches
   * - NOT in data-blocks view - searching all data-blocks takes way too long to be useful
   * - this variable is only set once per tree build */
  if (space_outliner->search_string[0] != 0 && space_outliner->outlinevis != SO_DATA_API) {
    space_outliner->search_flags |= SO_SEARCH_RECURSIVE;
  }
  else {
    space_outliner->search_flags &= ~SO_SEARCH_RECURSIVE;
  }

  if (space_outliner->treehash && (space_outliner->storeflag & SO_TREESTORE_REBUILD) &&
      space_outliner->treestore) {
    space_outliner->storeflag &= ~SO_TREESTORE_REBUILD;
    BKE_outliner_treehash_rebuild_from_treestore(space_outliner->treehash,
                                                 space_outliner->treestore);
  }

  if (region->do_draw & RGN_DRAW_NO_REBUILD) {
    return;
  }

  OutlinerTreeElementFocus focus;
  outliner_store_scrolling_position(space_outliner, region, &focus);

  outliner_free_tree(&space_outliner->tree);
  outliner_storage_cleanup(space_outliner);
  outliner_tree_display_destroy(&space_outliner->runtime->tree_display);

  space_outliner->runtime->tree_display = outliner_tree_display_create(space_outliner->outlinevis,
                                                                       space_outliner);
  if (space_outliner->runtime->tree_display) {
    TreeSourceData source_data = {.bmain = mainvar, .scene = scene, .view_layer = view_layer};
    space_outliner->tree = outliner_tree_display_build_tree(space_outliner->runtime->tree_display,
                                                            &source_data);
  }

  if (space_outliner->runtime->tree_display) {
    /* Skip if there's a tree-display that's responsible for adding all elements. */
  }
  /* options */
  else if (space_outliner->outlinevis == SO_LIBRARIES) {
    /* Ported to new tree-display, should be built there already. */
    BLI_assert(false);
  }
  else if (space_outliner->outlinevis == SO_SCENES) {
    Scene *sce;
    for (sce = mainvar->scenes.first; sce; sce = sce->id.next) {
      te = outliner_add_element(space_outliner, &space_outliner->tree, sce, NULL, 0, 0);
      tselem = TREESTORE(te);

      /* New scene elements open by default */
      if ((sce == scene && show_opened) || !tselem->used) {
        tselem->flag &= ~TSE_CLOSED;
      }

      outliner_make_object_parent_hierarchy(&te->subtree);
    }
  }
  else if (space_outliner->outlinevis == SO_SEQUENCE) {
    Sequence *seq;
    Editing *ed = BKE_sequencer_editing_get(scene, false);
    int op;

    if (ed == NULL) {
      return;
    }

    seq = ed->seqbasep->first;
    if (!seq) {
      return;
    }

    while (seq) {
      op = need_add_seq_dup(seq);
      if (op == 1) {
        /* ten = */ outliner_add_element(
            space_outliner, &space_outliner->tree, (void *)seq, NULL, TSE_SEQUENCE, 0);
      }
      else if (op == 0) {
        ten = outliner_add_element(
            space_outliner, &space_outliner->tree, (void *)seq, NULL, TSE_SEQUENCE_DUP, 0);
        outliner_add_seq_dup(space_outliner, seq, ten, 0);
      }
      seq = seq->next;
    }
  }
  else if (space_outliner->outlinevis == SO_DATA_API) {
    PointerRNA mainptr;

    RNA_main_pointer_create(mainvar, &mainptr);

    ten = outliner_add_element(
        space_outliner, &space_outliner->tree, (void *)&mainptr, NULL, TSE_RNA_STRUCT, -1);

    if (show_opened) {
      tselem = TREESTORE(ten);
      tselem->flag &= ~TSE_CLOSED;
    }
  }
  else if (space_outliner->outlinevis == SO_ID_ORPHANS) {
    outliner_add_orphaned_datablocks(mainvar, space_outliner);
  }
  else if (space_outliner->outlinevis == SO_VIEW_LAYER) {
    /* Ported to new tree-display, should be built there already. */
    BLI_assert(false);
  }

  if ((space_outliner->flag & SO_SKIP_SORT_ALPHA) == 0) {
    outliner_sort(&space_outliner->tree);
  }
  else if ((space_outliner->filter & SO_FILTER_NO_CHILDREN) == 0) {
    /* We group the children that are in the collection before the ones that are not.
     * This way we can try to draw them in a different style altogether.
     * We also have to respect the original order of the elements in case alphabetical
     * sorting is not enabled. This keep object data and modifiers before its children. */
    outliner_collections_children_sort(&space_outliner->tree);
  }

  outliner_filter_tree(space_outliner, view_layer);
  outliner_restore_scrolling_position(space_outliner, region, &focus);

  BKE_main_id_clear_newpoins(mainvar);
}
