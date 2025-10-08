/* SPDX-FileCopyrightText: 2006 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 * \brief BVH-tree implementation.
 *
 * k-DOP BVH (Discrete Oriented Polytope, Bounding Volume Hierarchy).
 * A k-DOP is represented as k/2 pairs of min, max values for k/2 directions (intervals, "slabs").
 *
 * See: http://www.gris.uni-tuebingen.de/people/staff/jmezger/papers/bvh.pdf
 *
 * implements a BVH-tree structure with support for:
 *
 * - Ray-cast:
 *   #BLI_bvhtree_ray_cast, #BVHRayCastData
 * - Nearest point on surface:
 *   #BLI_bvhtree_find_nearest, #BVHNearestData
 * - Overlapping 2 trees:
 *   #BLI_bvhtree_overlap, #BVHOverlapData_Shared, #BVHOverlapData_Thread
 * - Range Query:
 *   #BLI_bvhtree_range_query
 */

#include <algorithm>

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_heap_simple.h"
#include "BLI_kdopbvh.hh"
#include "BLI_math_geom.h"
#include "BLI_math_vector_types.hh"
#include "BLI_stack.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

/* used for iterative_raycast */
// #define USE_SKIP_LINKS

/* Use to print balanced output. */
// #define USE_PRINT_TREE

/* Check tree is valid. */
// #define USE_VERIFY_TREE

#define MAX_TREETYPE 32

#ifndef NDEBUG
/* Setting zero so we can catch bugs in BLI_task/KDOPBVH. */
#  define KDOPBVH_THREAD_LEAF_THRESHOLD 0
#else
#  define KDOPBVH_THREAD_LEAF_THRESHOLD 1024
#endif

/* -------------------------------------------------------------------- */
/** \name Struct Definitions
 * \{ */

using axis_t = uchar;

struct BVHNode {
  BVHNode **children;
  BVHNode *parent; /* some user defined traversed need that */
#ifdef USE_SKIP_LINKS
  BVHNode *skip[2];
#endif
  float *bv;      /* Bounding volume of all nodes, max 13 axis */
  int index;      /* face, edge, vertex index */
  char node_num;  /* how many nodes are used, used for speedup */
  char main_axis; /* Axis used to split this node */
};

/* keep under 26 bytes for speed purposes */
struct BVHTree {
  BVHNode **nodes;
  BVHNode *nodearray;  /* Pre-allocate branch nodes. */
  BVHNode **nodechild; /* Pre-allocate children for nodes. */
  float *nodebv;       /* Pre-allocate bounding-volumes for nodes. */
  float epsilon;       /* Epsilon is used for inflation of the K-DOP. */
  int leaf_num;        /* Leafs. */
  int branch_num;
  axis_t start_axis, stop_axis; /* bvhtree_kdop_axes array indices according to axis */
  axis_t axis;                  /* KDOP type (6 => OBB, 7 => AABB, ...) */
  char tree_type;               /* type of tree (4 => quad-tree). */
};

/* optimization, ensure we stay small */
BLI_STATIC_ASSERT((sizeof(void *) == 8 && sizeof(BVHTree) <= 48) ||
                      (sizeof(void *) == 4 && sizeof(BVHTree) <= 32),
                  "over sized")

/* avoid duplicating vars in BVHOverlapData_Thread */
struct BVHOverlapData_Shared {
  const BVHTree *tree1, *tree2;
  axis_t start_axis, stop_axis;
  bool use_self;

  /* use for callbacks */
  BVHTree_OverlapCallback callback;
  void *userdata;
};

struct BVHOverlapData_Thread {
  BVHOverlapData_Shared *shared;
  BLI_Stack *overlap; /* store BVHTreeOverlap */
  uint max_interactions;
  /* use for callbacks */
  int thread;
};

struct BVHNearestData {
  const BVHTree *tree;
  const float *co;
  BVHTree_NearestPointCallback callback;
  void *userdata;
  float proj[13]; /* coordinates projection over axis */
  BVHTreeNearest nearest;
};

struct BVHRayCastData {
  const BVHTree *tree;

  BVHTree_RayCastCallback callback;
  void *userdata;

  BVHTreeRay ray;

#ifdef USE_KDOPBVH_WATERTIGHT
  IsectRayPrecalc isect_precalc;
#endif

  /* initialized by bvhtree_ray_cast_data_precalc */
  float ray_dot_axis[13];
  float idot_axis[13];
  int index[6];

  BVHTreeRayHit hit;
};

struct BVHNearestProjectedData {
  DistProjectedAABBPrecalc precalc;
  bool closest_axis[3];
  BVHTree_NearestProjectedCallback callback;
  void *userdata;
  BVHTreeNearest nearest;

  int clip_plane_len;
  float clip_plane[0][4];
};

struct BVHIntersectPlaneData {
  const BVHTree *tree;
  float plane[4];
  BLI_Stack *intersect; /* Store indexes. */
};

/** \} */

/**
 * Bounding Volume Hierarchy Definition
 *
 * Notes: From OBB until 26-DOP --> all bounding volumes possible, just choose type below
 * Notes: You have to choose the type at compile time ITM
 * Notes: You can choose the tree type --> binary, quad, octree, choose below
 */

const float bvhtree_kdop_axes[13][3] = {
    {1.0, 0, 0},
    {0, 1.0, 0},
    {0, 0, 1.0},
    {1.0, 1.0, 1.0},
    {1.0, -1.0, 1.0},
    {1.0, 1.0, -1.0},
    {1.0, -1.0, -1.0},
    {1.0, 1.0, 0},
    {1.0, 0, 1.0},
    {0, 1.0, 1.0},
    {1.0, -1.0, 0},
    {1.0, 0, -1.0},
    {0, 1.0, -1.0},
};

/* Used to correct the epsilon and thus match the overlap distance. */
static const float bvhtree_kdop_axes_length[13] = {
    1.0f,
    1.0f,
    1.0f,
    1.7320508075688772f,
    1.7320508075688772f,
    1.7320508075688772f,
    1.7320508075688772f,
    1.4142135623730951f,
    1.4142135623730951f,
    1.4142135623730951f,
    1.4142135623730951f,
    1.4142135623730951f,
    1.4142135623730951f,
};

/* -------------------------------------------------------------------- */
/** \name Utility Functions
 * \{ */

MINLINE axis_t min_axis(axis_t a, axis_t b)
{
  return (a < b) ? a : b;
}
#if 0
MINLINE axis_t max_axis(axis_t a, axis_t b)
{
  return (b < a) ? a : b;
}
#endif

/**
 * Intro-sort
 * with permission deriving from the following Java code:
 * http://ralphunden.net/content/tutorials/a-guide-to-introsort/
 * and he derived it from the SUN STL
 */

static void node_minmax_init(const BVHTree *tree, BVHNode *node)
{
  axis_t axis_iter;
  float (*bv)[2] = (float (*)[2])node->bv;

  for (axis_iter = tree->start_axis; axis_iter != tree->stop_axis; axis_iter++) {
    bv[axis_iter][0] = FLT_MAX;
    bv[axis_iter][1] = -FLT_MAX;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Balance Utility Functions
 * \{ */

/**
 * Insertion sort algorithm
 */
static void bvh_insertionsort(BVHNode **a, int lo, int hi, int axis)
{
  int i, j;
  BVHNode *t;
  for (i = lo; i < hi; i++) {
    j = i;
    t = a[i];
    while ((j != lo) && (t->bv[axis] < (a[j - 1])->bv[axis])) {
      a[j] = a[j - 1];
      j--;
    }
    a[j] = t;
  }
}

static int bvh_partition(BVHNode **a, int lo, int hi, const BVHNode *x, int axis)
{
  int i = lo, j = hi;
  while (true) {
    while (a[i]->bv[axis] < x->bv[axis]) {
      i++;
    }
    j--;
    while (x->bv[axis] < a[j]->bv[axis]) {
      j--;
    }
    if (!(i < j)) {
      return i;
    }
    SWAP(BVHNode *, a[i], a[j]);
    i++;
  }
}

/* returns Sortable */
static BVHNode *bvh_medianof3(BVHNode **a, int lo, int mid, int hi, int axis)
{
  if ((a[mid])->bv[axis] < (a[lo])->bv[axis]) {
    if ((a[hi])->bv[axis] < (a[mid])->bv[axis]) {
      return a[mid];
    }
    if ((a[hi])->bv[axis] < (a[lo])->bv[axis]) {
      return a[hi];
    }
    return a[lo];
  }

  if ((a[hi])->bv[axis] < (a[mid])->bv[axis]) {
    if ((a[hi])->bv[axis] < (a[lo])->bv[axis]) {
      return a[lo];
    }
    return a[hi];
  }
  return a[mid];
}

/**
 * \note after a call to this function you can expect one of:
 * - every node to left of a[n] are smaller or equal to it
 * - every node to the right of a[n] are greater or equal to it.
 */
static void partition_nth_element(BVHNode **a, int begin, int end, const int n, const int axis)
{
  while (end - begin > 3) {
    const int cut = bvh_partition(
        a, begin, end, bvh_medianof3(a, begin, (begin + end) / 2, end - 1, axis), axis);
    if (cut <= n) {
      begin = cut;
    }
    else {
      end = cut;
    }
  }
  bvh_insertionsort(a, begin, end, axis);
}

#ifdef USE_SKIP_LINKS
static void build_skip_links(BVHTree *tree, BVHNode *node, BVHNode *left, BVHNode *right)
{
  int i;

  node->skip[0] = left;
  node->skip[1] = right;

  for (i = 0; i < node->node_num; i++) {
    if (i + 1 < node->node_num) {
      build_skip_links(tree, node->children[i], left, node->children[i + 1]);
    }
    else {
      build_skip_links(tree, node->children[i], left, right);
    }

    left = node->children[i];
  }
}
#endif

/*
 * BVHTree bounding volumes functions
 */
static void create_kdop_hull(
    const BVHTree *tree, BVHNode *node, const float *co, int numpoints, int moving)
{
  float newminmax;
  float *bv = node->bv;
  int k;
  axis_t axis_iter;

  /* Don't initialize bounds for the moving case */
  if (!moving) {
    node_minmax_init(tree, node);
  }

  for (k = 0; k < numpoints; k++) {
    /* for all Axes. */
    for (axis_iter = tree->start_axis; axis_iter < tree->stop_axis; axis_iter++) {
      newminmax = dot_v3v3(&co[k * 3], bvhtree_kdop_axes[axis_iter]);
      bv[2 * axis_iter] = std::min(newminmax, bv[2 * axis_iter]);
      bv[(2 * axis_iter) + 1] = std::max(newminmax, bv[(2 * axis_iter) + 1]);
    }
  }
}

/**
 * \note depends on the fact that the BVH's for each face is already built
 */
static void refit_kdop_hull(const BVHTree *tree, BVHNode *node, int start, int end)
{
  float newmin, newmax;
  float *__restrict bv = node->bv;
  int j;
  axis_t axis_iter;

  node_minmax_init(tree, node);

  for (j = start; j < end; j++) {
    const float *__restrict node_bv = tree->nodes[j]->bv;

    /* for all Axes. */
    for (axis_iter = tree->start_axis; axis_iter < tree->stop_axis; axis_iter++) {
      newmin = node_bv[(2 * axis_iter)];
      bv[(2 * axis_iter)] = std::min(newmin, bv[(2 * axis_iter)]);

      newmax = node_bv[(2 * axis_iter) + 1];
      bv[(2 * axis_iter) + 1] = std::max(newmax, bv[(2 * axis_iter) + 1]);
    }
  }
}

/**
 * Only supports x,y,z axis in the moment
 * but we should use a plain and simple function here for speed sake.
 */
static char get_largest_axis(const float *bv)
{
  float middle_point[3];

  middle_point[0] = (bv[1]) - (bv[0]); /* x axis */
  middle_point[1] = (bv[3]) - (bv[2]); /* y axis */
  middle_point[2] = (bv[5]) - (bv[4]); /* z axis */
  if (middle_point[0] > middle_point[1]) {
    if (middle_point[0] > middle_point[2]) {
      return 1; /* max x axis */
    }
    return 5; /* max z axis */
  }
  if (middle_point[1] > middle_point[2]) {
    return 3; /* max y axis */
  }
  return 5; /* max z axis */
}

/**
 * bottom-up update of bvh node BV
 * join the children on the parent BV.
 */
static void node_join(BVHTree *tree, BVHNode *node)
{
  int i;
  axis_t axis_iter;

  node_minmax_init(tree, node);

  for (i = 0; i < tree->tree_type; i++) {
    if (node->children[i]) {
      for (axis_iter = tree->start_axis; axis_iter < tree->stop_axis; axis_iter++) {
        /* update minimum */
        node->bv[(2 * axis_iter)] = std::min(node->children[i]->bv[(2 * axis_iter)],
                                             node->bv[(2 * axis_iter)]);

        /* update maximum */
        node->bv[(2 * axis_iter) + 1] = std::max(node->children[i]->bv[(2 * axis_iter) + 1],
                                                 node->bv[(2 * axis_iter) + 1]);
      }
    }
    else {
      break;
    }
  }
}

#ifdef USE_PRINT_TREE

/* -------------------------------------------------------------------- */
/** \name * Debug and Information Functions
 * \{ */

static void bvhtree_print_tree(BVHTree *tree, BVHNode *node, int depth)
{
  int i;
  axis_t axis_iter;

  for (i = 0; i < depth; i++) {
    printf(" ");
  }
  printf(" - %d (%ld): ", node->index, (long int)(node - tree->nodearray));
  for (axis_iter = (axis_t)(2 * tree->start_axis); axis_iter < (axis_t)(2 * tree->stop_axis);
       axis_iter++)
  {
    printf("%.3f ", node->bv[axis_iter]);
  }
  printf("\n");

  for (i = 0; i < tree->tree_type; i++) {
    if (node->children[i]) {
      bvhtree_print_tree(tree, node->children[i], depth + 1);
    }
  }
}

static void bvhtree_info(BVHTree *tree)
{
  printf("BVHTree Info: tree_type = %d, axis = %d, epsilon = %f\n",
         tree->tree_type,
         tree->axis,
         tree->epsilon);
  printf("nodes = %d, branches = %d, leafs = %d\n",
         tree->branch_num + tree->leaf_num,
         tree->branch_num,
         tree->leaf_num);
  printf("Memory per node = %ubytes\n",
         uint(sizeof(BVHNode) + sizeof(BVHNode *) * tree->tree_type + sizeof(float) * tree->axis));
  printf("BV memory = %ubytes\n", uint(MEM_allocN_len(tree->nodebv)));

  printf("Total memory = %ubytes\n",
         uint(sizeof(BVHTree) + MEM_allocN_len(tree->nodes) + MEM_allocN_len(tree->nodearray) +
              MEM_allocN_len(tree->nodechild) + MEM_allocN_len(tree->nodebv)));

  bvhtree_print_tree(tree, tree->nodes[tree->leaf_num], 0);
}

/** \} */

#endif /* USE_PRINT_TREE */

#ifdef USE_VERIFY_TREE

static void bvhtree_verify(BVHTree *tree)
{
  int i, j, check = 0;

  /* check the pointer list */
  for (i = 0; i < tree->leaf_num; i++) {
    if (tree->nodes[i]->parent == nullptr) {
      printf("Leaf has no parent: %d\n", i);
    }
    else {
      for (j = 0; j < tree->tree_type; j++) {
        if (tree->nodes[i]->parent->children[j] == tree->nodes[i]) {
          check = 1;
        }
      }
      if (!check) {
        printf("Parent child relationship doesn't match: %d\n", i);
      }
      check = 0;
    }
  }

  /* check the leaf list */
  for (i = 0; i < tree->leaf_num; i++) {
    if (tree->nodearray[i].parent == nullptr) {
      printf("Leaf has no parent: %d\n", i);
    }
    else {
      for (j = 0; j < tree->tree_type; j++) {
        if (tree->nodearray[i].parent->children[j] == &tree->nodearray[i]) {
          check = 1;
        }
      }
      if (!check) {
        printf("Parent child relationship doesn't match: %d\n", i);
      }
      check = 0;
    }
  }

  printf("branches: %d, leafs: %d, total: %d\n",
         tree->branch_num,
         tree->leaf_num,
         tree->branch_num + tree->leaf_num);
}
#endif /* USE_VERIFY_TREE */

/* Helper data and structures to build a min-leaf generalized implicit tree
 * This code can be easily reduced
 * (basically this is only method to calculate pow(k, n) in O(1).. and stuff like that) */
struct BVHBuildHelper {
  int tree_type;
  int leafs_num;

  /** Min number of leafs that are achievable from a node at depth `N`. */
  int leafs_per_child[32];
  /** Number of nodes at depth `N (tree_type^N)`. */
  int branches_on_level[32];

  /** Number of leafs that are placed on the level that is not 100% filled */
  int remain_leafs;
};

static void build_implicit_tree_helper(const BVHTree *tree, BVHBuildHelper *data)
{
  int depth = 0;
  int remain;
  int nnodes;

  data->leafs_num = tree->leaf_num;
  data->tree_type = tree->tree_type;

  /* Calculate the smallest tree_type^n such that tree_type^n >= leafs_num */
  for (data->leafs_per_child[0] = 1; data->leafs_per_child[0] < data->leafs_num;
       data->leafs_per_child[0] *= data->tree_type)
  {
    /* pass */
  }

  data->branches_on_level[0] = 1;

  for (depth = 1; (depth < 32) && data->leafs_per_child[depth - 1]; depth++) {
    data->branches_on_level[depth] = data->branches_on_level[depth - 1] * data->tree_type;
    data->leafs_per_child[depth] = data->leafs_per_child[depth - 1] / data->tree_type;
  }

  remain = data->leafs_num - data->leafs_per_child[1];
  nnodes = (remain + data->tree_type - 2) / (data->tree_type - 1);
  data->remain_leafs = remain + nnodes;
}

/**
 * Return the min index of all the leafs achievable with the given branch.
 */
static int implicit_leafs_index(const BVHBuildHelper *data, const int depth, const int child_index)
{
  int min_leaf_index = child_index * data->leafs_per_child[depth - 1];
  if (min_leaf_index <= data->remain_leafs) {
    return min_leaf_index;
  }
  if (data->leafs_per_child[depth]) {
    return data->leafs_num -
           (data->branches_on_level[depth - 1] - child_index) * data->leafs_per_child[depth];
  }
  return data->remain_leafs;
}

/**
 * Generalized implicit tree build
 *
 * An implicit tree is a tree where its structure is implied,
 * thus there is no need to store child pointers or indexes.
 * It's possible to find the position of the child or the parent with simple math
 * (multiplication and addition).
 * This type of tree is for example used on heaps..
 * where node N has its child at indices N*2 and N*2+1.
 *
 * Although in this case the tree type is general.. and not know until run-time.
 * tree_type stands for the maximum number of children that a tree node can have.
 * All tree types >= 2 are supported.
 *
 * Advantages of the used trees include:
 * - No need to store child/parent relations (they are implicit);
 * - Any node child always has an index greater than the parent;
 * - Brother nodes are sequential in memory;
 * Some math relations derived for general implicit trees:
 *
 *   K = tree_type, ( 2 <= K )
 *   ROOT = 1
 *   N child of node A = A * K + (2 - K) + N, (0 <= N < K)
 *
 * Util methods:
 *   TODO...
 *    (looping elements, knowing if its a leaf or not.. etc...)
 */

/* This functions returns the number of branches needed to have the requested number of leafs. */
static int implicit_needed_branches(int tree_type, int leafs)
{
  return max_ii(1, (leafs + tree_type - 3) / (tree_type - 1));
}

/**
 * This function handles the problem of "sorting" the leafs (along the split_axis).
 *
 * It arranges the elements in the given partitions such that:
 * - any element in partition N is less or equal to any element in partition N+1.
 * - if all elements are different all partition will get the same subset of elements
 *   as if the array was sorted.
 *
 * partition P is described as the elements in the range ( nth[P], nth[P+1] ]
 *
 * TODO: This can be optimized a bit by doing a specialized nth_element instead of K nth_elements
 */
static void split_leafs(BVHNode **leafs_array,
                        const int nth[],
                        const int partitions,
                        const int split_axis)
{
  int i;
  for (i = 0; i < partitions - 1; i++) {
    if (nth[i] >= nth[partitions]) {
      break;
    }

    partition_nth_element(leafs_array, nth[i], nth[partitions], nth[i + 1], split_axis);
  }
}

struct BVHDivNodesData {
  const BVHTree *tree;
  BVHNode *branches_array;
  BVHNode **leafs_array;

  int tree_type;
  int tree_offset;

  const BVHBuildHelper *data;

  int depth;
  int i;
  int first_of_next_level;
};

static void non_recursive_bvh_div_nodes_task_cb(void *__restrict userdata,
                                                const int j,
                                                const TaskParallelTLS *__restrict /*tls*/)
{
  BVHDivNodesData *data = static_cast<BVHDivNodesData *>(userdata);

  int k;
  const int parent_level_index = j - data->i;
  BVHNode *parent = &data->branches_array[j];
  int nth_positions[MAX_TREETYPE + 1];
  char split_axis;

  int parent_leafs_begin = implicit_leafs_index(data->data, data->depth, parent_level_index);
  int parent_leafs_end = implicit_leafs_index(data->data, data->depth, parent_level_index + 1);

  /* This calculates the bounding box of this branch
   * and chooses the largest axis as the axis to divide leafs */
  refit_kdop_hull(data->tree, parent, parent_leafs_begin, parent_leafs_end);
  split_axis = get_largest_axis(parent->bv);

  /* Save split axis (this can be used on ray-tracing to speedup the query time) */
  parent->main_axis = split_axis / 2;

  /* Split the children along the split_axis, NOTE: its not needed to sort the whole leafs array
   * Only to assure that the elements are partitioned on a way that each child takes the elements
   * it would take in case the whole array was sorted.
   * Split_leafs takes care of that "sort" problem. */
  nth_positions[0] = parent_leafs_begin;
  nth_positions[data->tree_type] = parent_leafs_end;
  for (k = 1; k < data->tree_type; k++) {
    const int child_index = j * data->tree_type + data->tree_offset + k;
    /* child level index */
    const int child_level_index = child_index - data->first_of_next_level;
    nth_positions[k] = implicit_leafs_index(data->data, data->depth + 1, child_level_index);
  }

  split_leafs(data->leafs_array, nth_positions, data->tree_type, split_axis);

  /* Setup `children` and `node_num` counters
   * Not really needed but currently most of BVH code
   * relies on having an explicit children structure */
  for (k = 0; k < data->tree_type; k++) {
    const int child_index = j * data->tree_type + data->tree_offset + k;
    /* child level index */
    const int child_level_index = child_index - data->first_of_next_level;

    const int child_leafs_begin = implicit_leafs_index(
        data->data, data->depth + 1, child_level_index);
    const int child_leafs_end = implicit_leafs_index(
        data->data, data->depth + 1, child_level_index + 1);

    if (child_leafs_end - child_leafs_begin > 1) {
      parent->children[k] = &data->branches_array[child_index];
      parent->children[k]->parent = parent;
    }
    else if (child_leafs_end - child_leafs_begin == 1) {
      parent->children[k] = data->leafs_array[child_leafs_begin];
      parent->children[k]->parent = parent;
    }
    else {
      break;
    }
  }
  parent->node_num = char(k);
}

/**
 * This functions builds an optimal implicit tree from the given leafs.
 * Where optimal stands for:
 * - The resulting tree will have the smallest number of branches;
 * - At most only one branch will have nullptr children;
 * - All leafs will be stored at level N or N+1.
 *
 * This function creates an implicit tree on branches_array,
 * the leafs are given on the leafs_array.
 *
 * The tree is built per depth levels. First branches at depth 1.. then branches at depth 2.. etc..
 * The reason is that we can build level N+1 from level N without any data dependencies..
 * thus it allows to use multi-thread building.
 *
 * To archive this is necessary to find how much leafs are accessible from a certain branch,
 * #BVHBuildHelper, #implicit_needed_branches and #implicit_leafs_index
 * are auxiliary functions to solve that "optimal-split".
 */
static void non_recursive_bvh_div_nodes(const BVHTree *tree,
                                        BVHNode *branches_array,
                                        BVHNode **leafs_array,
                                        int leafs_num)
{
  int i;

  const int tree_type = tree->tree_type;
  /* this value is 0 (on binary trees) and negative on the others */
  const int tree_offset = 2 - tree->tree_type;

  const int branches_num = implicit_needed_branches(tree_type, leafs_num);

  BVHBuildHelper data;
  int depth;

  {
    /* set parent from root node to nullptr */
    BVHNode *root = &branches_array[1];
    root->parent = nullptr;

    /* Most of bvhtree code relies on 1-leaf trees having at least one branch
     * We handle that special case here */
    if (leafs_num == 1) {
      refit_kdop_hull(tree, root, 0, leafs_num);
      root->main_axis = get_largest_axis(root->bv) / 2;
      root->node_num = 1;
      root->children[0] = leafs_array[0];
      root->children[0]->parent = root;
      return;
    }
  }

  build_implicit_tree_helper(tree, &data);

  BVHDivNodesData cb_data{};
  cb_data.tree = tree;
  cb_data.branches_array = branches_array;
  cb_data.leafs_array = leafs_array;
  cb_data.tree_type = tree_type;
  cb_data.tree_offset = tree_offset;
  cb_data.data = &data;
  cb_data.first_of_next_level = 0;
  cb_data.depth = 0;
  cb_data.i = 0;

  /* Loop tree levels (log N) loops */
  for (i = 1, depth = 1; i <= branches_num; i = i * tree_type + tree_offset, depth++) {
    const int first_of_next_level = i * tree_type + tree_offset;
    /* index of last branch on this level */
    const int i_stop = min_ii(first_of_next_level, branches_num + 1);

    /* Loop all branches on this level */
    cb_data.first_of_next_level = first_of_next_level;
    cb_data.i = i;
    cb_data.depth = depth;

    if (true) {
      TaskParallelSettings settings;
      BLI_parallel_range_settings_defaults(&settings);
      settings.use_threading = (leafs_num > KDOPBVH_THREAD_LEAF_THRESHOLD);
      BLI_task_parallel_range(i, i_stop, &cb_data, non_recursive_bvh_div_nodes_task_cb, &settings);
    }
    else {
      /* Less hassle for debugging. */
      TaskParallelTLS tls = {nullptr};
      for (int i_task = i; i_task < i_stop; i_task++) {
        non_recursive_bvh_div_nodes_task_cb(&cb_data, i_task, &tls);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BLI_bvhtree API
 * \{ */

BVHTree *BLI_bvhtree_new(int maxsize, float epsilon, char tree_type, char axis)
{
  int numnodes, i;

  BLI_assert(tree_type >= 2 && tree_type <= MAX_TREETYPE);

  BVHTree *tree = MEM_callocN<BVHTree>(__func__);

  /* tree epsilon must be >= FLT_EPSILON
   * so that tangent rays can still hit a bounding volume..
   * this bug would show up when casting a ray aligned with a KDOP-axis
   * and with an edge of 2 faces */
  epsilon = max_ff(FLT_EPSILON, epsilon);

  if (tree) {
    tree->epsilon = epsilon;
    tree->tree_type = tree_type;
    tree->axis = axis;

    if (axis == 26) {
      tree->start_axis = 0;
      tree->stop_axis = 13;
    }
    else if (axis == 18) {
      tree->start_axis = 7;
      tree->stop_axis = 13;
    }
    else if (axis == 14) {
      tree->start_axis = 0;
      tree->stop_axis = 7;
    }
    else if (axis == 8) { /* AABB */
      tree->start_axis = 0;
      tree->stop_axis = 4;
    }
    else if (axis == 6) { /* OBB */
      tree->start_axis = 0;
      tree->stop_axis = 3;
    }
    else {
      /* should never happen! */
      BLI_assert_unreachable();

      goto fail;
    }

    /* Allocate arrays */
    numnodes = maxsize + implicit_needed_branches(tree_type, maxsize) + tree_type;

    tree->nodes = MEM_calloc_arrayN<BVHNode *>(size_t(numnodes), "BVHNodes");
    tree->nodebv = MEM_calloc_arrayN<float>(axis * size_t(numnodes), "BVHNodeBV");
    tree->nodechild = MEM_calloc_arrayN<BVHNode *>(tree_type * size_t(numnodes), "BVHNodeBV");
    tree->nodearray = MEM_calloc_arrayN<BVHNode>(size_t(numnodes), "BVHNodeArray");

    if (UNLIKELY((!tree->nodes) || (!tree->nodebv) || (!tree->nodechild) || (!tree->nodearray))) {
      goto fail;
    }

    /* link the dynamic bv and child links */
    for (i = 0; i < numnodes; i++) {
      tree->nodearray[i].bv = &tree->nodebv[i * axis];
      tree->nodearray[i].children = &tree->nodechild[i * tree_type];
    }
  }
  return tree;

fail:
  BLI_bvhtree_free(tree);
  return nullptr;
}

void BLI_bvhtree_free(BVHTree *tree)
{
  if (tree) {
    MEM_SAFE_FREE(tree->nodes);
    MEM_SAFE_FREE(tree->nodearray);
    MEM_SAFE_FREE(tree->nodebv);
    MEM_SAFE_FREE(tree->nodechild);
    MEM_freeN(tree);
  }
}

void BLI_bvhtree_balance(BVHTree *tree)
{
  BVHNode **leafs_array = tree->nodes;

  /* This function should only be called once
   * (some big bug goes here if its being called more than once per tree) */
  BLI_assert(tree->branch_num == 0);

  /* Build the implicit tree */
  non_recursive_bvh_div_nodes(
      tree, tree->nodearray + (tree->leaf_num - 1), leafs_array, tree->leaf_num);

  /* current code expects the branches to be linked to the nodes array
   * we perform that linkage here */
  tree->branch_num = implicit_needed_branches(tree->tree_type, tree->leaf_num);
  for (int i = 0; i < tree->branch_num; i++) {
    tree->nodes[tree->leaf_num + i] = &tree->nodearray[tree->leaf_num + i];
  }

#ifdef USE_SKIP_LINKS
  build_skip_links(tree, tree->nodes[tree->leaf_num], nullptr, nullptr);
#endif

#ifdef USE_VERIFY_TREE
  bvhtree_verify(tree);
#endif

#ifdef USE_PRINT_TREE
  bvhtree_info(tree);
#endif
}

static void bvhtree_node_inflate(const BVHTree *tree, BVHNode *node, const float dist)
{
  axis_t axis_iter;
  for (axis_iter = tree->start_axis; axis_iter < tree->stop_axis; axis_iter++) {
    float dist_corrected = dist * bvhtree_kdop_axes_length[axis_iter];
    node->bv[(2 * axis_iter)] -= dist_corrected;     /* minimum */
    node->bv[(2 * axis_iter) + 1] += dist_corrected; /* maximum */
  }
}

void BLI_bvhtree_insert(BVHTree *tree, int index, const float co[3], int numpoints)
{
  BVHNode *node = nullptr;

  /* insert should only possible as long as tree->branch_num is 0 */
  BLI_assert(tree->branch_num <= 0);
  BLI_assert((size_t)tree->leaf_num < MEM_allocN_len(tree->nodes) / sizeof(*(tree->nodes)));

  node = tree->nodes[tree->leaf_num] = &(tree->nodearray[tree->leaf_num]);
  tree->leaf_num++;

  create_kdop_hull(tree, node, co, numpoints, 0);
  node->index = index;

  /* inflate the bv with some epsilon */
  bvhtree_node_inflate(tree, node, tree->epsilon);
}

bool BLI_bvhtree_update_node(
    BVHTree *tree, int index, const float co[3], const float co_moving[3], int numpoints)
{
  BVHNode *node = nullptr;

  /* check if index exists */
  if (index > tree->leaf_num) {
    return false;
  }

  node = tree->nodearray + index;

  create_kdop_hull(tree, node, co, numpoints, 0);

  if (co_moving) {
    create_kdop_hull(tree, node, co_moving, numpoints, 1);
  }

  /* inflate the bv with some epsilon */
  bvhtree_node_inflate(tree, node, tree->epsilon);

  return true;
}

void BLI_bvhtree_update_tree(BVHTree *tree)
{
  /* Update bottom=>top
   * TRICKY: the way we build the tree all the children have an index greater than the parent
   * This allows us todo a bottom up update by starting on the bigger numbered branch. */

  BVHNode **root = tree->nodes + tree->leaf_num;
  BVHNode **index = tree->nodes + tree->leaf_num + tree->branch_num - 1;

  for (; index >= root; index--) {
    node_join(tree, *index);
  }
}
int BLI_bvhtree_get_len(const BVHTree *tree)
{
  return tree->leaf_num;
}

int BLI_bvhtree_get_tree_type(const BVHTree *tree)
{
  return tree->tree_type;
}

float BLI_bvhtree_get_epsilon(const BVHTree *tree)
{
  return tree->epsilon;
}

void BLI_bvhtree_get_bounding_box(const BVHTree *tree, float r_bb_min[3], float r_bb_max[3])
{
  const BVHNode *root = tree->nodes[tree->leaf_num];
  if (root != nullptr) {
    const float bb_min[3] = {root->bv[0], root->bv[2], root->bv[4]};
    const float bb_max[3] = {root->bv[1], root->bv[3], root->bv[5]};
    copy_v3_v3(r_bb_min, bb_min);
    copy_v3_v3(r_bb_max, bb_max);
  }
  else {
    BLI_assert(false);
    zero_v3(r_bb_min);
    zero_v3(r_bb_max);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BLI_bvhtree_overlap
 * \{ */

/**
 * overlap - is it possible for 2 bv's to collide ?
 */
static bool tree_overlap_test(const BVHNode *node1,
                              const BVHNode *node2,
                              axis_t start_axis,
                              axis_t stop_axis)
{
  const float *bv1 = node1->bv + (start_axis << 1);
  const float *bv2 = node2->bv + (start_axis << 1);
  const float *bv1_end = node1->bv + (stop_axis << 1);

  /* test all axis if min + max overlap */
  for (; bv1 != bv1_end; bv1 += 2, bv2 += 2) {
    if ((bv1[0] > bv2[1]) || (bv2[0] > bv1[1])) {
      return false;
    }
  }

  return true;
}

static void tree_overlap_traverse(BVHOverlapData_Thread *data_thread,
                                  const BVHNode *node1,
                                  const BVHNode *node2)
{
  const BVHOverlapData_Shared *data = data_thread->shared;
  int j;

  if (tree_overlap_test(node1, node2, data->start_axis, data->stop_axis)) {
    /* check if node1 is a leaf */
    if (!node1->node_num) {
      /* check if node2 is a leaf */
      if (!node2->node_num) {
        BVHTreeOverlap *overlap;

        if (UNLIKELY(node1 == node2)) {
          return;
        }

        /* both leafs, insert overlap! */
        overlap = static_cast<BVHTreeOverlap *>(BLI_stack_push_r(data_thread->overlap));
        overlap->indexA = node1->index;
        overlap->indexB = node2->index;
      }
      else {
        for (j = 0; j < data->tree2->tree_type; j++) {
          if (node2->children[j]) {
            tree_overlap_traverse(data_thread, node1, node2->children[j]);
          }
        }
      }
    }
    else {
      for (j = 0; j < data->tree1->tree_type; j++) {
        if (node1->children[j]) {
          tree_overlap_traverse(data_thread, node1->children[j], node2);
        }
      }
    }
  }
}

/**
 * a version of #tree_overlap_traverse that runs a callback to check if the nodes really intersect.
 */
static void tree_overlap_traverse_cb(BVHOverlapData_Thread *data_thread,
                                     const BVHNode *node1,
                                     const BVHNode *node2)
{
  BVHOverlapData_Shared *data = data_thread->shared;
  int j;

  if (tree_overlap_test(node1, node2, data->start_axis, data->stop_axis)) {
    /* check if node1 is a leaf */
    if (!node1->node_num) {
      /* check if node2 is a leaf */
      if (!node2->node_num) {
        BVHTreeOverlap *overlap;

        if (UNLIKELY(node1 == node2)) {
          return;
        }

        /* only difference to tree_overlap_traverse! */
        if (data->callback(data->userdata, node1->index, node2->index, data_thread->thread)) {
          /* both leafs, insert overlap! */
          overlap = static_cast<BVHTreeOverlap *>(BLI_stack_push_r(data_thread->overlap));
          overlap->indexA = node1->index;
          overlap->indexB = node2->index;
        }
      }
      else {
        for (j = 0; j < data->tree2->tree_type; j++) {
          if (node2->children[j]) {
            tree_overlap_traverse_cb(data_thread, node1, node2->children[j]);
          }
        }
      }
    }
    else {
      for (j = 0; j < data->tree1->tree_type; j++) {
        if (node1->children[j]) {
          tree_overlap_traverse_cb(data_thread, node1->children[j], node2);
        }
      }
    }
  }
}

/**
 * a version of #tree_overlap_traverse_cb that break on first true return.
 */
static bool tree_overlap_traverse_num(BVHOverlapData_Thread *data_thread,
                                      const BVHNode *node1,
                                      const BVHNode *node2)
{
  BVHOverlapData_Shared *data = data_thread->shared;
  int j;

  if (tree_overlap_test(node1, node2, data->start_axis, data->stop_axis)) {
    /* check if node1 is a leaf */
    if (!node1->node_num) {
      /* check if node2 is a leaf */
      if (!node2->node_num) {
        BVHTreeOverlap *overlap;

        if (UNLIKELY(node1 == node2)) {
          return false;
        }

        /* only difference to tree_overlap_traverse! */
        if (!data->callback ||
            data->callback(data->userdata, node1->index, node2->index, data_thread->thread))
        {
          /* both leafs, insert overlap! */
          if (data_thread->overlap) {
            overlap = static_cast<BVHTreeOverlap *>(BLI_stack_push_r(data_thread->overlap));
            overlap->indexA = node1->index;
            overlap->indexB = node2->index;
          }
          return (--data_thread->max_interactions) == 0;
        }
      }
      else {
        for (j = 0; j < node2->node_num; j++) {
          if (tree_overlap_traverse_num(data_thread, node1, node2->children[j])) {
            return true;
          }
        }
      }
    }
    else {
      const uint max_interactions = data_thread->max_interactions;
      for (j = 0; j < node1->node_num; j++) {
        if (tree_overlap_traverse_num(data_thread, node1->children[j], node2)) {
          data_thread->max_interactions = max_interactions;
        }
      }
    }
  }
  return false;
}

/** Calls the appropriate recursive overlap traversal function. */
static void tree_overlap_invoke_traverse(BVHOverlapData_Thread *data,
                                         const BVHNode *node1,
                                         const BVHNode *node2)
{
  if (data->max_interactions) {
    tree_overlap_traverse_num(data, node1, node2);
  }
  else if (data->shared->callback) {
    tree_overlap_traverse_cb(data, node1, node2);
  }
  else {
    tree_overlap_traverse(data, node1, node2);
  }
}

/** Self-overlap traversal with callback. */
static void tree_overlap_traverse_self_cb(BVHOverlapData_Thread *data_thread, const BVHNode *node)
{
  for (int i = 0; i < node->node_num; i++) {
    /* Recursively compute self-overlap within each child. */
    tree_overlap_traverse_self_cb(data_thread, node->children[i]);

    /* Compute overlap of pairs of children, testing each one only once (assume symmetry). */
    for (int j = i + 1; j < node->node_num; j++) {
      tree_overlap_traverse_cb(data_thread, node->children[i], node->children[j]);
    }
  }
}

/** Self-overlap traversal without callback. */
static void tree_overlap_traverse_self(BVHOverlapData_Thread *data_thread, const BVHNode *node)
{
  for (int i = 0; i < node->node_num; i++) {
    /* Recursively compute self-overlap within each child. */
    tree_overlap_traverse_self(data_thread, node->children[i]);

    /* Compute overlap of pairs of children, testing each one only once (assume symmetry). */
    for (int j = i + 1; j < node->node_num; j++) {
      tree_overlap_traverse(data_thread, node->children[i], node->children[j]);
    }
  }
}

/** Calls the appropriate recursive self-overlap traversal. */
static void tree_overlap_invoke_traverse_self(BVHOverlapData_Thread *data_thread,
                                              const BVHNode *node)
{
  if (data_thread->shared->callback) {
    tree_overlap_traverse_self_cb(data_thread, node);
  }
  else {
    tree_overlap_traverse_self(data_thread, node);
  }
}

int BLI_bvhtree_overlap_thread_num(const BVHTree *tree)
{
  return std::min<int>(tree->tree_type, tree->nodes[tree->leaf_num]->node_num);
}

static void bvhtree_overlap_task_cb(void *__restrict userdata,
                                    const int j,
                                    const TaskParallelTLS *__restrict /*tls*/)
{
  BVHOverlapData_Thread *data = &((BVHOverlapData_Thread *)userdata)[j];
  BVHOverlapData_Shared *data_shared = data->shared;

  const BVHNode *root1 = data_shared->tree1->nodes[data_shared->tree1->leaf_num];

  if (data_shared->use_self) {
    /* This code matches one outer loop iteration within traverse_self. */
    tree_overlap_invoke_traverse_self(data, root1->children[j]);

    for (int k = j + 1; k < root1->node_num; k++) {
      tree_overlap_invoke_traverse(data, root1->children[j], root1->children[k]);
    }
  }
  else {
    const BVHNode *root2 = data_shared->tree2->nodes[data_shared->tree2->leaf_num];

    tree_overlap_invoke_traverse(data, root1->children[j], root2);
  }
}

BVHTreeOverlap *BLI_bvhtree_overlap_ex(
    const BVHTree *tree1,
    const BVHTree *tree2,
    uint *r_overlap_num,
    /* optional callback to test the overlap before adding (must be thread-safe!) */
    BVHTree_OverlapCallback callback,
    void *userdata,
    const uint max_interactions,
    const int flag)
{
  bool overlap_pairs = (flag & BVH_OVERLAP_RETURN_PAIRS) != 0;
  bool use_threading = (flag & BVH_OVERLAP_USE_THREADING) != 0 &&
                       (tree1->leaf_num > KDOPBVH_THREAD_LEAF_THRESHOLD);
  bool use_self = (flag & BVH_OVERLAP_SELF) != 0;

  /* 'RETURN_PAIRS' was not implemented without 'max_interactions'. */
  BLI_assert(overlap_pairs || max_interactions);
  /* Self-overlap does not support max interactions (it's not symmetrical). */
  BLI_assert(!use_self || (tree1 == tree2 && !max_interactions));

  const int root_node_len = BLI_bvhtree_overlap_thread_num(tree1);
  const int thread_num = use_threading ? root_node_len : 1;
  int j;
  size_t total = 0;
  BVHTreeOverlap *overlap = nullptr, *to = nullptr;
  BVHOverlapData_Shared data_shared;
  BVHOverlapData_Thread *data = BLI_array_alloca(data, size_t(thread_num));
  axis_t start_axis, stop_axis;

  /* check for compatibility of both trees (can't compare 14-DOP with 18-DOP) */
  if (UNLIKELY((tree1->axis != tree2->axis) && (tree1->axis == 14 || tree2->axis == 14) &&
               (tree1->axis == 18 || tree2->axis == 18)))
  {
    BLI_assert(0);
    return nullptr;
  }

  if (UNLIKELY(use_self && tree1 != tree2)) {
    use_self = false;
  }

  const BVHNode *root1 = tree1->nodes[tree1->leaf_num];
  const BVHNode *root2 = tree2->nodes[tree2->leaf_num];

  start_axis = min_axis(tree1->start_axis, tree2->start_axis);
  stop_axis = min_axis(tree1->stop_axis, tree2->stop_axis);

  /* fast check root nodes for collision before doing big splitting + traversal */
  if (!tree_overlap_test(root1, root2, start_axis, stop_axis)) {
    return nullptr;
  }

  data_shared.tree1 = tree1;
  data_shared.tree2 = tree2;
  data_shared.start_axis = start_axis;
  data_shared.stop_axis = stop_axis;
  data_shared.use_self = use_self;

  /* can be nullptr */
  data_shared.callback = callback;
  data_shared.userdata = userdata;

  for (j = 0; j < thread_num; j++) {
    /* init BVHOverlapData_Thread */
    data[j].shared = &data_shared;
    data[j].overlap = overlap_pairs ? BLI_stack_new(sizeof(BVHTreeOverlap), __func__) : nullptr;
    data[j].max_interactions = use_self ? 0 : max_interactions;

    /* for callback */
    data[j].thread = j;
  }

  if (use_threading) {
    TaskParallelSettings settings;
    BLI_parallel_range_settings_defaults(&settings);
    settings.min_iter_per_thread = 1;
    BLI_task_parallel_range(0, root_node_len, data, bvhtree_overlap_task_cb, &settings);
  }
  else if (use_self) {
    tree_overlap_invoke_traverse_self(data, root1);
  }
  else {
    tree_overlap_invoke_traverse(data, root1, root2);
  }

  if (overlap_pairs) {
    for (j = 0; j < thread_num; j++) {
      total += BLI_stack_count(data[j].overlap);
    }

    to = overlap = MEM_malloc_arrayN<BVHTreeOverlap>(total, "BVHTreeOverlap");

    for (j = 0; j < thread_num; j++) {
      uint count = uint(BLI_stack_count(data[j].overlap));
      BLI_stack_pop_n(data[j].overlap, to, count);
      BLI_stack_free(data[j].overlap);
      to += count;
    }
    *r_overlap_num = uint(total);
  }

  return overlap;
}

BVHTreeOverlap *BLI_bvhtree_overlap(
    const BVHTree *tree1,
    const BVHTree *tree2,
    uint *r_overlap_num,
    /* optional callback to test the overlap before adding (must be thread-safe!) */
    BVHTree_OverlapCallback callback,
    void *userdata)
{
  return BLI_bvhtree_overlap_ex(tree1,
                                tree2,
                                r_overlap_num,
                                callback,
                                userdata,
                                0,
                                BVH_OVERLAP_USE_THREADING | BVH_OVERLAP_RETURN_PAIRS);
}

BVHTreeOverlap *BLI_bvhtree_overlap_self(
    const BVHTree *tree,
    uint *r_overlap_num,
    /* optional callback to test the overlap before adding (must be thread-safe!) */
    BVHTree_OverlapCallback callback,
    void *userdata)
{
  return BLI_bvhtree_overlap_ex(tree,
                                tree,
                                r_overlap_num,
                                callback,
                                userdata,
                                0,
                                BVH_OVERLAP_USE_THREADING | BVH_OVERLAP_RETURN_PAIRS |
                                    BVH_OVERLAP_SELF);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BLI_bvhtree_intersect_plane
 * \{ */

static bool tree_intersect_plane_test(const float *bv, const float plane[4])
{
  /* TODO(@germano): Support other KDOP geometries. */
  const float bb_min[3] = {bv[0], bv[2], bv[4]};
  const float bb_max[3] = {bv[1], bv[3], bv[5]};
  float bb_near[3], bb_far[3];
  aabb_get_near_far_from_plane(plane, bb_min, bb_max, bb_near, bb_far);
  if ((plane_point_side_v3(plane, bb_near) > 0.0f) != (plane_point_side_v3(plane, bb_far) > 0.0f))
  {
    return true;
  }

  return false;
}

static void bvhtree_intersect_plane_dfs_recursive(BVHIntersectPlaneData *__restrict data,
                                                  const BVHNode *node)
{
  if (tree_intersect_plane_test(node->bv, data->plane)) {
    /* check if node is a leaf */
    if (!node->node_num) {
      int *intersect = static_cast<int *>(BLI_stack_push_r(data->intersect));
      *intersect = node->index;
    }
    else {
      for (int j = 0; j < data->tree->tree_type; j++) {
        if (node->children[j]) {
          bvhtree_intersect_plane_dfs_recursive(data, node->children[j]);
        }
      }
    }
  }
}

int *BLI_bvhtree_intersect_plane(const BVHTree *tree, float plane[4], uint *r_intersect_num)
{
  int *intersect = nullptr;
  size_t total = 0;

  if (tree->leaf_num) {
    BVHIntersectPlaneData data;
    data.tree = tree;
    copy_v4_v4(data.plane, plane);
    data.intersect = BLI_stack_new(sizeof(int), __func__);

    const BVHNode *root = tree->nodes[tree->leaf_num];
    bvhtree_intersect_plane_dfs_recursive(&data, root);

    total = BLI_stack_count(data.intersect);
    if (total) {
      intersect = MEM_malloc_arrayN<int>(total, __func__);
      BLI_stack_pop_n(data.intersect, intersect, uint(total));
    }
    BLI_stack_free(data.intersect);
  }
  *r_intersect_num = uint(total);
  return intersect;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BLI_bvhtree_find_nearest
 * \{ */

/* Determines the nearest point of the given node BV.
 * Returns the squared distance to that point. */
static float calc_nearest_point_squared(const float proj[3], BVHNode *node, float nearest[3])
{
  int i;
  const float *bv = node->bv;

  /* nearest on AABB hull */
  for (i = 0; i != 3; i++, bv += 2) {
    float val = proj[i];
    val = std::max(bv[0], val);
    val = std::min(bv[1], val);
    nearest[i] = val;
  }

  return len_squared_v3v3(proj, nearest);
}

/* Depth first search method */
static void dfs_find_nearest_dfs(BVHNearestData *data, BVHNode *node)
{
  if (node->node_num == 0) {
    if (data->callback) {
      data->callback(data->userdata, node->index, data->co, &data->nearest);
    }
    else {
      data->nearest.index = node->index;
      data->nearest.dist_sq = calc_nearest_point_squared(data->proj, node, data->nearest.co);
    }
  }
  else {
    /* Better heuristic to pick the closest node to dive on */
    int i;
    float nearest[3];

    if (data->proj[node->main_axis] <= node->children[0]->bv[node->main_axis * 2 + 1]) {

      for (i = 0; i != node->node_num; i++) {
        if (calc_nearest_point_squared(data->proj, node->children[i], nearest) >=
            data->nearest.dist_sq)
        {
          continue;
        }
        dfs_find_nearest_dfs(data, node->children[i]);
      }
    }
    else {
      for (i = node->node_num - 1; i >= 0; i--) {
        if (calc_nearest_point_squared(data->proj, node->children[i], nearest) >=
            data->nearest.dist_sq)
        {
          continue;
        }
        dfs_find_nearest_dfs(data, node->children[i]);
      }
    }
  }
}

static void dfs_find_nearest_begin(BVHNearestData *data, BVHNode *node)
{
  float nearest[3], dist_sq;
  dist_sq = calc_nearest_point_squared(data->proj, node, nearest);
  if (dist_sq >= data->nearest.dist_sq) {
    return;
  }
  dfs_find_nearest_dfs(data, node);
}

/* Priority queue method */
static void heap_find_nearest_inner(BVHNearestData *data, HeapSimple *heap, BVHNode *node)
{
  if (node->node_num == 0) {
    if (data->callback) {
      data->callback(data->userdata, node->index, data->co, &data->nearest);
    }
    else {
      data->nearest.index = node->index;
      data->nearest.dist_sq = calc_nearest_point_squared(data->proj, node, data->nearest.co);
    }
  }
  else {
    float nearest[3];

    for (int i = 0; i != node->node_num; i++) {
      float dist_sq = calc_nearest_point_squared(data->proj, node->children[i], nearest);

      if (dist_sq < data->nearest.dist_sq) {
        BLI_heapsimple_insert(heap, dist_sq, node->children[i]);
      }
    }
  }
}

static void heap_find_nearest_begin(BVHNearestData *data, BVHNode *root)
{
  float nearest[3];
  float dist_sq = calc_nearest_point_squared(data->proj, root, nearest);

  if (dist_sq < data->nearest.dist_sq) {
    HeapSimple *heap = BLI_heapsimple_new_ex(32);

    heap_find_nearest_inner(data, heap, root);

    while (!BLI_heapsimple_is_empty(heap) &&
           BLI_heapsimple_top_value(heap) < data->nearest.dist_sq)
    {
      BVHNode *node = static_cast<BVHNode *>(BLI_heapsimple_pop_min(heap));
      heap_find_nearest_inner(data, heap, node);
    }

    BLI_heapsimple_free(heap, nullptr);
  }
}

int BLI_bvhtree_find_nearest_ex(const BVHTree *tree,
                                const float co[3],
                                BVHTreeNearest *nearest,
                                BVHTree_NearestPointCallback callback,
                                void *userdata,
                                int flag)
{
  axis_t axis_iter;

  BVHNearestData data;
  BVHNode *root = tree->nodes[tree->leaf_num];

  /* init data to search */
  data.tree = tree;
  data.co = co;

  data.callback = callback;
  data.userdata = userdata;

  for (axis_iter = data.tree->start_axis; axis_iter != data.tree->stop_axis; axis_iter++) {
    data.proj[axis_iter] = dot_v3v3(data.co, bvhtree_kdop_axes[axis_iter]);
  }

  if (nearest) {
    memcpy(&data.nearest, nearest, sizeof(*nearest));
  }
  else {
    data.nearest.index = -1;
    data.nearest.dist_sq = FLT_MAX;
  }

  /* dfs search */
  if (root) {
    if (flag & BVH_NEAREST_OPTIMAL_ORDER) {
      heap_find_nearest_begin(&data, root);
    }
    else {
      dfs_find_nearest_begin(&data, root);
    }
  }

  /* copy back results */
  if (nearest) {
    memcpy(nearest, &data.nearest, sizeof(*nearest));
  }

  return data.nearest.index;
}

int BLI_bvhtree_find_nearest(const BVHTree *tree,
                             const float co[3],
                             BVHTreeNearest *nearest,
                             BVHTree_NearestPointCallback callback,
                             void *userdata)
{
  return BLI_bvhtree_find_nearest_ex(tree, co, nearest, callback, userdata, 0);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BLI_bvhtree_find_nearest_first
 * \{ */

static bool isect_aabb_v3(BVHNode *node, const float co[3])
{
  const BVHTreeAxisRange *bv = (const BVHTreeAxisRange *)node->bv;

  if (co[0] > bv[0].min && co[0] < bv[0].max && co[1] > bv[1].min && co[1] < bv[1].max &&
      co[2] > bv[2].min && co[2] < bv[2].max)
  {
    return true;
  }

  return false;
}

static bool dfs_find_duplicate_fast_dfs(BVHNearestData *data, BVHNode *node)
{
  if (node->node_num == 0) {
    if (isect_aabb_v3(node, data->co)) {
      if (data->callback) {
        const float dist_sq = data->nearest.dist_sq;
        data->callback(data->userdata, node->index, data->co, &data->nearest);
        return (data->nearest.dist_sq < dist_sq);
      }
      data->nearest.index = node->index;
      return true;
    }
  }
  else {
    /* Better heuristic to pick the closest node to dive on */
    int i;

    if (data->proj[node->main_axis] <= node->children[0]->bv[node->main_axis * 2 + 1]) {
      for (i = 0; i != node->node_num; i++) {
        if (isect_aabb_v3(node->children[i], data->co)) {
          if (dfs_find_duplicate_fast_dfs(data, node->children[i])) {
            return true;
          }
        }
      }
    }
    else {
      for (i = node->node_num; i--;) {
        if (isect_aabb_v3(node->children[i], data->co)) {
          if (dfs_find_duplicate_fast_dfs(data, node->children[i])) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

int BLI_bvhtree_find_nearest_first(const BVHTree *tree,
                                   const float co[3],
                                   const float dist_sq,
                                   BVHTree_NearestPointCallback callback,
                                   void *userdata)
{
  BVHNearestData data;
  BVHNode *root = tree->nodes[tree->leaf_num];

  /* init data to search */
  data.tree = tree;
  data.co = co;

  data.callback = callback;
  data.userdata = userdata;
  data.nearest.index = -1;
  data.nearest.dist_sq = dist_sq;

  /* dfs search */
  if (root) {
    dfs_find_duplicate_fast_dfs(&data, root);
  }

  return data.nearest.index;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BLI_bvhtree_ray_cast
 *
 * raycast is done by performing a DFS on the BVHTree and saving the closest hit.
 *
 * \{ */

/* Determines the distance that the ray must travel to hit the bounding volume of the given node */
static float ray_nearest_hit(const BVHRayCastData *data, const float bv[6])
{
  int i;

  float low = 0, upper = data->hit.dist;

  for (i = 0; i != 3; i++, bv += 2) {
    if (data->ray_dot_axis[i] == 0.0f) {
      /* axis aligned ray */
      if (data->ray.origin[i] < bv[0] - data->ray.radius ||
          data->ray.origin[i] > bv[1] + data->ray.radius)
      {
        return FLT_MAX;
      }
    }
    else {
      float ll = (bv[0] - data->ray.radius - data->ray.origin[i]) / data->ray_dot_axis[i];
      float lu = (bv[1] + data->ray.radius - data->ray.origin[i]) / data->ray_dot_axis[i];

      if (data->ray_dot_axis[i] > 0.0f) {
        low = std::max(ll, low);
        upper = std::min(lu, upper);
      }
      else {
        low = std::max(lu, low);
        upper = std::min(ll, upper);
      }

      if (low > upper) {
        return FLT_MAX;
      }
    }
  }
  return low;
}

/**
 * Determines the distance that the ray must travel to hit the bounding volume of the given node
 * Based on Tactical Optimization of Ray/Box Intersection, by Graham Fyffe
 * [http://tog.acm.org/resources/RTNews/html/rtnv21n1.html#art9]
 *
 * TODO: this doesn't take data->ray.radius into consideration. */
static float fast_ray_nearest_hit(const BVHRayCastData *data, const BVHNode *node)
{
  const float *bv = node->bv;

  float t1x = (bv[data->index[0]] - data->ray.origin[0]) * data->idot_axis[0];
  float t2x = (bv[data->index[1]] - data->ray.origin[0]) * data->idot_axis[0];
  float t1y = (bv[data->index[2]] - data->ray.origin[1]) * data->idot_axis[1];
  float t2y = (bv[data->index[3]] - data->ray.origin[1]) * data->idot_axis[1];
  float t1z = (bv[data->index[4]] - data->ray.origin[2]) * data->idot_axis[2];
  float t2z = (bv[data->index[5]] - data->ray.origin[2]) * data->idot_axis[2];

  if ((t1x > t2y || t2x < t1y || t1x > t2z || t2x < t1z || t1y > t2z || t2y < t1z) ||
      (t2x < 0.0f || t2y < 0.0f || t2z < 0.0f) ||
      (t1x > data->hit.dist || t1y > data->hit.dist || t1z > data->hit.dist))
  {
    return FLT_MAX;
  }
  return max_fff(t1x, t1y, t1z);
}

static void dfs_raycast(BVHRayCastData *data, BVHNode *node)
{
  int i;

  /* ray-bv is really fast.. and simple tests revealed its worth to test it
   * before calling the ray-primitive functions */
  /* XXX: temporary solution for particles until fast_ray_nearest_hit supports ray.radius */
  float dist = (data->ray.radius == 0.0f) ? fast_ray_nearest_hit(data, node) :
                                            ray_nearest_hit(data, node->bv);
  if (dist >= data->hit.dist) {
    return;
  }

  if (node->node_num == 0) {
    if (data->callback) {
      data->callback(data->userdata, node->index, &data->ray, &data->hit);
    }
    else {
      data->hit.index = node->index;
      data->hit.dist = dist;
      madd_v3_v3v3fl(data->hit.co, data->ray.origin, data->ray.direction, dist);
    }
  }
  else {
    /* pick loop direction to dive into the tree (based on ray direction and split axis) */
    if (data->ray_dot_axis[node->main_axis] > 0.0f) {
      for (i = 0; i != node->node_num; i++) {
        dfs_raycast(data, node->children[i]);
      }
    }
    else {
      for (i = node->node_num - 1; i >= 0; i--) {
        dfs_raycast(data, node->children[i]);
      }
    }
  }
}

/**
 * A version of #dfs_raycast with minor changes to reset the index & dist each ray cast.
 */
static void dfs_raycast_all(BVHRayCastData *data, BVHNode *node)
{
  int i;

  /* ray-bv is really fast.. and simple tests revealed its worth to test it
   * before calling the ray-primitive functions */
  /* XXX: temporary solution for particles until fast_ray_nearest_hit supports ray.radius */
  float dist = (data->ray.radius == 0.0f) ? fast_ray_nearest_hit(data, node) :
                                            ray_nearest_hit(data, node->bv);
  if (dist >= data->hit.dist) {
    return;
  }

  if (node->node_num == 0) {
    /* no need to check for 'data->callback' (using 'all' only makes sense with a callback). */
    dist = data->hit.dist;
    data->callback(data->userdata, node->index, &data->ray, &data->hit);
    data->hit.index = -1;
    data->hit.dist = dist;
  }
  else {
    /* pick loop direction to dive into the tree (based on ray direction and split axis) */
    if (data->ray_dot_axis[node->main_axis] > 0.0f) {
      for (i = 0; i != node->node_num; i++) {
        dfs_raycast_all(data, node->children[i]);
      }
    }
    else {
      for (i = node->node_num - 1; i >= 0; i--) {
        dfs_raycast_all(data, node->children[i]);
      }
    }
  }
}

static void bvhtree_ray_cast_data_precalc(BVHRayCastData *data, int flag)
{
  int i;

  for (i = 0; i < 3; i++) {
    data->ray_dot_axis[i] = dot_v3v3(data->ray.direction, bvhtree_kdop_axes[i]);

    if (fabsf(data->ray_dot_axis[i]) < FLT_EPSILON) {
      data->ray_dot_axis[i] = 0.0f;
      /* Sign is not important in this case, `data->index` is adjusted anyway. */
      data->idot_axis[i] = FLT_MAX;
    }
    else {
      data->idot_axis[i] = 1.0f / data->ray_dot_axis[i];
    }

    data->index[2 * i] = data->idot_axis[i] < 0.0f ? 1 : 0;
    data->index[2 * i + 1] = 1 - data->index[2 * i];
    data->index[2 * i] += 2 * i;
    data->index[2 * i + 1] += 2 * i;
  }

#ifdef USE_KDOPBVH_WATERTIGHT
  if (flag & BVH_RAYCAST_WATERTIGHT) {
    isect_ray_tri_watertight_v3_precalc(&data->isect_precalc, data->ray.direction);
    data->ray.isect_precalc = &data->isect_precalc;
  }
  else {
    data->ray.isect_precalc = nullptr;
  }
#else
  UNUSED_VARS(flag);
#endif
}

int BLI_bvhtree_ray_cast_ex(const BVHTree *tree,
                            const float co[3],
                            const float dir[3],
                            float radius,
                            BVHTreeRayHit *hit,
                            BVHTree_RayCastCallback callback,
                            void *userdata,
                            int flag)
{
  BVHRayCastData data;
  BVHNode *root = tree->nodes[tree->leaf_num];

  BLI_ASSERT_UNIT_V3(dir);

  data.tree = tree;

  data.callback = callback;
  data.userdata = userdata;

  copy_v3_v3(data.ray.origin, co);
  copy_v3_v3(data.ray.direction, dir);
  data.ray.radius = radius;

  bvhtree_ray_cast_data_precalc(&data, flag);

  if (hit) {
    memcpy(&data.hit, hit, sizeof(*hit));
  }
  else {
    data.hit.index = -1;
    data.hit.dist = BVH_RAYCAST_DIST_MAX;
  }

  if (root) {
    dfs_raycast(&data, root);
    //      iterative_raycast(&data, root);
  }

  if (hit) {
    memcpy(hit, &data.hit, sizeof(*hit));
  }

  return data.hit.index;
}

int BLI_bvhtree_ray_cast(const BVHTree *tree,
                         const float co[3],
                         const float dir[3],
                         float radius,
                         BVHTreeRayHit *hit,
                         BVHTree_RayCastCallback callback,
                         void *userdata)
{
  return BLI_bvhtree_ray_cast_ex(
      tree, co, dir, radius, hit, callback, userdata, BVH_RAYCAST_DEFAULT);
}

float BLI_bvhtree_bb_raycast(const float bv[6],
                             const float light_start[3],
                             const float light_end[3],
                             float pos[3])
{
  BVHRayCastData data;
  float dist;

  data.hit.dist = BVH_RAYCAST_DIST_MAX;

  /* get light direction */
  sub_v3_v3v3(data.ray.direction, light_end, light_start);

  data.ray.radius = 0.0;

  copy_v3_v3(data.ray.origin, light_start);

  normalize_v3(data.ray.direction);
  copy_v3_v3(data.ray_dot_axis, data.ray.direction);

  dist = ray_nearest_hit(&data, bv);

  madd_v3_v3v3fl(pos, light_start, data.ray.direction, dist);

  return dist;
}

void BLI_bvhtree_ray_cast_all_ex(const BVHTree *tree,
                                 const float co[3],
                                 const float dir[3],
                                 float radius,
                                 float hit_dist,
                                 BVHTree_RayCastCallback callback,
                                 void *userdata,
                                 int flag)
{
  BVHRayCastData data;
  BVHNode *root = tree->nodes[tree->leaf_num];

  BLI_ASSERT_UNIT_V3(dir);
  BLI_assert(callback != nullptr);

  data.tree = tree;

  data.callback = callback;
  data.userdata = userdata;

  copy_v3_v3(data.ray.origin, co);
  copy_v3_v3(data.ray.direction, dir);
  data.ray.radius = radius;

  bvhtree_ray_cast_data_precalc(&data, flag);

  data.hit.index = -1;
  data.hit.dist = hit_dist;

  if (root) {
    dfs_raycast_all(&data, root);
  }
}

void BLI_bvhtree_ray_cast_all(const BVHTree *tree,
                              const float co[3],
                              const float dir[3],
                              float radius,
                              float hit_dist,
                              BVHTree_RayCastCallback callback,
                              void *userdata)
{
  BLI_bvhtree_ray_cast_all_ex(
      tree, co, dir, radius, hit_dist, callback, userdata, BVH_RAYCAST_DEFAULT);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BLI_bvhtree_range_query
 *
 * Allocates and fills an array with the indices of node that are on the given spherical range
 * (center, radius).
 * Returns the size of the array.
 *
 * \{ */

struct RangeQueryData {
  const BVHTree *tree;
  const float *center;
  float radius_sq; /* squared radius */

  int hits;

  BVHTree_RangeQuery callback;
  void *userdata;
};

static void dfs_range_query(RangeQueryData *data, BVHNode *node)
{
  if (node->node_num == 0) {
#if 0 /*UNUSED*/
    /* Calculate the node min-coords
     * (if the node was a point then this is the point coordinates) */
    float co[3];
    co[0] = node->bv[0];
    co[1] = node->bv[2];
    co[2] = node->bv[4];
#endif
  }
  else {
    int i;
    for (i = 0; i != node->node_num; i++) {
      float nearest[3];
      float dist_sq = calc_nearest_point_squared(data->center, node->children[i], nearest);
      if (dist_sq < data->radius_sq) {
        /* Its a leaf.. call the callback */
        if (node->children[i]->node_num == 0) {
          data->hits++;
          data->callback(data->userdata, node->children[i]->index, data->center, dist_sq);
        }
        else {
          dfs_range_query(data, node->children[i]);
        }
      }
    }
  }
}

int BLI_bvhtree_range_query(const BVHTree *tree,
                            const float co[3],
                            float radius,
                            BVHTree_RangeQuery callback,
                            void *userdata)
{
  BVHNode *root = tree->nodes[tree->leaf_num];

  RangeQueryData data;
  data.tree = tree;
  data.center = co;
  data.radius_sq = radius * radius;
  data.hits = 0;

  data.callback = callback;
  data.userdata = userdata;

  if (root != nullptr) {
    float nearest[3];
    float dist_sq = calc_nearest_point_squared(data.center, root, nearest);
    if (dist_sq < data.radius_sq) {
      /* Its a leaf.. call the callback */
      if (root->node_num == 0) {
        data.hits++;
        data.callback(data.userdata, root->index, co, dist_sq);
      }
      else {
        dfs_range_query(&data, root);
      }
    }
  }

  return data.hits;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BLI_bvhtree_nearest_projected
 * \{ */

static void bvhtree_nearest_projected_dfs_recursive(BVHNearestProjectedData *__restrict data,
                                                    const BVHNode *node)
{
  if (node->node_num == 0) {
    if (data->callback) {
      data->callback(data->userdata, node->index, &data->precalc, nullptr, 0, &data->nearest);
    }
    else {
      data->nearest.index = node->index;
      data->nearest.dist_sq = dist_squared_to_projected_aabb(
          &data->precalc,
          blender::float3{node->bv[0], node->bv[2], node->bv[4]},
          blender::float3{node->bv[1], node->bv[3], node->bv[5]},
          data->closest_axis);
    }
  }
  else {
    /* First pick the closest node to recurse into */
    if (data->closest_axis[node->main_axis]) {
      for (int i = 0; i != node->node_num; i++) {
        const float *bv = node->children[i]->bv;

        if (dist_squared_to_projected_aabb(&data->precalc,
                                           blender::float3{bv[0], bv[2], bv[4]},
                                           blender::float3{bv[1], bv[3], bv[5]},
                                           data->closest_axis) <= data->nearest.dist_sq)
        {
          bvhtree_nearest_projected_dfs_recursive(data, node->children[i]);
        }
      }
    }
    else {
      for (int i = node->node_num; i--;) {
        const float *bv = node->children[i]->bv;

        if (dist_squared_to_projected_aabb(&data->precalc,
                                           blender::float3{bv[0], bv[2], bv[4]},
                                           blender::float3{bv[1], bv[3], bv[5]},
                                           data->closest_axis) <= data->nearest.dist_sq)
        {
          bvhtree_nearest_projected_dfs_recursive(data, node->children[i]);
        }
      }
    }
  }
}

static void bvhtree_nearest_projected_with_clipplane_test_dfs_recursive(
    BVHNearestProjectedData *__restrict data, const BVHNode *node)
{
  if (node->node_num == 0) {
    if (data->callback) {
      data->callback(data->userdata,
                     node->index,
                     &data->precalc,
                     data->clip_plane,
                     data->clip_plane_len,
                     &data->nearest);
    }
    else {
      data->nearest.index = node->index;
      data->nearest.dist_sq = dist_squared_to_projected_aabb(
          &data->precalc,
          blender::float3{node->bv[0], node->bv[2], node->bv[4]},
          blender::float3{node->bv[1], node->bv[3], node->bv[5]},
          data->closest_axis);
    }
  }
  else {
    /* First pick the closest node to recurse into */
    if (data->closest_axis[node->main_axis]) {
      for (int i = 0; i != node->node_num; i++) {
        const float *bv = node->children[i]->bv;
        const float bb_min[3] = {bv[0], bv[2], bv[4]};
        const float bb_max[3] = {bv[1], bv[3], bv[5]};

        int isect_type = isect_aabb_planes_v3(
            data->clip_plane, data->clip_plane_len, bb_min, bb_max);

        if ((isect_type != ISECT_AABB_PLANE_BEHIND_ANY) &&
            dist_squared_to_projected_aabb(&data->precalc, bb_min, bb_max, data->closest_axis) <=
                data->nearest.dist_sq)
        {
          if (isect_type == ISECT_AABB_PLANE_CROSS_ANY) {
            bvhtree_nearest_projected_with_clipplane_test_dfs_recursive(data, node->children[i]);
          }
          else {
            /* ISECT_AABB_PLANE_IN_FRONT_ALL */
            bvhtree_nearest_projected_dfs_recursive(data, node->children[i]);
          }
        }
      }
    }
    else {
      for (int i = node->node_num; i--;) {
        const float *bv = node->children[i]->bv;
        const float bb_min[3] = {bv[0], bv[2], bv[4]};
        const float bb_max[3] = {bv[1], bv[3], bv[5]};

        int isect_type = isect_aabb_planes_v3(
            data->clip_plane, data->clip_plane_len, bb_min, bb_max);

        if (isect_type != ISECT_AABB_PLANE_BEHIND_ANY &&
            dist_squared_to_projected_aabb(&data->precalc, bb_min, bb_max, data->closest_axis) <=
                data->nearest.dist_sq)
        {
          if (isect_type == ISECT_AABB_PLANE_CROSS_ANY) {
            bvhtree_nearest_projected_with_clipplane_test_dfs_recursive(data, node->children[i]);
          }
          else {
            /* ISECT_AABB_PLANE_IN_FRONT_ALL */
            bvhtree_nearest_projected_dfs_recursive(data, node->children[i]);
          }
        }
      }
    }
  }
}

int BLI_bvhtree_find_nearest_projected(const BVHTree *tree,
                                       float projmat[4][4],
                                       float winsize[2],
                                       float mval[2],
                                       float (*clip_plane)[4],
                                       int clip_plane_len,
                                       BVHTreeNearest *nearest,
                                       BVHTree_NearestProjectedCallback callback,
                                       void *userdata)
{
  const BVHNode *root = tree->nodes[tree->leaf_num];
  if (root != nullptr) {
    BVHNearestProjectedData *data = (BVHNearestProjectedData *)alloca(
        sizeof(*data) + (sizeof(*clip_plane) * size_t(max_ii(1, clip_plane_len))));

    dist_squared_to_projected_aabb_precalc(&data->precalc, projmat, winsize, mval);

    data->callback = callback;
    data->userdata = userdata;

#ifdef __GNUC__ /* Invalid `data->clip_plane` warning with GCC 14.2.1. */
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Warray-bounds"
#endif
    if (clip_plane) {
      data->clip_plane_len = clip_plane_len;
      for (int i = 0; i < clip_plane_len; i++) {
        copy_v4_v4(data->clip_plane[i], clip_plane[i]);
      }
    }
    else {
      data->clip_plane_len = 1;
      planes_from_projmat(
          projmat, nullptr, nullptr, nullptr, nullptr, data->clip_plane[0], nullptr);
    }
#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

    if (nearest) {
      memcpy(&data->nearest, nearest, sizeof(*nearest));
    }
    else {
      data->nearest.index = -1;
      data->nearest.dist_sq = FLT_MAX;
    }
    {
      const float bb_min[3] = {root->bv[0], root->bv[2], root->bv[4]};
      const float bb_max[3] = {root->bv[1], root->bv[3], root->bv[5]};

      int isect_type = isect_aabb_planes_v3(
          data->clip_plane, data->clip_plane_len, bb_min, bb_max);

      if (isect_type != 0 &&
          dist_squared_to_projected_aabb(&data->precalc, bb_min, bb_max, data->closest_axis) <=
              data->nearest.dist_sq)
      {
        if (isect_type == 1) {
          bvhtree_nearest_projected_with_clipplane_test_dfs_recursive(data, root);
        }
        else {
          bvhtree_nearest_projected_dfs_recursive(data, root);
        }
      }
    }

    if (nearest) {
      memcpy(nearest, &data->nearest, sizeof(*nearest));
    }

    return data->nearest.index;
  }
  return -1;
}

/** \} */
