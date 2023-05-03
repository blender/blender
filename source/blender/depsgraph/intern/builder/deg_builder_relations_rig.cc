/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2013 Blender Foundation */

/** \file
 * \ingroup depsgraph
 *
 * Methods for constructing depsgraph
 */

#include "intern/builder/deg_builder_relations.h"

#include <cstdio>
#include <cstdlib>
#include <cstring> /* required for STREQ later on. */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_utildefines.h"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_customdata_types.h"
#include "DNA_object_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"

#include "RNA_prototypes.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "intern/builder/deg_builder.h"
#include "intern/builder/deg_builder_cache.h"
#include "intern/builder/deg_builder_pchanmap.h"
#include "intern/debug/deg_debug.h"
#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_operation.h"

#include "intern/depsgraph_relation.h"
#include "intern/depsgraph_type.h"

namespace blender::deg {

/* IK Solver Eval Steps */
void DepsgraphRelationBuilder::build_ik_pose(Object *object,
                                             bPoseChannel *pchan,
                                             bConstraint *con,
                                             RootPChanMap *root_map,
                                             GHash *solverchan_from_chain_rootchan)
{
  if ((con->flag & CONSTRAINT_DISABLE) != 0) {
    /* Do not add disabled IK constraints to the relations. If these needs to be temporarily
     * enabled, they will be added as temporary constraints during transform. */
    return;
  }

  bKinematicConstraint *data = (bKinematicConstraint *)con->data;
  if (data->flag & CONSTRAINT_IK_DO_NOT_CREATE_POSETREE) {
    return;
  }

  /* Attach owner to IK Solver to. */
  bPoseChannel *chain_rootchan = BKE_armature_ik_solver_find_root(pchan, data);
  if (chain_rootchan == nullptr) {
    return;
  }

  bPoseChannel *posetree_rootchan = static_cast<bPoseChannel *>(
      BLI_ghash_lookup(solverchan_from_chain_rootchan, chain_rootchan));
  BLI_assert(posetree_rootchan);

  OperationKey pchan_local_key(
      &object->id, NodeType::BONE, pchan->name, OperationCode::BONE_LOCAL);
  OperationKey init_ik_key(&object->id, NodeType::EVAL_POSE, OperationCode::POSE_INIT_IK);
  OperationKey solver_key(
      &object->id, NodeType::EVAL_POSE, posetree_rootchan->name, OperationCode::POSE_IK_SOLVER);
  OperationKey pose_cleanup_key(&object->id, NodeType::EVAL_POSE, OperationCode::POSE_CLEANUP);
  /* If any of the constraint parameters are animated, connect the relation. Since there is only
   * one Init IK node per armature, this link has quite high risk of spurious dependency cycles.
   */
  const bool is_itasc = (object->pose->iksolver == IKSOLVER_ITASC);
  PointerRNA con_ptr;
  RNA_pointer_create(&object->id, &RNA_Constraint, con, &con_ptr);
  if (is_itasc || cache_->isAnyPropertyAnimated(&con_ptr)) {
    add_relation(pchan_local_key, init_ik_key, "IK Constraint -> Init IK Tree");
  }
  add_relation(init_ik_key, solver_key, "Init IK -> IK Solver");
  /* Never cleanup before solver is run. */
  add_relation(solver_key, pose_cleanup_key, "IK Solver -> Cleanup", RELATION_FLAG_GODMODE);
  /* The ITASC solver currently accesses the target transforms in init tree :(
   * TODO: Fix ITASC and remove this.
   */
  OperationKey target_dependent_key = is_itasc ? init_ik_key : solver_key;
  /* IK target */
  bPoseChannel *targetchan = nullptr;
  /* TODO(sergey): This should get handled as part of the constraint code. */
  if (data->tar != nullptr) {
    /* Different object - requires its transform. */
    if (data->tar != object) {
      ComponentKey target_key(&data->tar->id, NodeType::TRANSFORM);
      add_relation(target_key, target_dependent_key, con->name);
      /* Ensure target CoW is ready by the time IK tree is built just in case. */
      ComponentKey target_cow_key(&data->tar->id, NodeType::COPY_ON_WRITE);
      add_relation(
          target_cow_key, init_ik_key, "IK Target CoW -> Init IK Tree", RELATION_CHECK_BEFORE_ADD);
    }
    /* Subtarget references: */
    if ((data->tar->type == OB_ARMATURE) && (data->subtarget[0])) {
      /* Bone - use the final transformation. */
      targetchan = BKE_pose_channel_find_name(data->tar->pose, data->subtarget);

      const bool is_twoway = (data->flag & CONSTRAINT_IK_IS_TWOWAY) != 0;
      /* Target will have root whenever the objects are the same, so we need to further check if
       * the target is affected by the ik solver. */
      if (root_map->has_root(data->subtarget, posetree_rootchan->name) && is_twoway) {
        OperationKey target_key(
            &data->tar->id, NodeType::BONE, data->subtarget, OperationCode::BONE_READY);
        add_relation(target_key, target_dependent_key, con->name);
      }
      else {
        OperationKey target_key(
            &data->tar->id, NodeType::BONE, data->subtarget, OperationCode::BONE_DONE);
        add_relation(target_key, target_dependent_key, con->name);
      }

      if (is_twoway) {
        /** GG: CLEANUP: These 2 lines are redundant? maybe it differs if using itasc?*/
        bPoseChannel *parchan = targetchan;
        int segcount_target = 0;
        while (parchan != nullptr) {
          OperationKey parent_key(
              &object->id, NodeType::BONE, parchan->name, OperationCode::BONE_READY);
          add_relation(parent_key, solver_key, "IK Chain Parent");
          OperationKey bone_done_key(
              &object->id, NodeType::BONE, parchan->name, OperationCode::BONE_DONE);
          add_relation(solver_key, bone_done_key, "IK Chain Result");
          parchan->flag |= POSE_DONE;

          /* continue up chain, until we reach target number of items. */
          DEG_DEBUG_PRINTF(
              (::Depsgraph *)graph_, BUILD, "  %d = %s\n", segcount_target, parchan->name);
          /* TODO(sergey): This is an arbitrary value, which was just following
           * old code convention. */
          segcount_target++;
          if ((segcount_target == data->rootbone_target) || (segcount_target > 255)) {
            BLI_assert(segcount_target <= 255);
            break;
          }
          parchan = parchan->parent;
        }
      }
    }
    else if (data->subtarget[0] && ELEM(data->tar->type, OB_MESH, OB_LATTICE)) {
      /* Vertex group target. */
      /* NOTE: for now, we don't need to represent vertex groups
       * separately. */
      ComponentKey target_key(&data->tar->id, NodeType::GEOMETRY);
      add_relation(target_key, target_dependent_key, con->name);
      add_customdata_mask(data->tar, DEGCustomDataMeshMasks::MaskVert(CD_MASK_MDEFORMVERT));
    }
  }
  /* Pole Target. */
  /* TODO(sergey): This should get handled as part of the constraint code. */
  if (data->poletar != nullptr) {
    /* Different object - requires its transform. */
    if (data->poletar != object) {
      ComponentKey target_key(&data->poletar->id, NodeType::TRANSFORM);
      add_relation(target_key, target_dependent_key, con->name);
      /* Ensure target CoW is ready by the time IK tree is built just in case. */
      ComponentKey target_cow_key(&data->poletar->id, NodeType::COPY_ON_WRITE);
      add_relation(
          target_cow_key, init_ik_key, "IK Target CoW -> Init IK Tree", RELATION_CHECK_BEFORE_ADD);
    }
    /* Subtarget references: */
    if ((data->poletar->type == OB_ARMATURE) && (data->polesubtarget[0])) {
      /* Bone - use the final transformation. */
      OperationKey target_key(
          &data->poletar->id, NodeType::BONE, data->polesubtarget, OperationCode::BONE_DONE);
      add_relation(target_key, target_dependent_key, con->name);
    }
    else if (data->polesubtarget[0] && ELEM(data->poletar->type, OB_MESH, OB_LATTICE)) {
      /* Vertex group target. */
      /* NOTE: for now, we don't need to represent vertex groups
       * separately. */
      ComponentKey target_key(&data->poletar->id, NodeType::GEOMETRY);
      add_relation(target_key, target_dependent_key, con->name);
      add_customdata_mask(data->poletar, DEGCustomDataMeshMasks::MaskVert(CD_MASK_MDEFORMVERT));
    }
  }
  DEG_DEBUG_PRINTF((::Depsgraph *)graph_,
                   BUILD,
                   "\nStarting IK Build: pchan = %s, target = (%s, %s), "
                   "segcount = %d\n",
                   pchan->name,
                   data->tar ? data->tar->id.name : "nullptr",
                   data->subtarget,
                   data->rootbone);
  bPoseChannel *parchan = pchan;
  /* Exclude tip from chain if needed. */
  if (!(data->flag & CONSTRAINT_IK_TIP)) {
    parchan = pchan->parent;
  }
  OperationKey parchan_transforms_key(
      &object->id, NodeType::BONE, parchan->name, OperationCode::BONE_READY);
  add_relation(parchan_transforms_key, solver_key, "IK Solver Owner");
  /* Walk to the chain's root. */
  int segcount = 0;
  while (parchan != nullptr) {
    /* Make IK-solver dependent on this bone's result, since it can only run
     * after the standard results of the bone are know. Validate links step
     * on the bone will ensure that users of this bone only grab the result
     * with IK solver results. */
    /** GG: CLEANUP: This branch is so redundant. Can pull else up above.. or even just remove the
     * condition altogether.. and delete above parchan_transforms_key ..
     */
    if (parchan != pchan) {
      OperationKey parent_key(
          &object->id, NodeType::BONE, parchan->name, OperationCode::BONE_READY);
      add_relation(parent_key, solver_key, "IK Chain Parent");
      OperationKey bone_done_key(
          &object->id, NodeType::BONE, parchan->name, OperationCode::BONE_DONE);
      add_relation(solver_key, bone_done_key, "IK Chain Result");
    }
    else {
      OperationKey final_transforms_key(
          &object->id, NodeType::BONE, parchan->name, OperationCode::BONE_DONE);
      add_relation(solver_key, final_transforms_key, "IK Solver Result");
    }
    parchan->flag |= POSE_DONE;
    /* continue up chain, until we reach target number of items. */
    DEG_DEBUG_PRINTF((::Depsgraph *)graph_, BUILD, "  %d = %s\n", segcount, parchan->name);
    /* TODO(sergey): This is an arbitrary value, which was just following
     * old code convention. */
    segcount++;
    if ((segcount == data->rootbone) || (segcount > 255)) {
      BLI_assert(segcount <= 255);
      break;
    }
    parchan = parchan->parent;
  }
  OperationKey pose_done_key(&object->id, NodeType::EVAL_POSE, OperationCode::POSE_DONE);
  add_relation(solver_key, pose_done_key, "PoseEval Result-Bone Link");

  /* Add relation when the root of this IK chain is influenced by another IK chain. For twoway
   * IK's, this also checks that target chain root. */
  // build_inter_ik_chains(object, solver_key, posetree_rootchan, root_map);
  bPoseChannel *owner_and_target_pchans[2] = {chain_rootchan, nullptr};
  const bool is_twoway = (data->flag & CONSTRAINT_IK_IS_TWOWAY) != 0;
  if (is_twoway && (targetchan != nullptr)) {
    /** If oneway, then this solver already implicitly depends on target's DONE
     * (assuming not part of child of owner chain) and so there's no need to add any relations.
     * When its any other value, then it's affected by some solver so we must add relations for our
     * solver. */
    bPoseChannel *target_rootchan = BKE_armature_ik_solver_find_root_ex(targetchan,
                                                                        data->rootbone_target);
    owner_and_target_pchans[1] = target_rootchan;
  }
  for (int i = 0; i < 2; i++) {
    bPoseChannel *_rootchan = owner_and_target_pchans[i];
    if (_rootchan == nullptr) {
      continue;
    }
    Set<StringRefNull> *child_root_names = root_map->get_roots(_rootchan->name);

    parchan = _rootchan->parent;
    while (parchan != nullptr) {
      /**
       * root_map contains each bone's posetree solver and a ref to each chains' root its part of.
       * If a parent doesn't solve as part of a posetree of the chain root, then the child's solver
       * must wait for parent IK solvers to finish.
       *
       * TODO: GG: CLEANUP: move this outside of bone iter loop? will need to re-iter all rootse
       * tho.. can do as cleanup its not required anyways..buyt then, well these roots nor the
       * posetree ever change.. wrell i suppose the ineffiicwency is if multiple cons use the same
       * root, then this is called for the same rootchan bone mul times which is redundant.
       *
       */
      Set<StringRefNull> *parent_root_names = root_map->get_roots(parchan->name);
      if (parent_root_names == nullptr) {
        /* If any parent isn't part of a solver, then there's no way for chain_rootchan's
         * associated solver to evaluate in the wrong order. chain_rootchan's READY will depend
         * on this parent's DONE since it doesn't share any common root in root_map. And the
         * solver depends on rootchan's READY already.
         *
         * The case where we do need to ensure proper relations occurs when an upstream parent is
         * evaluated as part of any same posetree as chain_rootchan and a different branching
         * solver affects the upstream too without including chain_rootchan. In this case,
         * chain_rootchan's solver depends on that branching solver. The former means
         * chain_rootchan's READY only depends on its upstream parents' READY which means
         * chain_roothchan's IK solver isn't implicitly dependent on any upstream solver's DONE.
         * */
        break;
      }

      bool child_uses_different_solver = false;

      for (auto iter = child_root_names->begin(), end = child_root_names->end(); iter != end;
           iter++) {
        const char *child_root_name = iter->c_str();
        /* Shared rootname means shared posetree. */
        if (parent_root_names->contains(child_root_name)) {
          continue;
        }
        bPoseChannel *child_root_pchan = BKE_pose_channel_find_name(object->pose, child_root_name);
        BLI_assert(child_root_pchan);
        /** GG: CLEANUP: make solverchan mapping store bone names to prevent the posechannel
         * lookup..*/
        bPoseChannel *posetree_root = static_cast<bPoseChannel *>(
            BLI_ghash_lookup(solverchan_from_chain_rootchan, child_root_pchan));
        BLI_assert(posetree_root);
        /* Ensure that parent doesn't evaluate as part of this posetree. */
        if (parent_root_names->contains(posetree_root->name)) {
          continue;
        }
        child_uses_different_solver = true;
        break;
      }

      if (child_uses_different_solver) {
        /* Using _rootchan or parchan are interchangable. It shouldn't change anything. */
        OperationKey parent_bone_key(
            &object->id, NodeType::BONE, _rootchan->parent->name, OperationCode::BONE_DONE);
        add_relation(parent_bone_key, solver_key, "IK Chain Overlap");

        /* No need to keep checking upstream, as the upstream of this current parent eventually
         * leads to another rootchan, which has the proper relation added too. */
        break;
      }

      parchan = parchan->parent;
    }
  }
}

/* Spline IK Eval Steps */
void DepsgraphRelationBuilder::build_splineik_pose(Object *object,
                                                   bPoseChannel *pchan,
                                                   bConstraint *con,
                                                   RootPChanMap *root_map)
{
  bSplineIKConstraint *data = (bSplineIKConstraint *)con->data;
  bPoseChannel *rootchan = BKE_armature_splineik_solver_find_root(pchan, data);
  OperationKey transforms_key(&object->id, NodeType::BONE, pchan->name, OperationCode::BONE_READY);
  OperationKey init_ik_key(&object->id, NodeType::EVAL_POSE, OperationCode::POSE_INIT_IK);
  OperationKey solver_key(
      &object->id, NodeType::EVAL_POSE, rootchan->name, OperationCode::POSE_SPLINE_IK_SOLVER);
  OperationKey pose_cleanup_key(&object->id, NodeType::EVAL_POSE, OperationCode::POSE_CLEANUP);
  /* Solver depends on initialization. */
  add_relation(init_ik_key, solver_key, "Init IK -> IK Solver");
  /* Never cleanup before solver is run. */
  add_relation(solver_key, pose_cleanup_key, "IK Solver -> Cleanup");
  /* Attach owner to IK Solver. */
  add_relation(transforms_key, solver_key, "Spline IK Solver Owner", RELATION_FLAG_GODMODE);
  /* Attach path dependency to solver. */
  if (data->tar != nullptr) {
    ComponentKey target_geometry_key(&data->tar->id, NodeType::GEOMETRY);
    add_relation(target_geometry_key, solver_key, "Curve.Path -> Spline IK");
    ComponentKey target_transform_key(&data->tar->id, NodeType::TRANSFORM);
    add_relation(target_transform_key, solver_key, "Curve.Transform -> Spline IK");
    add_special_eval_flag(&data->tar->id, DAG_EVAL_NEED_CURVE_PATH);
  }
  pchan->flag |= POSE_DONE;
  OperationKey final_transforms_key(
      &object->id, NodeType::BONE, pchan->name, OperationCode::BONE_DONE);
  add_relation(solver_key, final_transforms_key, "Spline IK Result");
  root_map->add_bone(pchan->name, rootchan->name);
  /* Walk to the chain's root/ */
  int segcount = 1;
  for (bPoseChannel *parchan = pchan->parent; parchan != nullptr && segcount < data->chainlen;
       parchan = parchan->parent, segcount++)
  {
    /* Make Spline IK solver dependent on this bone's result, since it can
     * only run after the standard results of the bone are know. Validate
     * links step on the bone will ensure that users of this bone only grab
     * the result with IK solver results. */
    OperationKey parent_key(&object->id, NodeType::BONE, parchan->name, OperationCode::BONE_READY);
    add_relation(parent_key, solver_key, "Spline IK Solver Update");
    OperationKey bone_done_key(
        &object->id, NodeType::BONE, parchan->name, OperationCode::BONE_DONE);
    add_relation(solver_key, bone_done_key, "Spline IK Solver Result");
    parchan->flag |= POSE_DONE;
    root_map->add_bone(parchan->name, rootchan->name);
  }
  OperationKey pose_done_key(&object->id, NodeType::EVAL_POSE, OperationCode::POSE_DONE);
  add_relation(solver_key, pose_done_key, "PoseEval Result-Bone Link");

  /* Add relation when the root of this IK chain is influenced by another IK chain. */
  build_inter_ik_chains(object, solver_key, rootchan, root_map);
}

void DepsgraphRelationBuilder::build_inter_ik_chains(Object *object,
                                                     const OperationKey &solver_key,
                                                     const bPoseChannel *rootchan,
                                                     const RootPChanMap *root_map)
{
  bPoseChannel *deepest_root = nullptr;
  const char *root_name = rootchan->name;

  /* Find shared IK chain root. */
  /** GG: ... shouldn't every common root have the relation added? Doesn't this break down
   * For case of 3 ik chains, all w/ diff ik chain roots on same hierarchy?
   * A: Yeah, it breaks in that case. Really.. why not just skip the "has_common_root()" check
   * and just plain add the relation to the parent?
   */
  for (bPoseChannel *parchan = rootchan->parent; parchan; parchan = parchan->parent) {
    if (!root_map->has_common_root(root_name, parchan->name)) {
      break;
    }
    deepest_root = parchan;
  }
  if (deepest_root == nullptr) {
    return;
  }

  OperationKey other_bone_key(
      &object->id, NodeType::BONE, deepest_root->name, OperationCode::BONE_DONE);
  add_relation(other_bone_key, solver_key, "IK Chain Overlap");
}

/* Pose/Armature Bones Graph */
void DepsgraphRelationBuilder::build_rig(Object *object)
{
  /* Armature-Data */
  bArmature *armature = (bArmature *)object->data;
  // TODO: selection status?
  /* Attach links between pose operations. */
  ComponentKey local_transform(&object->id, NodeType::TRANSFORM);
  OperationKey pose_init_key(&object->id, NodeType::EVAL_POSE, OperationCode::POSE_INIT);
  OperationKey pose_init_ik_key(&object->id, NodeType::EVAL_POSE, OperationCode::POSE_INIT_IK);
  OperationKey pose_cleanup_key(&object->id, NodeType::EVAL_POSE, OperationCode::POSE_CLEANUP);
  OperationKey pose_done_key(&object->id, NodeType::EVAL_POSE, OperationCode::POSE_DONE);
  add_relation(local_transform, pose_init_key, "Local Transform -> Pose Init");
  add_relation(pose_init_key, pose_init_ik_key, "Pose Init -> Pose Init IK");
  add_relation(pose_init_ik_key, pose_done_key, "Pose Init IK -> Pose Cleanup");
  /* Make sure pose is up-to-date with armature updates. */
  build_armature(armature);
  OperationKey armature_key(&armature->id, NodeType::ARMATURE, OperationCode::ARMATURE_EVAL);
  add_relation(armature_key, pose_init_key, "Data dependency");
  /* Run cleanup even when there are no bones. */
  add_relation(pose_init_key, pose_cleanup_key, "Init -> Cleanup");
  /* IK Solvers.
   *
   * - These require separate processing steps are pose-level to be executed
   *   between chains of bones (i.e. once the base transforms of a bunch of
   *   bones is done).
   *
   * - We build relations for these before the dependencies between operations
   *   in the same component as it is necessary to check whether such bones
   *   are in the same IK chain (or else we get weird issues with either
   *   in-chain references, or with bones being parented to IK'd bones).
   *
   * Unsolved Issues:
   * - Care is needed to ensure that multi-headed trees work out the same as
   *   in ik-tree building
   * - Animated chain-lengths are a problem. */
  RootPChanMap root_map;
  bool pose_depends_on_local_transform = false;
  /* Fill in root_map data, associating all IK evaluated pchans with their posetrees. For
   * implicitly evaluated pchans, we also add relations (pchan READY -> IK Solver -> pchan
   * DONE). This is necessary so owner/target chain's BONE_READY leads to hierarchy updates and
   * BONE_DONE set by this IK solver while avoiding a cyclic dependency. */
  {
    GHash *solver_from_chain_root = BKE_determine_posetree_roots(&object->pose->chanbase);
    GHash *explicit_pchans_from_posetree_pchan;
    GHash *implicit_pchans_from_posetree_pchan;
    BKE_determine_posetree_pchan_implicity(&object->pose->chanbase,
                                           solver_from_chain_root,
                                           &explicit_pchans_from_posetree_pchan,
                                           &implicit_pchans_from_posetree_pchan);
    {
      /* Add explicit mappings. */
      GHashIterator gh_iter;
      GHASH_ITER (gh_iter, explicit_pchans_from_posetree_pchan) {
        GSet *pchans_set = static_cast<GSet *>(BLI_ghashIterator_getValue(&gh_iter));
        BLI_assert(pchans_set);

        char *posetree_name = NULL;
        {
          bPoseChannel *posetree_chan = static_cast<bPoseChannel *>(
              BLI_ghashIterator_getKey(&gh_iter));
          /** GG: CLEANUP: I wonder if these asserts are just not necessary.. Do i assume
           * debugger caught it earlier or not?*/
          BLI_assert(posetree_chan);
          posetree_name = posetree_chan->name;
        }

        GSetIterator gs_iter;
        GSET_ITER (gs_iter, pchans_set) {
          bPoseChannel *chain_chan = static_cast<bPoseChannel *>(
              BLI_gsetIterator_getKey(&gs_iter));
          BLI_assert(chain_chan);

          root_map.add_bone(chain_chan->name, posetree_name);
        }
      }

      /* Add mappings when an IK chain root's hierarchy is implicitly part of the same posetree.
       * This occurs when 2 two-way chains exist for an armature, let's call them L/R chains.
       * Both chains branch from a common hierarchy. The Lchain goes to armature root and the
       * Rchain is shorter w/o overlapping the Lchain. Now, the entire armature is affected by
       * the same singular posetree. However the nonoverlapping part of Rchain is only implicitly
       * part of the posetree. Despite the rigger or animator explicitly setting a shorter chain
       * length, the common hierarchy will be affected by the posetree solver which means the
       * nonoverlapping part of Rchain is also affected. Thus, we must ensure the Rchain root and
       * the entire implicit chain depends on its parent's BONE_READY instead of BONE_DONE,
       * otherwise a cyclic dependency occurs: -L/R Chain Common IK Affected Parent Done (CAPD)
       *      -> Implicit Chain Root Parent Pose
       *      -> ..
       *      -> RChain Root Ready
       *      -> IK Solver
       *      -> CAPD DONE
       *
       * We must also add relations between the IK solver and the implicit chain. Bones in an
       * implicit chain needs the relations to all IK solvers its implicit to.
       */
      GHASH_ITER (gh_iter, implicit_pchans_from_posetree_pchan) {
        GSet *pchans_set = static_cast<GSet *>(BLI_ghashIterator_getValue(&gh_iter));
        BLI_assert(pchans_set);

        char *posetree_name = NULL;
        {
          bPoseChannel *posetree_chan = static_cast<bPoseChannel *>(
              BLI_ghashIterator_getKey(&gh_iter));
          /** GG: CLEANUP: I wonder if these asserts are just not necessary.. Do i assume
           * debugger caught it earlier or not?*/
          BLI_assert(posetree_chan);
          posetree_name = posetree_chan->name;
        }

        OperationKey solver_key(
            &object->id, NodeType::EVAL_POSE, posetree_name, OperationCode::POSE_IK_SOLVER);

        GSetIterator gs_iter;
        GSET_ITER (gs_iter, pchans_set) {
          bPoseChannel *chain_chan = static_cast<bPoseChannel *>(
              BLI_gsetIterator_getKey(&gs_iter));
          BLI_assert(chain_chan);

          root_map.add_bone(chain_chan->name, posetree_name);

          /* NOTE: These same relations are added for explicit pchans within
           * build_ik_pose().
           *
           * GG: CLEANUP: It would probably be clearer to put the loop above
           * instead of in that function. */
          OperationKey parent_key(
              &object->id, NodeType::BONE, chain_chan->name, OperationCode::BONE_READY);
          add_relation(parent_key, solver_key, "Implicit IK Chain Parent");
          OperationKey bone_done_key(
              &object->id, NodeType::BONE, chain_chan->name, OperationCode::BONE_DONE);
          add_relation(solver_key, bone_done_key, "Implicit IK Chain Result");
        }
      }

      LISTBASE_FOREACH (bPoseChannel *, pchan, &object->pose->chanbase) {
        const BuilderStack::ScopedEntry stack_entry = stack_.trace(*pchan);

        LISTBASE_FOREACH (bConstraint *, con, &pchan->constraints) {
          const BuilderStack::ScopedEntry stack_entry = stack_.trace(*con);
          if ((con->flag & CONSTRAINT_DISABLE) != 0) {
            continue;
          }
          if (con->type != CONSTRAINT_TYPE_KINEMATIC) {
            continue;
          }

          bKinematicConstraint *data = (bKinematicConstraint *)con->data;
          if (data->tar == nullptr) {
            continue;
          }
          if (data->tar->type != OB_ARMATURE) {
            continue;
          }
          if (data->subtarget[0] == '\0') {
            continue;
          }

          if (data->tar != object) {
            continue;
          }

          bPoseChannel *chain_rootchan = BKE_armature_ik_solver_find_root(pchan, data);
          if (chain_rootchan == nullptr) {
            continue;
          }

          bPoseChannel *posetree_rootchan = static_cast<bPoseChannel *>(
              BLI_ghash_lookup(solver_from_chain_root, chain_rootchan));
          BLI_assert(posetree_rootchan);

          /* Prevent target's constraints from linking to anything from same
           * chain that it controls. */
          root_map.add_bone(data->subtarget, posetree_rootchan->name);
        }
      }
    }

    LISTBASE_FOREACH (bPoseChannel *, pchan, &object->pose->chanbase) {
      const BuilderStack::ScopedEntry stack_entry = stack_.trace(*pchan);

      LISTBASE_FOREACH (bConstraint *, con, &pchan->constraints) {
        const BuilderStack::ScopedEntry stack_entry = stack_.trace(*con);

        switch (con->type) {
          case CONSTRAINT_TYPE_KINEMATIC:
            build_ik_pose(object, pchan, con, &root_map, solver_from_chain_root);
            pose_depends_on_local_transform = true;
            break;
          case CONSTRAINT_TYPE_SPLINEIK:
            build_splineik_pose(object, pchan, con, &root_map);
            pose_depends_on_local_transform = true;
            break;
          /* Constraints which needs world's matrix for transform.
           * TODO(sergey): More constraints here? */
          case CONSTRAINT_TYPE_ROTLIKE:
          case CONSTRAINT_TYPE_SIZELIKE:
          case CONSTRAINT_TYPE_LOCLIKE:
          case CONSTRAINT_TYPE_TRANSLIKE:
            /* TODO(sergey): Add used space check. */
            pose_depends_on_local_transform = true;
            break;
          default:
            break;
        }
      }
    }

    BLI_ghash_free(solver_from_chain_root, NULL, NULL);
    BLI_ghash_free(explicit_pchans_from_posetree_pchan, NULL, BLI_gset_freefp_no_keyfree);
    BLI_ghash_free(implicit_pchans_from_posetree_pchan, NULL, BLI_gset_freefp_no_keyfree);
    solver_from_chain_root = NULL;
    explicit_pchans_from_posetree_pchan = NULL;
    implicit_pchans_from_posetree_pchan = NULL;
  }

  // root_map.print_debug();
  if (pose_depends_on_local_transform) {
    /* TODO(sergey): Once partial updates are possible use relation between
     * object transform and solver itself in its build function. */
    ComponentKey pose_key(&object->id, NodeType::EVAL_POSE);
    ComponentKey local_transform_key(&object->id, NodeType::TRANSFORM);
    add_relation(local_transform_key, pose_key, "Local Transforms");
  }
  /* Links between operations for each bone. */
  LISTBASE_FOREACH (bPoseChannel *, pchan, &object->pose->chanbase) {
    const BuilderStack::ScopedEntry stack_entry = stack_.trace(*pchan);

    build_idproperties(pchan->prop);
    OperationKey bone_local_key(
        &object->id, NodeType::BONE, pchan->name, OperationCode::BONE_LOCAL);
    OperationKey bone_pose_key(
        &object->id, NodeType::BONE, pchan->name, OperationCode::BONE_POSE_PARENT);
    OperationKey bone_ready_key(
        &object->id, NodeType::BONE, pchan->name, OperationCode::BONE_READY);
    OperationKey bone_done_key(&object->id, NodeType::BONE, pchan->name, OperationCode::BONE_DONE);
    pchan->flag &= ~POSE_DONE;
    /* Pose init to bone local. */
    add_relation(pose_init_key, bone_local_key, "Pose Init - Bone Local", RELATION_FLAG_GODMODE);
    /* Local to pose parenting operation. */
    add_relation(bone_local_key, bone_pose_key, "Bone Local - Bone Pose");
    /* Parent relation. */
    if (pchan->parent != nullptr) {
      OperationCode parent_key_opcode;
      /* NOTE: this difference in handling allows us to prevent lockups
       * while ensuring correct poses for separate chains. */
      if (root_map.has_common_root(pchan->name, pchan->parent->name)) {
        parent_key_opcode = OperationCode::BONE_READY;
      }
      else {
        /** TODO: GG: Need to check if pchan and any parent in its hierarchy share a common
         * root that's the root and any IK chain.. This means that I need to maintain another
         * map since it currently is a mapping from-- wait i wonder if I can re-use root_map
         * and also store the roots of all chains instead of just a ref to the solver? Nothign
         * else uses such info, and I only really use it to test whether two bones eval as part
         * of any same posetree. To share roots also means you eval as part of the same
         * posetree for some posetree.
         *
         * Maybe it'd be simplest to make a function in the iksolver (or armature_update where
         * solver also has access) that effectively calcualtes all the posetree data. Then
         * depsgraph and iksolver use this. That way both are in sync and its easier to work
         * with. I mean, the root_map  check is to check if you're part of the same posetree..
         * so why not just calculate it already... -maybe because it might slow down depsgarph
         * build?
         *
         * Also why is IK tree initialization/posetree-build part of depsgrpah eval when
         * anything that changes teh posetree requires the depspgraph to rebuild anyways...
         * might as well just initialize it once on depsgraph build..maybe that is how it sorta
         * works and init doesn't occur that often. The reason is probably that i'm wrong.
         * posetree building mght still depend on things, so its order of eval is important
         * which is why its in the depsgraph..No bceause that inherently means the depsgraph
         * needs to be rebuilt too..
         *
         * I think I can calc all the data needed in the same func that determines pose tree
         * solver roots? As I walk the chains, just plain add the bones to their chain roots.
         * In hte post process pass, i'll also add them to the solver root?
         */
        parent_key_opcode = OperationCode::BONE_DONE;
      }

      OperationKey parent_key(&object->id, NodeType::BONE, pchan->parent->name, parent_key_opcode);
      add_relation(parent_key, bone_pose_key, "Parent Bone -> Child Bone");
    }
    /* Build constraints. */
    if (pchan->constraints.first != nullptr) {
      /* Build relations for indirectly linked objects. */
      BuilderWalkUserData data;
      data.builder = this;
      BKE_constraints_id_loop(&pchan->constraints, constraint_walk, &data);
      /* Constraints stack and constraint dependencies. */
      build_constraints(&object->id, NodeType::BONE, pchan->name, &pchan->constraints, &root_map);
      /* Pose -> constraints. */
      OperationKey constraints_key(
          &object->id, NodeType::BONE, pchan->name, OperationCode::BONE_CONSTRAINTS);
      add_relation(bone_pose_key, constraints_key, "Pose -> Constraints Stack");
      add_relation(bone_local_key, constraints_key, "Local -> Constraints Stack");
      /* Constraints -> ready/ */
      /* TODO(sergey): When constraint stack is exploded, this step should
       * occur before the first IK solver. */
      add_relation(constraints_key, bone_ready_key, "Constraints -> Ready");
    }
    else {
      /* Pose -> Ready */
      add_relation(bone_pose_key, bone_ready_key, "Pose -> Ready");
    }
    /* Bone ready -> Bone done.
     * NOTE: For bones without IK, this is all that's needed.
     *       For IK chains however, an additional rel is created from IK
     *       to done, with transitive reduction removing this one. */
    add_relation(bone_ready_key, bone_done_key, "Ready -> Done");

    /* B-Bone shape is the real final step after Done if present. */
    if (check_pchan_has_bbone(object, pchan)) {
      OperationKey bone_segments_key(
          &object->id, NodeType::BONE, pchan->name, OperationCode::BONE_SEGMENTS);
      /* B-Bone shape depends on the final position of the bone. */
      add_relation(bone_done_key, bone_segments_key, "Done -> B-Bone Segments");
      /* B-Bone shape depends on final position of handle bones. */
      bPoseChannel *prev, *next;
      BKE_pchan_bbone_handles_get(pchan, &prev, &next);
      if (prev) {
        OperationCode opcode = OperationCode::BONE_DONE;
        /* Inheriting parent roll requires access to prev handle's B-Bone properties. */
        if ((pchan->bone->bbone_flag & BBONE_ADD_PARENT_END_ROLL) != 0 &&
            check_pchan_has_bbone_segments(object, prev))
        {
          opcode = OperationCode::BONE_SEGMENTS;
        }
        OperationKey prev_key(&object->id, NodeType::BONE, prev->name, opcode);
        add_relation(prev_key, bone_segments_key, "Prev Handle -> B-Bone Segments");
      }
      if (next) {
        OperationKey next_key(&object->id, NodeType::BONE, next->name, OperationCode::BONE_DONE);
        add_relation(next_key, bone_segments_key, "Next Handle -> B-Bone Segments");
      }
      /* Pose requires the B-Bone shape. */
      add_relation(
          bone_segments_key, pose_done_key, "PoseEval Result-Bone Link", RELATION_FLAG_GODMODE);
      add_relation(bone_segments_key, pose_cleanup_key, "Cleanup dependency");
    }
    else {
      /* Assume that all bones must be done for the pose to be ready
       * (for deformers). */
      add_relation(bone_done_key, pose_done_key, "PoseEval Result-Bone Link");

      /* Bones must be traversed before cleanup. */
      add_relation(bone_done_key, pose_cleanup_key, "Done -> Cleanup");

      add_relation(bone_ready_key, pose_cleanup_key, "Ready -> Cleanup");
    }
    /* Custom shape. */
    if (pchan->custom != nullptr) {
      build_object(pchan->custom);
      add_visibility_relation(&pchan->custom->id, &armature->id);
    }
  }
}

}  // namespace blender::deg
