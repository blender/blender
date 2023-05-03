/* SPDX-License-Identifier: GPL-2.0-or-later Copyright 2001-2002 NaN Holding BV. All rights
 * reserved. */

/** \file \ingroup ikplugin
 */

#include "MEM_guardedalloc.h"

#include "BIK_api.h"
#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"

#include "IK_solver.h"
#include "iksolver_plugin.h"

#include <string.h> /* memcpy */

#define USE_NONUNIFORM_SCALE

/* ********************** THE IK SOLVER ******************* */
/**
 * Appends chanlist to tree->pchan, taking care of stitching parents together properly. Order of
 * chanlist is assumed to be [tip .. root] (GG: CLEANUP: It'd be simpler to assume the order of
 * [root..tip]).
 *
 * Assumes chanlist root is an existing root in tree->pchan, or a new root to be added to
 * tree->pchan. Otherwise, parenting won't be properly setup and pchans may have duplicate entries
 * in the tree.
 */
static int posetree_append_chanlist(PoseTree *tree, bPoseChannel **chanlist, const int segcount)
{
  /** chanlist: Assumes root is last element, tip is first. Allows chanlist to not have a parent in
   * tree. Assumes all target chanlist's either contain same root or don't contain any ancestor of
   * another target chanlist. */
  /** GG: TODO: If 2-way IK works well, then will need to update depsgraph so the relations are
   * correct when for bones that depend on target's chain. Dependents need to also depend on tip's
   * ik solver to finish, wihch simplifies to making target's BoneDone depend on tip's IK sovler
   * and ofc tip's IK solver exec to onmly dep on target's BoneReady (which is just a pad node for
   * IK stuff iirc)
   */

  /* Skip common pose channels and add remaining. */
  /** GG: NOTE: .. min() is redundant? */
  int size = MIN2(segcount, tree->totchannel);
  int a = 0;
  int t = 0;
  /** GG: NOTE: 'a' is the index of the first branched path between the existing tree and the new
   * con's tree. So (a-1) is the last shared parent between both branches. We repeat this up to the
   * largest 'a', We search through all paths (which all exist within a single tree) until we get
   * to longest path that immediately branches from an existing path. 'a' is the index within
   * chanlist of the first pchan not in the tree yet.
   *
   * if con extends a tree, then 'a' equal to first new pchan, from chanlist
   *
   * This works well unless the root of the new con extends further (closer to root) than the tree.
   * Then a = len(chanlist) -1.. but ofc we shouldn't treat it as completely new pchans..right?
   * --this doesnt occur. A tree is placed on teh root. So a different root is a differnt tree.
   *
   * ~Following uses assumption of chanlist root.
   */
  int index_of_tip = -1;
  while (a < size && t < tree->totchannel) {
    /* locate first matching channel */
    for (; t < tree->totchannel && tree->pchan[t] != chanlist[segcount - a - 1]; t++) {
      /* pass */
    }
    if (t >= tree->totchannel) {
      break;
    }
    for (; a < size && t < tree->totchannel && tree->pchan[t] == chanlist[segcount - a - 1];
         a++, t++) {
      index_of_tip = t;
      /* pass */
    }
  }

  const int newly_added_segcount = segcount - a;
  /** TODO: GG: BUG:?... but what if we have a single path chain then added a shorter IK con? Then
   * segment=0, and the tip is definitely not this value... it would be (a-1)==(segcount-1), i
   * think... ..i think it hasn't occured yet maybe due to depsgraph call initalize_posetree in
   * order of parent to child?
   */
  if (newly_added_segcount == 0) {
    return index_of_tip;
  }

  /** GG: NOTE: althoguh tree->pchan[a-1] not necssarily parent, its a good starting point. I guess
   * this is an optimization to begin at a-1? It's not necessarily a parent since (a-1) is just the
   * number of matching pchans, which is definitely before or at the first new pchan parent's
   * index.
   */
  int parent;
  for (parent = a - 1; parent < tree->totchannel; parent++) {
    if (tree->pchan[parent] == chanlist[newly_added_segcount - 1]->parent) {
      break;
    }
  }

  /* shouldn't happen, but could with dependency cycles */
  if (parent == tree->totchannel) {
    parent = a - 1;
  }

  /* resize array */
  int newsize = tree->totchannel + newly_added_segcount;
  bPoseChannel **oldchan = tree->pchan;
  int *oldparent = tree->parent;

  tree->pchan = MEM_callocN(newsize * sizeof(void *), "ik tree pchan");
  tree->parent = MEM_callocN(newsize * sizeof(int), "ik tree parent");

  memcpy(tree->pchan, oldchan, sizeof(void *) * tree->totchannel);
  memcpy(tree->parent, oldparent, sizeof(int) * tree->totchannel);
  MEM_freeN(oldchan);
  MEM_freeN(oldparent);

  /* add new pose channels at the end, in reverse order */
  for (a = 0; a < newly_added_segcount; a++) {
    tree->pchan[tree->totchannel + a] = chanlist[newly_added_segcount - a - 1];
    tree->parent[tree->totchannel + a] = tree->totchannel + a - 1;
  }

  tree->parent[tree->totchannel] = parent;
  bPoseChannel *newly_added_root_bone = chanlist[newly_added_segcount - 1];
  /* Only not equal when the parent doesn't exist in the posetree. */
  /** TODO: GG: problem: if two IK constraints have diff targets but same target root and one
   * target is a parent of the other, then we need to patch that parent in.
   *
   * Proper order is necessary for proper applying of IK matrices post ik solver. Even if there are
   * gaps in parents, it does matter ofr iksolver for clculating proper world xofrm. So we need to
   * support gaps in parents too...and what if that parent gap isn't trivial parenting (i.e.
   * doesn't inehrit rotation).. then how do we handle the gap xform properly?? -maybe, don't
   * connect segmetns across gap (missing) parents? This is more likely the desired behavior, where
   * the ik solver simply doesn't solve past the gap and treats as a separate IK pass?..but still..
   * for proper world xform calculation, we do need to account for the gap parents even if we won't
   * modify them with the solver..
   *
   * XXX: For now, just assume all two-way IK's go to same target root if any target chain contains
   * an ancestor of any other target chain? Under this assumption, everything works fine as is.
   * This should be fine for now, we're just trying to figure out if this "two-way-IK" is even
   * useful.
   */
  if (tree->pchan[parent] != newly_added_root_bone->parent) {
    tree->parent[tree->totchannel] = -1;
  }

  tree->totchannel = newsize;
  index_of_tip = newsize - 1;
  /** GG: XXX: Bug to fix.. hundreds of bones can occur? Due to AutoIK and pinning maybe? */
  BLI_assert_msg(tree->totchannel < 30, "... how");

  return index_of_tip;
}

/* allocates PoseTree, and links that to root bone/channel */
/* NOTE: detecting the IK chain is duplicate code...
 * in drawarmature.c and in transform_conversions.c */
static void initialize_posetree(struct Object *UNUSED(ob),
                                bPoseChannel *pchan_tip,
                                struct GHash *solverchan_from_chain_rootchan,
                                struct GHash *explicit_pchans_per_solverchan,
                                struct GHash *implicit_pchans_per_solverchan)
{
  /**
   * GG: TODO: CLEANUP: HIGH: PRIORITY: Stop putting notes about todos and
   *    Q's/potential-solutions in the code itself -it's hard to verify that
   *    it's still relevant and can easily be forgotten about. Maybe it'd be
   *    easier to use a single text file? Atleast its a single place to look. I
   *    should also track features and bug fixes in there too since tracking by
   *    code is hard since it requires comparing to vanilla Blender, which is
   *    general most obvious at the time of coding but not when coming back to
   *    the code after too long of a time.
   *
   * GG: TODO: BUG: (LOW) IK constraint muting doesn't evaluate properly (CONSTRAINT_OFF).
   *    (This is a bug in vanilla Bledner 4.0.2 and 3.6). The solutoin is for tree
   *    init to not skip muted/zero-influence IK constraints. Those should instead
   *    become implicit IK chains (assuming the bones aren't used explicitly by enabled
   *    IK constraints). I wonder if they can just not be part ofthe IK and just, post solve,
   *    calculate thier standard paernt-child hierarchy xform results?
   *      -the proper solution is to treat it as if the IK constraint doesn't exist.
   *          - we must respect non-standard hierarchy properties (inherit_rot=OFF, loc_location, scale inheritance etc)
   *          -which means, that it's not as simple as treating the bones as implicit.
   *        solution:
   *            For bones that are part of muted IKs:
   *               TODO: need to havea function thta determines implicity while considering muted-ness (shouldn't affect depsgraph relations)
   *               If they become implciit, then let IK apply their heirarch (trivial solution)
   *               If they don't become implicit (and notably also not explicitly) related to any IK, 
   *                  then the bone needs to be evaluated with their normal hierarchy relations.
   *                  .. which also requires that these bones evaluate after their parent's IKSOLVER
   *                  but currently such bones all eval after parent's READY to avoid a cyclic eval..
   *                  ..and it's not just hierarchy but also constraints eval that needs to be done after
   *                  parent's IKSOlver+Done.. which means proper depsgraph ordering.. which mneans 
   *                  the solution must include  changes to the depsgraph (adding post IK solver bone nodes
   *                  that are noops if all IK cons are active) that are effectively a duplicate of the nodes
   *                  and relations of those between parent done/ready to child pre-ik solver..
   *     -LOW priority reason: bug also exists in vanilla blender 4.0.2 and lower.
   *
   * GG: CLEANUP: instead of storing chains in order of tip to root, store it in root to tip. That
   * way accessors don't having to iterate in reverse... which is needlessly confusing..
   *
   * GG: TODO: SUPPORT: (LOW) Given two IK chains (same root), where one chain (A) points
   *    to the other chain (B) as a target, then we should not treat that as a
   *    cyclic dependency. The goal should just be a reference instead of a
   *    constant position. It's similar to 2way IK. Vanilla blender treats this
   *    case as a cyclic dependency. Though, for the shared hierarchy, the IK
   *    shouldn't modify those bones when solving for the goal of the owner
   *    chain since it's redundant. And the IK solver doesn't support
   *    specifically cutting off chain length per target, for the case where
   *    there is 3 IK chains and the third (C) chain overlaps with A and shares
   *    the same root. If it was just A+B, then we technically could make the
   *    branch point the segment root for A. Then it'll solve properly. With C,
   *    that's not a solution.
   *
   *    - A solution would be: detect that A's shares a hierarchy with B, then
   *      when calculating the jacobian for an A-bone's offset effect on the end
   *      effector, we also account for whether the current bone is a parent of
   *      the target? For that case, the offset effect is zero since the parent
   *      will move both the end effector and target by the same amount (assumes
   *      scaling is not possible). Note, we would have to use the original
   *      bPoseChannel hierarchies, not the PoseTree cached hierarchy since the
   *      latter can be shorter and exclude the shared hierarchy information.
   *      Per solver task, we can send in an additional per-segment weight of 0
   *      or 1.
   *
   *    -LOW priority reason: Currently I don't have any use cases for
   *    interdependent one-way IK chains.
   *
   *    - will have to keep around a rootmap during IK solving.
   *
   *    -BUG: A 2way IK setup version of this has unnecessary rotations since it
   *     doesn't (but should) account for the target sharing hierarchies. (It
   *     doesn't result in cyclic dependencies, good)
   * */
  /* Set translation segment's start so that tail is the rest position. This allows translation
   * limits to use the tail as origin. */
  bPoseChannel *curchan, *pchan_root = NULL, *chanlist[256], **oldchan;
  PoseTree *tree;
  PoseTarget *target;
  bConstraint *con;
  bKinematicConstraint *data;
  int a, t, segcount = 0, size, newsize, *oldparent, parent;

  /* find IK constraint, and validate it */
  /** TODO: GG: .. .defiinitely hsould grab the last one.. not first...
   * TODO: GG: .. and why do we only limit to one IK constraint?
   */
  for (con = pchan_tip->constraints.first; con; con = con->next) {
    if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
      data = (bKinematicConstraint *)con->data;
      if (data->flag & CONSTRAINT_IK_AUTO) {
        break;
      }
      if (data->tar == NULL) {
        continue;
      }
      if (data->tar->type == OB_ARMATURE && data->subtarget[0] == 0) {
        continue;
      }
      if ((con->flag & (CONSTRAINT_DISABLE | CONSTRAINT_OFF)) == 0 && (con->enforce != 0.0f)) {
        break;
      }
    }
  }
  if (con == NULL) {
    return;
  }
  if (data->flag & CONSTRAINT_IK_DO_NOT_CREATE_POSETREE) {
    return;
  }

  if (!(data->flag & CONSTRAINT_IK_TIP)) {
    pchan_tip = pchan_tip->parent;
  }

  /* Find the chain's root & count the segments needed */
  for (curchan = pchan_tip; curchan; curchan = curchan->parent) {
    pchan_root = curchan;

    /** GG: NOTE:... this isnt even completely cleared for all bones in teh chain... is it
     * supoposed to be?*/
    curchan->flag |= POSE_CHAIN; /* don't forget to clear this */
    chanlist[segcount] = curchan;
    segcount++;

    if (segcount == data->rootbone || segcount > 255) {
      BLI_assert(segcount <= 255);
      break; /* 255 is weak */
    }
  }
  curchan = NULL;

  /** GG: NOTE: I don't know why this block exists? branched IK works fine, all in one tree.
   * The only 3 cases afaik for an IK tree is:
   *  1) no tree exists, so we create a new one
   *  2) a tree exists, and we branch off from it. No pre-existing chain tip occurs in this con.
   * Branching is OK, so branch off the existing one, especically since the roots are the same.
   *
   * 3)a tree exsts, adn we extend it. Pre-existing chain tip occurs but that's fine. It appears to
   * solve fine.
   *
   * NOTE: I have no idea what case a 2nd tree is created where just plain using the existing one
   * won't work??
   *
   */
  bPoseChannel *posetree_root = BLI_ghash_lookup(solverchan_from_chain_rootchan, pchan_root);
  BLI_assert(posetree_root);

  GSet *implicit_pchans = BLI_ghash_lookup(implicit_pchans_per_solverchan, posetree_root);
  BLI_assert(implicit_pchans);
  GSet *explicit_pchans = BLI_ghash_lookup(explicit_pchans_per_solverchan, posetree_root);
  BLI_assert(explicit_pchans);

  tree = posetree_root->iktree.first;

  /* Extend the chanlist with their evaluated parents so that all chains are properly appended for
   * other chains in the posetree. */
  {
    bPoseChannel *parent_chan = pchan_root->parent;
    while (BLI_gset_haskey(implicit_pchans, parent_chan) ||
           BLI_gset_haskey(explicit_pchans, parent_chan))
    {
      chanlist[segcount] = parent_chan;
      /** GG: Q: Breakpoint: When does this occur for a single 2way chain + oneways due to AutoIK?
       * **/
      parent_chan->flag |= POSE_CHAIN; /* don't forget to clear this: GG: Is this still relevant */

      segcount++;
      if (segcount > 255) {
        BLI_assert(false);
        break; /* 255 is weak */
      }
      parent_chan = parent_chan->parent;
    }
  }

  pchan_tip->flag &= ~POSE_CHAIN;

  if (tree == NULL) {
    /** GG: STEP: move up to before target chanlist created*/
    /* make new tree */
    tree = MEM_callocN(sizeof(PoseTree), "posetree");

    tree->type = CONSTRAINT_TYPE_KINEMATIC;

    tree->iterations = data->iterations;
    tree->totchannel = segcount;
    tree->stretch = (data->flag & CONSTRAINT_IK_STRETCH);

    tree->pchan = MEM_callocN(tree->totchannel * sizeof(void *), "ik tree pchan");
    tree->parent = MEM_callocN(tree->totchannel * sizeof(int), "ik tree parent");
    /** GG: I wonder if ik chains are required to be from parent-child bones.
     * Q: Can we create an IK chain that connects the left palm to right palm? left foot to right?
     * Q: I wonder if the pole must be associated w/ the chain base? For long IK chains, it'd be
     *    nice if, for example, (l/r palm case) if I can specify even multiple poles for specific
     *    bones (like ensuring the arms don't twist awkwardly relative to parent)
     *        A: For this, you can have 2 Ik chains, the shorter one evaluates 1st, the longer
     *        extension evaluates after. To separate the evaluations, the longer extension has to
     *        be an overlapping chain that copyxforms the shorter chain. If they don't share roots,
     *        then copyxform duplication not necessary.
     *      */
    for (a = 0; a < segcount; a++) {
      tree->pchan[a] = chanlist[segcount - a - 1];
      tree->parent[a] = a - 1;
    }

    if (data->tar) {
      /* Create a target. AutoIK targetless is handled in execute(). */
      target = MEM_callocN(sizeof(PoseTarget), "posetarget");
      target->con = con;

      target->tip = segcount - 1;
      target->target = -1;
      target->zero_weight_sentinel_index = -1;
      BLI_addtail(&tree->targets, target);
    }
    // if (has_target_chain) {
    //   for (a = 0; a < segcount_target; a++) {
    //     const int tree_element_index = a + segcount;

    //     tree->pchan[tree_element_index] = chanlist_target[segcount_target - a - 1];
    //     tree->parent[tree_element_index] = tree_element_index - 1;
    //   }
    //   tree->parent[segcount] = -1;
    //   target->target = tree->totchannel - 1;
    //   target->flag |= POSETARGET_CON_IS_TWOWAY;
    // }

    /* AND! link the tree to the root */
    /** GG: There is only ever one tree on a particular pchan? No need to store a list?*/
    BLI_addtail(&posetree_root->iktree, tree);
    BLI_assert_msg(tree->totchannel < 30, "... how");
  }
  else {
    tree->iterations = MAX2(data->iterations, tree->iterations);
    /** TODO: GG: XXX: .. should the ! be there? (dind't remove yet since i'm focused on other bugs
     * atm)*/
    tree->stretch = tree->stretch && !(data->flag & CONSTRAINT_IK_STRETCH);

    const int tip_index = posetree_append_chanlist(tree, chanlist, segcount);

    if (data->tar) {
      /* Create a target. AutoIK targetless is handled in execute(). */
      target = MEM_callocN(sizeof(PoseTarget), "posetarget");
      target->con = con;

      target->tip = tip_index;
      target->target = -1;
      target->zero_weight_sentinel_index = -1;
      BLI_addtail(&tree->targets, target);
    }

    /** GG: There is only ever one tree on a particular pchan? No need to do this. */
    /* move tree to end of list, for correct evaluation order */
    BLI_remlink(&posetree_root->iktree, tree);
    BLI_addtail(&posetree_root->iktree, tree);
  }

  tree->implicit_pchans = implicit_pchans;
  tree->explicit_pchans = explicit_pchans;
  /* mark root channel having an IK tree */
  posetree_root->flag |= POSE_IKTREE;

  bPoseChannel *target_chan = NULL;
  if (data->tar != NULL && data->tar->type == OB_ARMATURE && data->subtarget[0] != 0) {
    target_chan = BKE_pose_channel_find_name(data->tar->pose, data->subtarget);
  }
  if (target_chan == NULL) {
    return;
  }

  /* Only append and mark target as 2way if its evaluated. No need to check for
   * CONSTRAINT_IK_IS_TWOWAY since the flag is set if target is in explicit_pchans and if it's not
   * set, it may still be evaluated implictly.*/
  const bool is_target_evaluated = BLI_gset_haskey(explicit_pchans, target_chan) ||
                                   BLI_gset_haskey(implicit_pchans, target_chan);
  if (!is_target_evaluated)
    return;

  bPoseChannel *chanlist_target[256];
  int segcount_target = 0;
  {
    curchan = target_chan;
    /* Find the chain's root & count the segments needed */
    for (; curchan; curchan = curchan->parent) {
      /** GG: NOTE: TODO: is this important for the target chain? */
      curchan->flag |= POSE_CHAIN; /* don't forget to clear this
                                    */
      chanlist_target[segcount_target] = curchan;
      segcount_target++;

      if (segcount_target == data->rootbone_target || segcount_target > 255) {
        BLI_assert(segcount_target <= 255);
        break; /* 255 is weak */
      }
    }
  }
  curchan = NULL;

  /* Extend the chanlist with their evaluated parents so that all chains are properly appended for
   * other chains in the posetree. */
  {
    bPoseChannel *parent_chan = chanlist_target[segcount_target - 1]->parent;
    while (BLI_gset_haskey(implicit_pchans, parent_chan) ||
           BLI_gset_haskey(explicit_pchans, parent_chan))
    {
      chanlist_target[segcount_target] = parent_chan;
      parent_chan->flag |= POSE_CHAIN; /* don't forget to clear this: GG: Is this still relevant */

      segcount_target++;
      if (segcount_target > 255) {
        break; /* 255 is weak */
      }
      parent_chan = parent_chan->parent;
    }
  }

  BLI_assert_msg(data->tar != NULL,
                 "Did not expect targetless autoIK to use 2way IK. It doesn't make sense");
  target->target = posetree_append_chanlist(tree, chanlist_target, segcount_target);

  int index_closest_shared_ancestor = 0;
  while (index_closest_shared_ancestor < segcount_target &&
         index_closest_shared_ancestor < segcount &&
         chanlist_target[segcount_target - 1 - index_closest_shared_ancestor] ==
             chanlist[segcount - 1 - index_closest_shared_ancestor])
  {
    index_closest_shared_ancestor++;
  }
  index_closest_shared_ancestor -= 1;
  if (index_closest_shared_ancestor <= -1) {
    return;
  }
  target->zero_weight_sentinel_index = index_closest_shared_ancestor;

  // target->flag |= POSETARGET_CON_IS_TWOWAY;

  /** TODO: GG: user shuold speecify when a new posetree is created.
   * TODO: GG: when target is descendent of root, then chain needs to be split at nearest ancestor,
   * however we still wantthe same root to store the pose tree so that, if the target itself is
   * part of an IK chain, then these 2 now pull on eachother properly. Otherwise, a completely new
   * posetree has independent effects on the two IK chains. (this is a case where maybe itd be
   * simpler for user to specify which posetree to eval with?: (posetree container pchan, tree eval
   * index)--> effectively a property that used to be automatically solved is now manually up to
   * user (we can use auto behavior for sane defaults)
   *    TODO: XXX: GG: (introduced by me probably) An Ik that points at a differnt IK's mid chain
   *    (same root) doesn't work (doesn't update properly) I think this is teh case where a new
   * tree shouild've been created. (when Ik target occurs in a chain of an existing tree, then it
   * can't branched off, it must eval afterwards))... low priority: Blender complains thats its a
   * cyclic dep. anyways.
   * */
  /** Look for the tree that shares any chain pbones. We'll append to that tree. */
  /** GG: NOTE: maybe devs didn't add support for intersecting a chain (or it does work) since,
   * despgraph-wise, the depended on chain needs to fully eval for proper eval (constraints,
   * drivers, etc). Then this new con will eval its IK. The result appears as if the depended on
   * tree detaching, but it is solving properly, just not in parallel, thus ofc we lose a core
   * behavior of IK evaluation.. but depsgraphw-ise i don't think theres a proper solution that
   * includes allowing the rigger to constrain non-IK properly to the "parent" ik chain.
   * ...though.. one SOLUTION: duplicate the parent IK trees into the extended root (only necessary
   * if the trees overlap, not if tehy only meet at junction: eval is already fine in tht cse).
   * That means we are able to constrain to the base parent result while preserving the ik
   * parelling solving behavior.
   *    PROBLEM: though.. that still leads to issues for bones that depend on the parent chain
   * constraint-wise. What shouild the result of their con be when it targets the parent ik tree?
   * the old result or the new one?
   */
  // if (tree == NULL) {
  //   for (int pchan_index = 0; pchan_index < segcount; pchan_index++) {
  //     bPoseChannel *pchan = chanlist[pchan_index];
  //     for (tree = pchan->iktree.first; tree; tree = tree->next) {
  //       for (target = tree->targets.first; target; target = target->next) {
  //         bKinematicConstraint *con_ik = target->con->data;
  //         //....but to get that info, i need to go over all armature bones, not just thse
  //         ones...
  //       }
  //     }
  //   }
  // }
  /* we make tree-IK, unless all existing targets are in this chain */
  // for (tree = pchan_root->iktree.first; tree; tree = tree->next) {
  //   for (target = tree->targets.first; target; target = target->next) {
  //     curchan = tree->pchan[target->tip];
  //     if (curchan->flag & POSE_CHAIN) {
  //       curchan->flag &= ~POSE_CHAIN;
  //     }
  //     else {
  //       break;
  //     }
  //   }
  //   /** target is NULL in 3 cases:
  //    * 1) tree is empty (unlikely)
  //    *
  //    * 2) the constraint completely includes all pchan_tips of the current tree (thus tree ends
  //         up
  //    * null and we mistakenly create a new tree) -this occurs for the trivial single-chain w/
  //    mul
  //    * IK's, even if all go to root. A new tree should not be created.
  //    *
  //    * XXX: Im pretty sure IK also breaks on a single chain when IKs don't have the same root
  //    yet
  //    * the code seems like itd be fine in taht case?
  //      */
  //   if (target) {
  //     break;
  //   }
  // }
}

/* transform from bone(b) to bone(b+1), store in chan_mat */
static void make_dmats(bPose *pose, PoseTree *tree)
{
  for (int a = 0; a < tree->totchannel; a++) {
    bPoseChannel *pchan = tree->pchan[a];

    if (pchan->parent) {
      float iR_parmat[4][4];
      invert_m4_m4(iR_parmat, pchan->parent->pose_mat);
      mul_m4_m4m4(pchan->chan_mat, iR_parmat, pchan->pose_mat); /* delta mat */
    }
    else {
      copy_m4_m4(pchan->chan_mat, pchan->pose_mat);
    }

    /** GG: DESIGN: I wonder if we have it backwards. Maybe a iksolver that's a subchain of a
     * larger iksolver should evaluate before the larger solver? In vanilla blender, this a smaller
     * IK subchain evaluates after the larger one which leads to a disconnected chain because the
     * children below the smaller subchain tip don't get their posemats updates. That's what the
     * below code fixes. But maybe it'd be more useful to evaluate with the smaller subchain first?
     * Or maybe order it by tip instead of root? If a tip pchan occurs upstream, then that iksolver
     * evaluates first?  I think that makes the most sense and seems useful? When tips aren't
     * children of another, then we can order by roots, where the upstream/closer-to-armature-roots
     * eval first (branched IK chains).
     *
     * XXX:TODO: I can do the manual update, but I should probably respect their hierarchy
     * relations? (Local rotation, scaling, etc). So commented out for now.
     */
    // float iR_mat[4][4];
    // invert_m4_m4(iR_mat, pchan->pose_mat);
    // LISTBASE_FOREACH (Bone *, child_bone, &pchan->bone->childbase) {
    //   bPoseChannel *child_chan = BKE_pose_channel_find_name(pose, child_bone->name);
    //   if (child_chan == NULL) {
    //     continue;
    //   }
    //   if (BLI_gset_haskey(tree->explicit_pchans, child_chan)) {
    //     continue;
    //   }
    //   if (BLI_gset_haskey(tree->implicit_pchans, child_chan)) {
    //     continue;
    //   }
    //   mul_m4_m4m4(child_chan->chan_mat, iR_mat, child_chan->pose_mat);
    // }
  }
}

/* applies IK matrix to pchan, IK is done separated */
/* formula: pose_mat(b) = pose_mat(b-1) * diffmat(b-1, b) * ik_mat(b) */
/* to make this work, the diffmats have to be precalculated! Stored in chan_mat */
static void where_is_ik_bones(bPose *pose,
                              PoseTree *tree) /* nr = to detect if this is first bone */
{
  for (int a = 0; a < tree->totchannel; a++) {
    bPoseChannel *pchan = tree->pchan[a];
    float *ik_mat = tree->basis_change[a];
    float *translation = tree->translation_change[a];
    float vec[3], ikmat[4][4];

    copy_m4_m3(ikmat, ik_mat);

    add_v3_v3(pchan->chan_mat[3], translation);
    // GG: Subtle, we did make_dmats() so that we have our original basis relative to the parent.
    // We update bones in order of parent to child so the parent->pose_mat is the IK solved pose at
    // this point.
    if (pchan->parent) {
      mul_m4_m4m4(pchan->pose_mat, pchan->parent->pose_mat, pchan->chan_mat);
    }
    else {
      copy_m4_m4(pchan->pose_mat, pchan->chan_mat);
    }

#ifdef USE_NONUNIFORM_SCALE
    /* apply IK mat, but as if the bones have uniform scale since the IK solver
     * is not aware of non-uniform scale */
    float scale[3];
    mat4_to_size(scale, pchan->pose_mat);
    normalize_v3_length(pchan->pose_mat[0], scale[1]);
    normalize_v3_length(pchan->pose_mat[2], scale[1]);
#endif

    mul_m4_m4m4(pchan->pose_mat, pchan->pose_mat, ikmat);

#ifdef USE_NONUNIFORM_SCALE
    float ik_scale[3];
    mat3_to_size(ik_scale, ik_mat);
    normalize_v3_length(pchan->pose_mat[0], scale[0] * ik_scale[0]);
    normalize_v3_length(pchan->pose_mat[2], scale[2] * ik_scale[2]);
#endif

    /* calculate head */
    copy_v3_v3(pchan->pose_head, pchan->pose_mat[3]);
    /* calculate tail */
    copy_v3_v3(vec, pchan->pose_mat[1]);
    mul_v3_fl(vec, pchan->bone->length);
    add_v3_v3v3(pchan->pose_tail, pchan->pose_head, vec);

    pchan->flag |= POSE_DONE;
  }

  // GG: See make_dmats() for why this is commented out.
  // for (int a = 0; a < tree->totchannel; a++) {
  //   bPoseChannel *pchan = tree->pchan[a];
  //   LISTBASE_FOREACH (Bone *, child_bone, &pchan->bone->childbase) {
  //     bPoseChannel *child_chan = BKE_pose_channel_find_name(pose, child_bone->name);
  //     if (child_chan == NULL) {
  //       continue;
  //     }
  //     if (BLI_gset_haskey(tree->explicit_pchans, child_chan)) {
  //       continue;
  //     }
  //     if (BLI_gset_haskey(tree->implicit_pchans, child_chan)) {
  //       continue;
  //     }
  //     mul_m4_m4m4(child_chan->pose_mat, pchan->pose_mat, child_chan->chan_mat);
  //   }
  // }
}

/**
 * Called from within the core #BKE_pose_where_is loop, all animation-systems and constraints
 * were executed & assigned. Now as last we do an IK pass.
 */
static void execute_posetree(struct Depsgraph *depsgraph,
                             struct Scene *scene,
                             Object *ob,
                             PoseTree *tree)
{
  /* make matrix names... sane. and rename them in DNA too maybe or atleast add comment.  */
  /** TODO: fix limits rendering for animspace rot and maybe add limits rendering for location
   * limits */
  float identity[3][3];
  float *ikstretch = NULL;
  float resultinf = 0.0f;
  int a, flag, hasstretch = 0, resultblend = 0;
  IK_Segment **iktree, *iktarget;
  IK_Solver *solver;
  bKinematicConstraint *data;

  if (tree->totchannel == 0) {
    return;
  }
  const bool is_op_transforming_bone = (ob->pose->flag1 & POSE1_IS_TRANSFORMING_PCHAN) != 0;

  iktree = MEM_mallocN(sizeof(void *) * tree->totchannel, "ik tree");
  ListBase root_segments = {NULL, NULL};

  bPoseChannel *rootchan = tree->pchan[0];
  float solver_root_from_world[4][4];
  float pose_from_solver_root[4][4];
  float solver_root_from_pose[4][4];
  unit_m4(solver_root_from_world);
  unit_m4(pose_from_solver_root);
  unit_m4(solver_root_from_pose);
  {
    /* first set the goal inverse transform, assuming the root of tree was done ok! */
    if (rootchan->parent) {
      /* transform goal by parent mat, so this rotation is not part of the
       * segment's basis. otherwise rotation limits do not work on the
       * local transform of the segment itself. */
      copy_m4_m4(pose_from_solver_root, rootchan->parent->pose_mat);
      /* However, we do not want to get (i.e. reverse) parent's scale,
       * as it generates #31008 kind of nasty bugs. */
      normalize_m4(pose_from_solver_root);
    }
    else {
      unit_m4(pose_from_solver_root);
    }

    copy_v3_v3(pose_from_solver_root[3], rootchan->pose_head);
    const bool root_use_restpose_location = (rootchan->ikflag_location & BONE_IK_DOF_SPACE_REST) !=
                                            0;
    if (root_use_restpose_location) {
      /* NOTE: For targetless IK created from AutoIK, this will reset a selected bone to its
      restpose origin location which appears buggy.. but technically makes sense..
      TODO: GG: consider temporarily forcing use_restpose_location=False?, set by autoIK trnasform
      code? */
      copy_v3_v3(pose_from_solver_root[3], rootchan->bone->arm_head);
    }
    invert_m4_m4(solver_root_from_pose, pose_from_solver_root);

    mul_m4_m4m4(solver_root_from_world, ob->object_to_world, pose_from_solver_root);
    invert_m4(solver_root_from_world);
  }

  const bool solver_root_uses_restpose_location = (rootchan->ikflag_location &
                                                   BONE_IK_DOF_SPACE_REST) != 0;
  for (a = 0; a < tree->totchannel; a++) {
    bPoseChannel *curchan = tree->pchan[a];
    Bone *curbone = curchan->bone;
    bPoseChannel *parchan = curchan->parent;

    /* set DoF flag */
    flag = 0;
    if (!(curchan->ikflag & BONE_IK_NO_XDOF) && !(curchan->ikflag & BONE_IK_NO_XDOF_TEMP)) {
      flag |= IK_XDOF;
    }
    if (!(curchan->ikflag & BONE_IK_NO_YDOF) && !(curchan->ikflag & BONE_IK_NO_YDOF_TEMP)) {
      flag |= IK_YDOF;
    }
    if (!(curchan->ikflag & BONE_IK_NO_ZDOF) && !(curchan->ikflag & BONE_IK_NO_ZDOF_TEMP)) {
      flag |= IK_ZDOF;
    }

    const bool is_ext_nonzero = tree->stretch && (curchan->ikstretch > 0.0f);
    if (is_ext_nonzero && !(curchan->ikflag_stretch & BONE_IK_NO_YDOF) &&
        !(curchan->ikflag_stretch & BONE_IK_NO_YDOF_TEMP))
    {
      flag |= IK_EXTENSION_YDOF;
    }

    if ((curbone->flag & BONE_CONNECTED) == 0) {
      if (!(curchan->ikflag_location & BONE_IK_NO_XDOF) &&
          !(curchan->ikflag_location & BONE_IK_NO_XDOF_TEMP))
      {
        flag |= IK_TRANS_XDOF;
      }
      if (!(curchan->ikflag_location & BONE_IK_NO_YDOF) &&
          !(curchan->ikflag_location & BONE_IK_NO_YDOF_TEMP))
      {
        flag |= IK_TRANS_YDOF;
      }
      if (!(curchan->ikflag_location & BONE_IK_NO_ZDOF) &&
          !(curchan->ikflag_location & BONE_IK_NO_ZDOF_TEMP))
      {
        flag |= IK_TRANS_ZDOF;
      }
    }

    const bool use_animpose_loc = (curchan->ikflag_location & BONE_IK_DOF_SPACE_REST) == 0;
    const bool use_animpose_rot = (curchan->ikflag & BONE_IK_DOF_SPACE_REST) == 0;
    const bool use_animpose_ext = (curchan->ikflag_stretch & BONE_IK_DOF_SPACE_REST) == 0;

    const bool is_overriding_animspace_limits = !ELEM(curchan->ik_animspace_override_type,
                                                      IK_ANIMSPACE_OVERRIDE_TYPE_NO_OVERRIDE);
    const bool allow_loc_limits = !is_overriding_animspace_limits || !use_animpose_loc;
    const bool allow_rot_limits = !is_overriding_animspace_limits || !use_animpose_rot;
    const bool allow_ext_limits = !is_overriding_animspace_limits || !use_animpose_ext;

    const bool do_free_animspace_limits = is_op_transforming_bone &&
                                          ELEM(curchan->ik_animspace_override_type,
                                               IK_ANIMSPACE_OVERRIDE_TYPE_DO_FREE);
    const bool do_restrict_animspace_limits =
        is_op_transforming_bone &&
        (ELEM(curchan->ik_animspace_override_type, IK_ANIMSPACE_OVERRIDE_TYPE_DO_RESTRICT_FULL) ||
         (ELEM(curchan->ik_animspace_override_type,
               IK_ANIMSPACE_OVERRIDE_TYPE_DO_RESTRICT_PARTIAL) &&
          ((curbone->flag & BONE_SELECTED) == 0)));
    if (do_free_animspace_limits) {
      if (use_animpose_loc)
        flag |= (IK_TRANS_XDOF | IK_TRANS_YDOF | IK_TRANS_ZDOF);
      if (use_animpose_rot)
        flag |= (IK_XDOF | IK_YDOF | IK_ZDOF);
      if (use_animpose_ext && is_ext_nonzero)
        flag |= (IK_EXTENSION_YDOF);
    }
    else if (do_restrict_animspace_limits) {
      if (use_animpose_loc)
        flag &= ~(IK_TRANS_XDOF | IK_TRANS_YDOF | IK_TRANS_ZDOF);
      if (use_animpose_rot)
        flag &= ~(IK_XDOF | IK_YDOF | IK_ZDOF);
      if (use_animpose_ext && is_ext_nonzero)
        flag &= ~(IK_EXTENSION_YDOF);
    }

    /* Implicit chans evaluate as locked IK_QSegments. */
    if (BLI_gset_haskey(tree->implicit_pchans, curchan)) {
      /**  NOTE: If we ever do add support for unlocking the implicit bones, then AutoIK's
       * (transform_convert_armature.c)apply_targetless_ik() func will have to visual key those
       * bones too. Otherwise, the pose will snap out of place after confirming an AutoIK
       * transform. */
      flag = 0;
    }

    if (flag & IK_EXTENSION_YDOF) {
      hasstretch = 1;
    }
    /** TODO: For CONSTRIANT_IK_TIP_HEAD_AS_EE_POS, there is issue that stretch won't saitsfy it?
     * but thats OK since its the head that we're trying to satrisfy.. then why is it breaking?
     */

    IK_Segment *seg = iktree[a] = IK_CreateSegment(flag, curchan->name);

    const bool is_posetree_root = tree->parent[a] == -1;
    {
      if (is_posetree_root) {
        BLI_addtail(&root_segments, BLI_genericNodeN(seg));
      }
      else {
        IK_Segment *parent_seg = iktree[tree->parent[a]];
        IK_SetParent(seg, parent_seg);
      }
    }

    /* Translation segment */
    // const bool has_translation = ((flag & (IK_TRANS_XDOF | IK_TRANS_YDOF | IK_TRANS_ZDOF)) !=
    // 0);
    {
      float pose_from_tseg_rot[3][3];
      {
        /* The translation segment of a root without a parent uses pose space identity.
         * (GG: TODO: is this note accurate?) The translation segment of a root with a parent
         * copies its parent's pose orientation. */
        if (parchan != NULL) {
          copy_m3_m4(pose_from_tseg_rot, parchan->pose_mat);
          /* The iksolver doesn't support scale, so calculations shouldn't rely on parent scale. */
          normalize_m3(pose_from_tseg_rot);
        }
        else {
          unit_m3(pose_from_tseg_rot);
        }
      }

      float parent_tail_from_pose[4][4];
      {
        const bool is_evaluated_relative_to_solver_root = is_posetree_root;
        if (is_evaluated_relative_to_solver_root) {
          /* The owner chain root is solved in its parent's pose rotation space (with chain root's
          pose position), so the target root must also be made relative. Otherwise, moving the
          owner chain root has the effect of target chain not solving (since its goal hasn't
          moved). */
          copy_m4_m4(parent_tail_from_pose, solver_root_from_pose);
        }
        else /* is_evaluated_relative_to_parchan */ {
          BLI_assert(parchan);
          copy_m4_m4(parent_tail_from_pose, parchan->pose_mat);
          copy_v3_v3(parent_tail_from_pose[3], parchan->pose_tail);
          /* The iksolver doesn't support scale, so calculations shouldn't rely on parent scale. */
          normalize_m4(parent_tail_from_pose);
          invert_m4(parent_tail_from_pose);
        }
      }

      float parent_from_tseg_rot[3][3];
      {
        float m3_parent_from_pose[3][3];
        copy_m3_m4(m3_parent_from_pose, parent_tail_from_pose);
        mul_m3_m3m3(parent_from_tseg_rot, m3_parent_from_pose, pose_from_tseg_rot);
      }
      if (use_animpose_loc) {
        /* Translation segment always placed at pchan's head.*/
        float tseg_origin_parentspace[3];
        mul_v3_m4v3(tseg_origin_parentspace, parent_tail_from_pose, curchan->pose_head);

        float zero[3] = {0, 0, 0};
        /* No need to apply limits since they're relative to the animated location. */
        IK_SetTransform_TranslationSegment(
            seg, tseg_origin_parentspace, parent_from_tseg_rot, zero, zero);
      }
      else { /* use_restpose_location */

        float tseg_origin_parentspace[3];
        if (parchan != NULL) {
          if (!is_posetree_root) {
            /* Calculate tseg_origin_parentspace as the restspace location offset of curchan's
             * head from parent's tail (the parent's extension segment position). This will be used
             * as the location basis so that limits are applied relative to its rest location. */
            float parent_from_arm[4][4];
            copy_m4_m4(parent_from_arm, parchan->bone->arm_mat);
            copy_v3_v3(parent_from_arm[3], parchan->bone->arm_tail);
            invert_m4(parent_from_arm);

            mul_v3_m4v3(tseg_origin_parentspace, parent_from_arm, curbone->arm_head);
          }
          else {
            /* Since root parchan's aren't necessarily associated with the parent matrices
             * calculated, we have to account for parchan's animation explicitly. Otherwise
             * parented roots will be translated out of place. */
            float parent_from_arm[4][4];
            copy_m4_m4(parent_from_arm, parchan->bone->arm_mat);
            // copy_v3_v3(parent_from_arm[3], parchan->bone->arm_head);
            invert_m4(parent_from_arm);

            mul_v3_m4v3(tseg_origin_parentspace, parent_from_arm, curbone->arm_head);
            /* We use parent scale here for correct pose-space position result. */
            mul_m4_v3(parchan->pose_mat, tseg_origin_parentspace);
            mul_m4_v3(solver_root_from_pose, tseg_origin_parentspace);
          }
        }
        else {
          /* IK solver uses root bone's pose space head location as origin so its restpose tail
           * should be relative to its own head, not its parent's head.
           *
           * pchan[0] is the tip's rootbone which is the translational origin of all roots.
           */
          mul_v3_m4v3(tseg_origin_parentspace, solver_root_from_pose, curbone->arm_head);
        }

        /* Translation segment always placed at pchan's head.*/
        float tseg_position_parentspace[3];
        mul_v3_m4v3(tseg_position_parentspace, parent_tail_from_pose, curchan->pose_head);

        float tseg_initial[3];
        {
          sub_v3_v3v3(tseg_initial, tseg_position_parentspace, tseg_origin_parentspace);

          float tseg_rot_from_parent[3][3];
          invert_m3_m3(tseg_rot_from_parent, parent_from_tseg_rot);
          mul_m3_v3(tseg_rot_from_parent, tseg_initial);
        }

        float tseg_clamped[3];
        copy_v3_v3(tseg_clamped, tseg_initial);

        /* Apply the limits to the location basis in case no limits are active. */
        if (curchan->ikflag_location & (BONE_IK_NO_XDOF | BONE_IK_NO_XDOF_TEMP)) {
          tseg_clamped[0] = 0;
        }
        if (curchan->ikflag_location & (BONE_IK_NO_YDOF | BONE_IK_NO_YDOF_TEMP)) {
          tseg_clamped[1] = 0;
        }
        if (curchan->ikflag_location & (BONE_IK_NO_ZDOF | BONE_IK_NO_ZDOF_TEMP)) {
          tseg_clamped[2] = 0;
        }

        IK_SetTransform_TranslationSegment(
            seg, tseg_origin_parentspace, parent_from_tseg_rot, tseg_initial, tseg_clamped);
      }
    }

    /* Rotation Segment */
    const bool has_rotation = ((flag & (IK_XDOF | IK_YDOF | IK_ZDOF)) != 0);
    // if (has_rotation)
    {
      float tseg_rot_from_pose[3][3];
      float tseg_rot_at_rest_from_pose[3][3];
      /* The translation segment of a root without a parent uses pose space identity.
       * The translation segment of a root with a parent copies its parent's pose orientation. */
      if (parchan != NULL) {
        copy_m3_m4(tseg_rot_from_pose, parchan->pose_mat);
        normalize_m3(tseg_rot_from_pose);
        transpose_m3(tseg_rot_from_pose);

        copy_m3_m4(tseg_rot_at_rest_from_pose, parchan->bone->arm_mat);
        normalize_m3(tseg_rot_at_rest_from_pose);
        transpose_m3(tseg_rot_at_rest_from_pose);
      }
      else {
        unit_m3(tseg_rot_from_pose);
        unit_m3(tseg_rot_at_rest_from_pose);
      }
      // const bool use_animated_as_rest_rotation = !use_animpose_rot; // UNUSED
      if (use_animpose_rot) {

        float tseg_from_rseg[3][3];
        {
          float pose_from_curchan[3][3];
          copy_m3_m4(pose_from_curchan, curchan->pose_mat);
          /* basis must be pure rotation */
          normalize_m3(pose_from_curchan);

          mul_m3_m3m3(tseg_from_rseg, tseg_rot_from_pose, pose_from_curchan);
        }

        float identity_m3[3][3];
        unit_m3(identity_m3);

        /* No need to apply restpose limits on basis for individually locked components since this
         * call does so, see SetBasis() for the segment. */
        IK_SetTransform_RotationSegment(seg, tseg_from_rseg, identity_m3, identity_m3);
      }
      else {

        float tseg_from_curchan_at_rest[3][3];
        {
          float pose_from_curchan_at_rest[3][3];
          copy_m3_m4(pose_from_curchan_at_rest, curbone->arm_mat);
          normalize_m3(pose_from_curchan_at_rest);

          mul_m3_m3m3(
              tseg_from_curchan_at_rest, tseg_rot_at_rest_from_pose, pose_from_curchan_at_rest);
        }

        float curchan_at_rest_from_animated[3][3];
        {
          float pose_from_animated_curchan[3][3];
          copy_m3_m4(pose_from_animated_curchan, curchan->pose_mat);
          /* basis must be pure rotation */
          normalize_m3(pose_from_animated_curchan);

          /* Calculate curchan_at_rest_from_animated using tseg_rot_from_pose as intermediate space
           * so that tseg (parent) animation is accounted for. */
          float tseg_from_animated_curchan[3][3];
          mul_m3_m3m3(tseg_from_animated_curchan, tseg_rot_from_pose, pose_from_animated_curchan);

          float curchan_at_rest_from_tseg[3][3];
          transpose_m3_m3(curchan_at_rest_from_tseg, tseg_from_curchan_at_rest);

          mul_m3_m3m3(curchan_at_rest_from_animated,
                      curchan_at_rest_from_tseg,
                      tseg_from_animated_curchan);
        }

        /* Note: No need to apply restpose limits on basis for individually locked components since
         * this call does so, see SetBasis() for the segment. */
        if (has_rotation) {
          IK_SetTransform_RotationSegment(seg,
                                          tseg_from_curchan_at_rest,
                                          curchan_at_rest_from_animated,
                                          curchan_at_rest_from_animated);
        }
        else {
          float identity_m3[3][3];
          unit_m3(identity_m3);
          /* Completely locked rotation segments are reset to rest basis. */
          IK_SetTransform_RotationSegment(
              seg, tseg_from_curchan_at_rest, curchan_at_rest_from_animated, identity_m3);
        }
      }
    }

    /* Extension Segment*/
    // if (flag & IK_EXTENSION_YDOF)
    {
      float basis_length = curbone->length;
      float pose_extension = basis_length * len_v3(curchan->pose_mat[1]);
      float initial_extension = pose_extension;

      const bool has_extension = (flag & (IK_EXTENSION_Y)) != 0;
      if (!use_animpose_ext && !has_extension) {
        // Explicitly apply restpose default limits since limits are not active.
        pose_extension = basis_length * 1.0f;
      }
      /** GG: CLEANUP: Other segments are always created, can reduce this to just plain length. */
      IK_SetTransform_ExtensionSegment(seg, initial_extension, pose_extension);
    }

    if (allow_rot_limits) {
      if (((curchan->ikflag & BONE_IK_XLIMIT) != 0)) {
        IK_SetLimit(seg, IK_X, curchan->limitmin[0], curchan->limitmax[0]);
      }
      if (curchan->ikflag & BONE_IK_YLIMIT) {
        IK_SetLimit(seg, IK_Y, curchan->limitmin[1], curchan->limitmax[1]);
      }
      if (curchan->ikflag & BONE_IK_ZLIMIT) {
        IK_SetLimit(seg, IK_Z, curchan->limitmin[2], curchan->limitmax[2]);
      }
    }

    if (allow_loc_limits) {
      if (curchan->ikflag_location & BONE_IK_XLIMIT) {
        IK_SetLimit(seg, IK_TRANS_X, curchan->limitmin_location[0], curchan->limitmax_location[0]);
      }
      if (curchan->ikflag_location & BONE_IK_YLIMIT) {
        IK_SetLimit(seg, IK_TRANS_Y, curchan->limitmin_location[1], curchan->limitmax_location[1]);
      }
      if (curchan->ikflag_location & BONE_IK_ZLIMIT) {
        IK_SetLimit(seg, IK_TRANS_Z, curchan->limitmin_location[2], curchan->limitmax_location[2]);
      }
    }

    IK_SetStiffness(seg, IK_X, curchan->stiffness[0]);
    IK_SetStiffness(seg, IK_Y, curchan->stiffness[1]);
    IK_SetStiffness(seg, IK_Z, curchan->stiffness[2]);
    IK_SetStiffness(seg, IK_TRANS_X, curchan->stiffness_location[0]);
    IK_SetStiffness(seg, IK_TRANS_Y, curchan->stiffness_location[1]);
    IK_SetStiffness(seg, IK_TRANS_Z, curchan->stiffness_location[2]);

    // GG: TODO: place comment in more relevant area.
    /* Non-Extension composites are always placed such that their end effector is at curchan's
     * head. The extension segmetn is always placed such that its end effector is at curchan's
     * tail. This allows CONSTRAINT_IK_TIP_HEAD_AS_EE_POS to work as expected, where we must have a
     * segment's whose end effector is at the tip curchan's head. */
    if (allow_ext_limits) {
      if (curchan->ikflag_stretch & BONE_IK_YLIMIT) {
        float limit_factor = 1;
        if (use_animpose_ext) {
          limit_factor = len_v3(curchan->pose_mat[1]);
        }
        const float min = curbone->length * curchan->limitmin_stretch * limit_factor;
        const float max = curbone->length * curchan->limitmax_stretch * limit_factor;
        IK_SetLimit(seg, IK_EXTENSION_Y, min, max);
      }
    }

    if (is_ext_nonzero) {
      const float ikstretch_sq = square_f(curchan->ikstretch);
      /* this function does its own clamping */
      IK_SetStiffness(seg, IK_EXTENSION_Y, 1.0f - ikstretch_sq);
    }
  }

  IK_Segment **roots = NULL;
  const int root_count = BLI_listbase_count(&root_segments);
  {
    roots = MEM_callocN(sizeof(IK_Segment *) * root_count, __func__);
    int i = 0;
    LISTBASE_FOREACH_INDEX (LinkData *, ld_segment, &root_segments, i) {
      roots[i] = ld_segment->data;
    }
    BLI_freelistN(&root_segments);

    solver = IK_CreateSolver(roots, root_count);

    // float m3_pose_from_solver_root[3][3];
    // copy_m3_m4(m3_pose_from_solver_root, pose_from_solver_root);
    // IK_DEBUG_print_matrices(roots, root_count, m3_pose_from_solver_root,
    // pose_from_solver_root[3]);
  }

  /* Add goals based on IK constraint data. */
  for (PoseTarget *target = tree->targets.first; target; target = target->next) {
    float polepos[3];
    int poleconstrain = 0;

    data = (bKinematicConstraint *)target->con->data;
    BLI_assert_msg(data->tar != NULL, "Targetless IK not expected to be added to tree->targets");
    /* 1.0=ctime, we pass on object for auto-ik (owner-type here is object, even though
     * strictly speaking, it is a posechannel)
     */
    /**
     * GG: Q: XXX: Is there a use for this complicated func call? Does IK con allow non-trivial
     * target matrix?
     * - I suppose so if the target is not a bone?... which i'll have to acount for in
     *   initialization... and deps graph (or just assume target always a bone when target
     *   chain_count!=1...) */
    /** GG: TODO: For a target chain, then it (and the owner chain) don't need target->con to
     * calculate the goalpos/rot. It's still needed for the poletarget but only for the owner
     * (ofc the target technically should also support a pole).
     *
     * GG: TODO: to properly support
     * poletargets with branched and extended IK chains, then we need to determine which constraint
     * poletarget to use (really, use should just set the same one for all or for atleast one).
     * Currently, the last one wins, which is fine atm.
     */
    float world_from_target[4][4];
    unit_m4(world_from_target);

    BKE_constraint_target_matrix_get(
        depsgraph, scene, target->con, 0, CONSTRAINT_OBTYPE_OBJECT, ob, world_from_target, 1.0);

    // printf_s("goal_pos_world: %.2f %.2f %.2f\n",
    //         world_from_target[3][0],
    //         world_from_target[3][1],
    //         world_from_target[3][2]);
    /* and set and transform goal */
    float solver_root_from_target[4][4];
    mul_m4_m4m4(solver_root_from_target, solver_root_from_world, world_from_target);

    float goalrot[3][3], goalpos[3];
    copy_v3_v3(goalpos, solver_root_from_target[3]);
    copy_m3_m4(goalrot, solver_root_from_target);
    normalize_m3(goalrot);

    /* same for pole vector target */
    if (data->poletar) {
      float world_from_poletarget[4][4];
      unit_m4(world_from_poletarget);
      BKE_constraint_target_matrix_get(depsgraph,
                                       scene,
                                       target->con,
                                       1,
                                       CONSTRAINT_OBTYPE_OBJECT,
                                       ob,
                                       world_from_poletarget,
                                       1.0);

      float solver_root_from_poletarget[4][4];
      unit_m4(solver_root_from_poletarget);
      mul_m4_m4m4(solver_root_from_poletarget, solver_root_from_world, world_from_poletarget);
      copy_v3_v3(polepos, solver_root_from_poletarget[3]);
      poleconstrain = 1;

      /* for pole targets, we blend the result of the ik solver
       * instead of the target position, otherwise we can't get
       * a smooth transition */
      resultblend = 1;
      resultinf = target->con->enforce;
    }

    /* do we need blending? */
    /** GG: TODO: ik_poseability branch has yet to consider properly supporting
     * non-One-or-Zero influence.
     *
     * GG: DESIGN: ... Also blending to identity basis still has the effect of a
     *    virtual IK, even when influence==0 or very small. For two oneway IK
     *    chains with a shared hierarchy and one chain having very small
     *    influence, the low influence chain will still fully influence the
     *    solution of the shared hierarchy. Thus, for the sahrd hierarhcy,
     *    there's a differnece between zero influence and a disabled constraint.
     *    In 4.0.2, there is a problem where even the non-shared hierarchy and
     *    tip appears to be IK constrained completely, even at low influence.
     *    Pre 4.0.2 just plain doesn't work in any useful way either. So the
     *    proper way to handle non-integer influence needs to be figured out. It
     *    should be intuitive to the user and be invertible so poses can be
     *    preserved when enabling the non-integer influence IK con.
     *
     *    potential DESIGN: solve IK without the target, as if disabled. Solve
     *    fully enabled. then blend teh results. This results in 2^n independent
     *    solves, where n= total IK cons that have non-integer influence. It's
     *    exponential due to the recursive requirement of having to do a solve
     *    for every combination of enabled/disabled IK cons. This also has the
     *    problem of being difficult to invert, at least at the moment, without
     *    having to use an IK solver too (though that's not a deal breaker, just
     *    seems non-trivial and inefficient)
     */
    if (!resultblend && target->con->enforce != 1.0f) {
      float q1[4], q2[4], q[4];
      float fac = target->con->enforce;
      float mfac = 1.0f - fac;

      bPoseChannel *tipchan = tree->pchan[target->tip];

      /* end effector in world space */
      float end_pose[4][4], world_pose[4][4];
      copy_m4_m4(end_pose, tipchan->pose_mat);
      copy_v3_v3(end_pose[3], tipchan->pose_tail);
      mul_m4_series(world_pose, solver_root_from_world, ob->object_to_world, end_pose);

      /* blend position */
      goalpos[0] = fac * goalpos[0] + mfac * world_pose[3][0];
      goalpos[1] = fac * goalpos[1] + mfac * world_pose[3][1];
      goalpos[2] = fac * goalpos[2] + mfac * world_pose[3][2];

      /* blend rotation */
      mat3_to_quat(q1, goalrot);
      mat4_to_quat(q2, world_pose);
      interp_qt_qtqt(q, q1, q2, mfac);
      quat_to_mat3(goalrot, q);
    }

    // GG: Cleanup: rename iktarget to tip_seg
    iktarget = iktree[target->tip];

    IK_Segment *goalseg = NULL;
    if (target->target != -1) {
      goalseg = iktree[target->target];
    }
    IK_Segment *zero_weight_sentinel = NULL;
    /* XXX: zero weight sentinel does work but having it off seems to result in a more useful
     * result for looped chains? For loops and sentinel=OFF, then the root can translate,
     * effectively removing any apparent visually pinned root during interpolation. A workaround to
     * enforce sentinel=ON's effect from the user level is to just use shorter chain lengths such
     * that the twoway chains dont share roots. */
    const bool do_support_zero_weight_sentinel = false;
    if (do_support_zero_weight_sentinel && target->zero_weight_sentinel_index != -1) {
      BLI_assert(target->zero_weight_sentinel_index >= 0);
      BLI_assert(target->zero_weight_sentinel_index < tree->totchannel);
      zero_weight_sentinel = iktree[target->zero_weight_sentinel_index];
    }
    if ((data->flag & CONSTRAINT_IK_POS) && data->weight != 0.0f) {
      const bool tip_use_tail_as_ee_pos = ((data->flag & CONSTRAINT_IK_TIP_HEAD_AS_EE_POS) == 0);
      const bool tip_use_goal_tip = true;

      if (poleconstrain) {

        int root_tree_index = target->tip;
        for (int cur_index = target->tip; cur_index != -1; cur_index = tree->parent[cur_index])
          root_tree_index = cur_index;

        BLI_assert(root_tree_index >= 0);

        int root_index_in_solver = -1;
        IK_Segment *root_segment = iktree[root_tree_index];
        for (int r = 0; r < root_count; r++) {
          if (root_segment != roots[r]) {
            continue;
          }
          root_index_in_solver = r;
          break;
        }
        BLI_assert(root_index_in_solver >= 0);

        IK_SolverAddPoleVectorConstraint(solver,
                                         root_index_in_solver,
                                         iktarget,
                                         tip_use_tail_as_ee_pos,
                                         goalpos,
                                         polepos,
                                         data->poleangle,
                                         goalseg,
                                         false);
      }

      // printf_s("goal_pos: %.2f %.2f %.2f\n", goalpos[0], goalpos[1], goalpos[2]);

      IK_SolverAddGoal(solver,
                       iktarget,
                       goalpos,
                       data->weight,
                       tip_use_tail_as_ee_pos,
                       goalseg,
                       !tip_use_goal_tip,
                       zero_weight_sentinel);

      if (data->flag & CONSTRAINT_IK_IS_TWOWAY) {
        BLI_assert(goalseg);
        const bool target_use_tail_as_ee_pos = false;
        /* When owner is using tail-end of pchan_tip to goal to target's head, then target chain
         * should ensure its head is goaling to the owner's tail end too. */
        const bool target_use_goal_tip = tip_use_tail_as_ee_pos;

        IK_SolverAddGoal(solver,
                         goalseg,
                         goalpos, /* UNUSED */
                         data->weight,
                         target_use_tail_as_ee_pos,
                         iktarget,
                         target_use_goal_tip,
                         zero_weight_sentinel);
      }
    }
    if ((data->flag & CONSTRAINT_IK_ROT) && (data->orientweight != 0.0f)) {
      IK_SolverAddGoalOrientation(
          solver, iktarget, goalrot, data->orientweight, goalseg, zero_weight_sentinel);

      if (data->flag & CONSTRAINT_IK_IS_TWOWAY) {
        BLI_assert(goalseg);
        IK_SolverAddGoalOrientation(solver,
                                    goalseg,
                                    goalrot, /* UNUSED */
                                    data->orientweight,
                                    iktarget,
                                    zero_weight_sentinel);
      }
    }
  }

  /* Add goals to keep AutoIK pinned bones in place. */
  for (a = 0; a < tree->totchannel; a++) {
    bPoseChannel *pchan_tip = tree->pchan[a];
    bConstraint *con;
    bKinematicConstraint *data;

    for (con = pchan_tip->constraints.first; con; con = con->next) {
      if (con->type != CONSTRAINT_TYPE_KINEMATIC) {
        continue;
      }
      data = (bKinematicConstraint *)con->data;

      if (con->flag & CONSTRAINT_DISABLE) {
        continue;
      }
      if (con->flag & CONSTRAINT_OFF) {
        continue;
      }
      if (IS_EQF(con->enforce, 0.0f)) {
        continue;
      }
      if ((data->autoik_flag & CONSTRAINT_AUTOIK_ENABLED) == 0) {
        continue;
      }

      // GG: Q: It's more user friendly to allow pinned bones to also add goals for pinned parents
      // when possible, even when their roots differ? So leave commented out?
      // const bool creates_posetree = (data->autoik_flag & CONSTRAINT_IK_DO_NOT_CREATE_POSETREE)
      // == 0; if(creates_posetree){
      //   continue;
      // }

      break;
    }
    if (con == NULL) {
      continue;
    }

    const bool pin_head = (data->autoik_flag & CONSTRAINT_AUTOIK_USE_HEAD) != 0;
    const bool pin_tail = (data->autoik_flag & CONSTRAINT_AUTOIK_USE_TAIL) != 0;
    const bool pin_rotation = (data->autoik_flag & CONSTRAINT_AUTOIK_USE_ROTATION) != 0;
    const IK_Segment *null_sentinel_segment = NULL;

    IK_Segment *segment_end_effector = iktree[a];

    if (pin_head) {
      const bool use_tail_as_ee_pos = false;

      float goalpos[3];
      {
        copy_v3_v3(goalpos, data->grabtarget);
        /** GG: TODO: If pchan is not part of same object as solver root, then we need to go
         * through world first. */
        mul_m4_v3(solver_root_from_pose, goalpos);
      }

      IK_SolverAddGoal(solver,
                       segment_end_effector,
                       goalpos,
                       data->autoik_weight_head,
                       use_tail_as_ee_pos,
                       NULL,
                       /*UNUSED*/
                       false,
                       null_sentinel_segment);
    }

    const bool is_pin_tail_redundant = pin_rotation && pin_head;
    if (pin_tail && !is_pin_tail_redundant) {
      /* We only create this goal if there is no rotation goal to avoid redundancy. */
      const bool use_tail_as_ee_pos = true;

      float goalpos[3];
      {
        copy_v3_v3(goalpos, data->autoik_target_tail);
        /** GG: TODO: If pchan is not part of same object as solver root, then we need to go
         * through world first. */
        mul_m4_v3(solver_root_from_pose, goalpos);
      }

      IK_SolverAddGoal(solver,
                       segment_end_effector,
                       goalpos,
                       data->autoik_weight_tail,
                       use_tail_as_ee_pos,
                       NULL,
                       /*UNUSED*/
                       false,
                       null_sentinel_segment);
    }

    if (pin_rotation) {

      float goalrot[3][3];
      {
        copy_m3_m3(goalrot, data->rotation_target);
        /** GG: TODO: If pchan is not part of same object as solver root, then we need to go
         * through world first. */
        mul_m3_m4m3(goalrot, solver_root_from_pose, goalrot);
      }
      IK_SolverAddGoalOrientation(solver,
                                  segment_end_effector,
                                  goalrot,
                                  data->autoik_weight_rotation,
                                  NULL,
                                  null_sentinel_segment);
    }
  }

  /* solve */
  IK_Solve(solver, 0.0f, tree->iterations);
  MEM_freeN(roots);
  roots = NULL;

  IK_FreeSolver(solver);

  /* gather basis changes */
  tree->basis_change = MEM_mallocN(sizeof(float[4][4]) * tree->totchannel, "ik basis change");
  tree->translation_change = MEM_mallocN(sizeof(float[3]) * tree->totchannel,
                                         "ik translation change");
  if (hasstretch) {
    ikstretch = MEM_mallocN(sizeof(float) * tree->totchannel, "ik stretch");
  }
  for (a = 0; a < tree->totchannel; a++) {
    IK_GetBasisChange(iktree[a], tree->basis_change[a]);
    bPoseChannel *pchan = tree->pchan[a];

    if (hasstretch) {
      /* have to compensate for scaling received from parent */
      float parentstretch, stretch;

      parentstretch = (tree->parent[a] >= 0) ? ikstretch[tree->parent[a]] : 1.0f;

      if (tree->stretch && (pchan->ikstretch > 0.0f)) {
        float stretch[3], length;

        IK_GetStretchChange(iktree[a], stretch);
        length = pchan->bone->length * len_v3(pchan->pose_mat[1]);

        ikstretch[a] = (length == 0.0f) ? 1.0f : (stretch[1] + length) / length;
      }
      else {
        ikstretch[a] = 1.0;
      }

      stretch = (parentstretch == 0.0f) ? 1.0f : ikstretch[a] / parentstretch;

      mul_v3_fl(tree->basis_change[a][0], stretch);
      mul_v3_fl(tree->basis_change[a][1], stretch);
      mul_v3_fl(tree->basis_change[a][2], stretch);
    }
    /** TODO: GG: I don't think stretch and root transltaion are mutually exclusive? Though I
     * also don't know how to tell teh difference based on ik sovler output data...
     * Maybe get min trnaslation diff of all bones and use that as root's stretch? then the
     * rest of the translation change is stored as translational change? That seems reasonable?
     */
    /** XXX: GG: if use_head_as_end_effector and tip has loc/rot (so its a composite segment),
     * then we're not obtaining the right transltaion so it break.
     */
    float trans[3];
    IK_GetTranslationChange(iktree[a], trans);
    // print_v3("trans_change: ", trans);

    /* Account for parent scale for translation offset, which are currently in posespace units.
     * NOTE: Seems to work fine as long as parent is uniformly scaled. */
    bPoseChannel *parchan = pchan->parent;
    if (parchan) {
      float parent_scale[3];
      mat4_to_size(parent_scale, parchan->pose_mat);
      invert_v3_safe(parent_scale);
      mul_v3_v3(trans, parent_scale);
    }
    tree->translation_change[a][0] = trans[0];
    tree->translation_change[a][1] = trans[1];
    tree->translation_change[a][2] = trans[2];

    if (resultblend && resultinf != 1.0f) {
      unit_m3(identity);
      blend_m3_m3m3(tree->basis_change[a], identity, tree->basis_change[a], resultinf);
    }

    IK_FreeSegment(iktree[a]);
  }

  MEM_freeN(iktree);
  if (ikstretch) {
    MEM_freeN(ikstretch);
  }
}

static void free_posetree(PoseTree *tree)
{
  BLI_freelistN(&tree->targets);
  if (tree->pchan) {
    MEM_freeN(tree->pchan);
  }
  if (tree->parent) {
    MEM_freeN(tree->parent);
  }
  if (tree->basis_change) {
    MEM_freeN(tree->basis_change);
  }
  if (tree->translation_change) {
    MEM_freeN(tree->translation_change);
  }
  if (tree->explicit_pchans) {
    BLI_gset_free(tree->explicit_pchans, NULL);
  }
  if (tree->implicit_pchans) {
    BLI_gset_free(tree->implicit_pchans, NULL);
  }

  MEM_freeN(tree);
}

/* ------------------------------
 * Plugin API for legacy iksolver */

void iksolver_initialize_tree(struct Depsgraph *UNUSED(depsgraph),
                              struct Scene *UNUSED(scene),
                              struct Object *ob,
                              float UNUSED(ctime))
{
  bPoseChannel *pchan;

  /** GG: ... if a pose tree is a result of all ik cons .. then why do we pass less data than it
   * generally needs...?? Should just pass all pchans since all cons needs to be considered
   * anyways. The current flow is to create a tree, then append nodes from other ik cons.. Would it
   * be simpler to just gather all the cons with same roots, then create a tree from that? - no
   * need to modify, which iirc is a bit convoluted/tedious to do atm.
   *
   * I suppose a complexity added is that we're going through all constraints of all bones, not
   * just the bones of the same posetree..
   *
   * .. I wonder if tree appending would be simpler if .. we just didn't store tree->parent  (index
   * of parent)? Why not just wrap the pchan pointers, segments, and IK change data, and have  a
   * pointer to parent? The solverse don't make use of the particular order.. So why do we?
   */

  /** TODO: GG: per posetree, need to  (post process) keep walking past owner chains to null root
   * in case non-chained hierarchy is part of same posetree..
   *
   * -can do as we append branches and extend since each posetree already has a ref of all its
   * roots.
   *
   * TODO: GG: .. do we properly setup parenting when a branched hcain is added- do we fix
   * the parenting of prior roots?  Currently, we only patch in direct parents that are appended to
   * the tree. However, after all the processing is done, we will still need to go through and add
   * the implicit parent pchans and segments. Segments that are strictly postprocess need to be
   * locked during eval. A property of implicit chains is that it eventually leads to an explicit
   * bone in the posetree. Knowing that, it means that implciit hcains don't have parent gaps.
   */
  GHash *solver_from_chain_root = BKE_determine_posetree_roots(&ob->pose->chanbase);
  /** GG: CLEANUP: Explicit + implicit = known total pchans that will be part of the posetree.
   * There's no need for initialize_posetree() to realloc the posetree per chain append.
   *
   * GG: CLEANUP: By pre-allocating the posetree, we also won't have to pass
   * implicit_pchans_from_posetree_pchan anymore.
   */
  GHash *explicit_pchans_from_posetree_pchan;
  GHash *implicit_pchans_from_posetree_pchan;
  BKE_determine_posetree_pchan_implicity(&ob->pose->chanbase,
                                         solver_from_chain_root,
                                         &explicit_pchans_from_posetree_pchan,
                                         &implicit_pchans_from_posetree_pchan);

  for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
    if (pchan->constflag & PCHAN_HAS_IK) { /* flag is set on editing constraints */
                                           /* will attach it to root! */
      initialize_posetree(ob,
                          pchan,
                          solver_from_chain_root,
                          explicit_pchans_from_posetree_pchan,
                          implicit_pchans_from_posetree_pchan);
    }
  }

  // /* GG: XXX: the posetree chan free explicit_.. doesn't have a potsetee...
  //  * GG: XXX: there's a memory leak or invalid memory write seomwhere. somehow obj->pose->pchans
  //  is
  //  * being overwritten..**/
  // /** GG: CLEANUP: I suppose these checks are why unit tests exist.. */
  // int total_solvers_e = BLI_ghash_len(explicit_pchans_from_posetree_pchan);
  // int total_solvers_i = BLI_ghash_len(implicit_pchans_from_posetree_pchan);
  // int total_roots = BLI_ghash_len(solver_from_chain_root);
  // GHashIterator gh_iter;
  // GSet *traversed_posetrees = BLI_gset_ptr_new(__func__);
  // GHASH_ITER (gh_iter, solver_from_chain_root) {
  //   bPoseChannel *posetree_chan = BLI_ghashIterator_getValue(&gh_iter);
  //   BLI_gset_add(traversed_posetrees, posetree_chan);
  // }
  // int total_posetrees = BLI_gset_len(traversed_posetrees);
  // BLI_assert(total_posetrees == total_solvers_i);
  // BLI_assert(total_posetrees == total_solvers_e);
  // GHASH_ITER (gh_iter, explicit_pchans_from_posetree_pchan) {
  //   bPoseChannel *posetree_chan = BLI_ghashIterator_getKey(&gh_iter);
  //   BLI_assert(posetree_chan);

  //   PoseTree *posetree = posetree_chan->iktree.first;
  //   BLI_assert(posetree);

  //   /* Ensure that pchans only occur once in the posetree. If not, then posetree chain appending
  //    * function is bugged. */
  //   GSet *traversed_pchans = BLI_gset_ptr_new(__func__);
  //   for (int i = 0; i < posetree->totchannel; i++) {
  //     bPoseChannel *pchan = posetree->pchan[i];
  //     if (BLI_gset_haskey(traversed_pchans, pchan)) {
  //       for (int k = 0; k < posetree->totchannel; k++) {
  //         bPoseChannel *tmp_pchan = posetree->pchan[k];
  //         printf("%s\n", tmp_pchan->name);
  //       }
  //     }
  //     BLI_assert_msg(
  //         !BLI_gset_haskey(traversed_pchans, pchan),
  //         "posetree_append_chanlist() is bugged! There shouldn't be any duplicate pchans!");
  //     BLI_gset_add(traversed_pchans, posetree->pchan[i]);
  //   }
  //   BLI_gset_clear(traversed_pchans, NULL);

  //   ListBase roots = {NULL, NULL};
  //   GSet *root_indices = BLI_gset_int_new(__func__);

  //   BLI_assert_msg(posetree->parent[0] == -1, ".. How is the first node not a root??");

  //   for (int i = 0; i < posetree->totchannel; i++) {
  //     if (posetree->parent[i] != -1) {
  //       continue;
  //     }

  //     bPoseChannel *rootchan = posetree->pchan[i];
  //     BLI_addtail(&roots, BLI_genericNodeN(rootchan));
  //     BLI_gset_add(root_indices, i);

  //     if (rootchan->parent == NULL) {
  //       continue;
  //     }
  //     const int expected_parent_index = posetree->parent[i];
  //     for (int parent_index = 0; parent_index < posetree->totchannel; parent_index++) {
  //       bPoseChannel *parchan = posetree->pchan[parent_index];
  //       if (parchan != rootchan->parent) {
  //         continue;
  //       }
  //       BLI_assert_msg(expected_parent_index == parent_index,
  //                      "Posetree chain append() function bugged");
  //     }
  //   }

  //   LISTBASE_FOREACH (PoseTarget *, target, &posetree->targets) {
  //     int tip_index = target->tip;
  //     int parent_index = posetree->parent[tip_index];

  //     int root_index = tip_index;
  //     int depth = 0;
  //     while (parent_index != -1) {
  //       root_index = parent_index;
  //       parent_index = posetree->parent[parent_index];
  //       BLI_assert_msg(depth < 100,
  //                      "potential infinite recursion! posetree_append_chanlist() bugged!");
  //       depth++;
  //     }
  //     BLI_assert_msg(root_index != -1, "Bug in test: root_index assignment");
  //     BLI_assert_msg(BLI_gset_haskey(root_indices, root_index),
  //                    "Tip doesn't lead to an expected root!");
  //   }

  //   BLI_freelistN(&roots);
  //   BLI_gset_free(traversed_pchans, NULL);
  //   traversed_pchans = NULL;
  //   BLI_gset_free(root_indices, NULL);
  //   root_indices = NULL;
  // }

  BLI_ghash_free(solver_from_chain_root, NULL, NULL);
  /* We don't use BLI_gset_free per entry since they're used by their posetrees now. */
  BLI_ghash_free(explicit_pchans_from_posetree_pchan, NULL, NULL);
  BLI_ghash_free(implicit_pchans_from_posetree_pchan, NULL, NULL);

  ob->pose->flag &= ~POSE_WAS_REBUILT;
}

void iksolver_execute_tree(struct Depsgraph *depsgraph,
                           struct Scene *scene,
                           Object *ob,
                           bPoseChannel *pchan_root,
                           float ctime)
{
  while (pchan_root->iktree.first) {
    PoseTree *tree = pchan_root->iktree.first;
    int a;

    /* stop on the first tree that isn't a standard IK chain */
    if (tree->type != CONSTRAINT_TYPE_KINEMATIC) {
      return;
    }

    /* 4. walk over the tree for regular solving */
    for (a = 0; a < tree->totchannel; a++) {
      if (!(tree->pchan[a]->flag & POSE_DONE)) { /* successive trees can set the flag */
        BKE_pose_where_is_bone(depsgraph, scene, ob, tree->pchan[a], ctime, 1);
      }
      /* Tell blender that this channel was controlled by IK,
       * it's cleared on each BKE_pose_where_is(). */
      tree->pchan[a]->flag |= POSE_CHAIN;
    }

    /* 5. execute the IK solver */
    execute_posetree(depsgraph, scene, ob, tree);

    /* 6. apply the differences to the channels,
     *    we need to calculate the original differences first */
    make_dmats(ob->pose, tree);

    /* sets POSE_DONE */
    where_is_ik_bones(ob->pose, tree);

    /* 7. and free */
    BLI_remlink(&pchan_root->iktree, tree);
    free_posetree(tree);
  }
}

void iksolver_release_tree(struct Scene *UNUSED(scene), struct Object *ob, float UNUSED(ctime))
{
  iksolver_clear_data(ob->pose);
}

void iksolver_clear_data(bPose *pose)
{
  LISTBASE_FOREACH (bPoseChannel *, pchan, &pose->chanbase) {
    if ((pchan->flag & POSE_IKTREE) == 0) {
      continue;
    }

    while (pchan->iktree.first) {
      PoseTree *tree = pchan->iktree.first;

      /* stop on the first tree that isn't a standard IK chain */
      if (tree->type != CONSTRAINT_TYPE_KINEMATIC) {
        break;
      }

      BLI_remlink(&pchan->iktree, tree);
      free_posetree(tree);
    }
  }
}
