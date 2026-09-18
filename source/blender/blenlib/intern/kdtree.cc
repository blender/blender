/* SPDX-FileCopyrightText: 2008-2009 Marius Muja, David G. Lowe.
 * SPDX-FileCopyrightText: 2011-2026 Jose Luis Blanco-Claraco.
 * SPDX-FileCopyrightText: 2026 Blender Authors.
 *
 * SPDX-License-Identifier: BSD-2-Clause */

/** \file
 * \ingroup bli
 *
 * A KD-tree for nearest neighbor search, which started from the nanoflann library and has since
 * been rewritten around Blender's own containers, threading and math. The node layout and the way
 * a search walks it are still recognizably nanoflann's, which is where the copyright above comes
 * from.
 */

#include <algorithm>
#include <bit>
#include <cmath>
#include <numeric>
#include <optional>

#include "BLI_array.hh"
#include "BLI_bounds.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_index_mask.hh"
#include "BLI_kdtree_new.hh"
#include "BLI_linear_allocator.hh"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"
#include "BLI_threads.hh"

namespace blender {

/* -------------------------------------------------------------------- */
/** \name Build Tuning
 * \{ */

/** Tree size that makes building with more than one thread worthwhile. */
static constexpr int PARALLEL_BUILD_THRESHOLD = 10000;

/** Threads used to build a tree of #PARALLEL_BUILD_THRESHOLD points. */
static constexpr int PARALLEL_BUILD_THRESHOLD_THREADS = 4;

/** Subtrees smaller than this are built in one task, which is cheaper than handing them over. */
static constexpr int PARALLEL_SUBTREE_GRAIN = 4096;

/** Points that make a block worth giving a thread of its own within a single node. */
static constexpr int NODE_BLOCK_SIZE = 8192;

/** Upper bound on those blocks, so that the bookkeeping fits on the stack. */
static constexpr int NODE_MAX_BLOCKS = 64;

/**
 * Nodes holding more than this fraction of the tree are split a block at a time, which is what a
 * threaded build spreads over its threads. Below that there are enough subtrees to keep the
 * threads busy on their own, and the blocks and the fix up they need are only extra work.
 */
static constexpr int NODE_BLOCK_FRACTION = 8;

/**
 * Points a partition works on at a time. An offset within a block has to fit in a #uint8_t, and
 * two blocks have to fit in a range for the block path to be used at all.
 */
static constexpr int PARTITION_BLOCK_SIZE = 128;

/**
 * The best number of threads to build a tree depends on the memory bandwidth relative to the
 * amount of computational work necessary. Building repeatedly reorders coordinates and indices so
 * it is heavily bottlenecked by memory bandwidth at high point counts. We want to be relatively
 * conservative with the thread count as well; the CPU may be better off spending time on other
 * less memory-bound tasks.
 *
 * This fourth root relationship between increases in point count and threads used is tuned for a
 * 16 core Ryzen 7950X: a million point build takes 25 ms on one thread and 3.5 ms on eight, past
 * which it stops improving, while the processor time it uses keeps growing with every thread. See
 * `BLI_kdtree_performance_test`.
 */
static int parallel_build_threads(const int64_t points_num)
{
  const double growth = std::sqrt(
      std::sqrt(double(points_num) / double(PARALLEL_BUILD_THRESHOLD)));
  return std::max(int(std::round(double(PARALLEL_BUILD_THRESHOLD_THREADS) * growth)), 1);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tree
 * \{ */

template<typename T> class TreeBuilder {
 public:
  using value_type = typename T::base_type;
  static constexpr int dimensions = T::type_length;

 private:
  /**
   * A point as the build holds it: its coordinates, and its index in #points_ in the element
   * after them.
   */
  using BuildPoint = VecBase<value_type, dimensions + 1>;

  using Node = kdtree::Node<value_type>;

  /**
   * Nodes are handed out from batches, so that a walk down the tree stays within a few
   * allocations. A linear allocator packs small allocations into buffers of a few kilobytes at
   * most, which for a tree of any size means nodes scattered over many of them.
   */
  struct NodeAllocator {
    /**
     * Most nodes per batch, and so the most that can go unused per thread that built a subtree.
     * The first batch of a build is sized from the number of points instead, so that a small tree
     * does not reserve room for a large one. See #node_batch_size.
     */
    static constexpr int BATCH_SIZE_MAX = 256;

    int batch_size = BATCH_SIZE_MAX;
    /**
     * Where the batches come from. A pool that a thread of its own fills has to have one to
     * itself, while the pool of a build that stays on one thread takes them straight from the
     * tree's allocator, so that its nodes land in the same buffer as its indices.
     */
    LinearAllocator<> owned_allocator;
    LinearAllocator<> *allocator = &owned_allocator;
    MutableSpan<Node> batch;
    /* Equal to the (empty) batch size, so that the first allocation starts a batch. */
    int used = 0;

    /** A node, or the two children of one, which have to be next to each other. */
    Node *allocate(const int num)
    {
      if (used + num > batch.size()) {
        this->batch = this->allocator->allocate_array<Node>(batch_size);
        this->used = 0;
        /* Only the first batch is sized for the tree, the rest are full size. */
        this->batch_size = BATCH_SIZE_MAX;
      }
      Node *nodes = &this->batch[used];
      this->used += num;
      return nodes;
    }
  };

 public:
  /**
   * The approximate number of nodes a tree with \a points_num points takes. Leaves hold a bit over
   * two thirds of #leaf_size_ points on average, and a tree has one interior node fewer than it
   * has leaves. Measured at 22.6 points per leaf for a leaf size of 32. See
   * `BLI_kdtree_performance_test`.
   */
  static int node_batch_size(const int points_num, const int leaf_size)
  {
    const int64_t leaves = std::max<int64_t>(1, points_num * 3 / (int64_t(leaf_size) * 2));
    return int(std::min<int64_t>(2 * leaves + 1, NodeAllocator::BATCH_SIZE_MAX));
  }

 private:
  const Span<T> points_;
  /** Filled with the indices of the points, in the order the tree visits them. */
  const MutableSpan<int> indices_;
  const int leaf_size_;
  /** The buffers the nodes end up in, which the pools below hand theirs to. */
  LinearAllocator<> &allocator_;

  /**
   * The points being reordered, which #indices_ is taken from once the tree is built. Only set
   * while #build_nodes runs, which is the only thing that needs it.
   */
  MutableSpan<BuildPoint> build_points_;
  /** The pool of a build that stays on one thread. */
  NodeAllocator pool_;
  /**
   * A pool per thread that takes part in a threaded build. One pool behind a mutex would put
   * every node in the tree through it, while a pool per subtree leaves a partly used batch
   * behind for each of them and spreads the nodes of a subtree over several pools. Only built
   * when there are threads to give pools to: reaching into thread local storage costs more than
   * building a tree of a few points does.
   */
  std::optional<threading::EnumerableThreadSpecific<NodeAllocator>> thread_allocators_;

 public:
  TreeBuilder(const Span<T> points,
              const MutableSpan<int> indices,
              const int leaf_size,
              LinearAllocator<> &allocator)
      : points_(points), indices_(indices), leaf_size_(leaf_size), allocator_(allocator)
  {
  }

  Node *build(const Span<int> indices, Bounds<T> &r_bounds)
  {
    const int size = int(indices_.size());
    BLI_assert(size > 0);

    /* A tree that is a single leaf has nothing to split, and copying the coordinates aside would
     * cost more than the rest of its build. */
    if (size <= leaf_size_) {
      return this->build_single_leaf(indices, r_bounds);
    }

    /* Splitting the build into tasks costs something even when there is no second thread to run
     * them, so only ask for it when there is one. */
    const bool threaded = size >= PARALLEL_BUILD_THRESHOLD && BLI_system_thread_count() > 1;
    Node *root = nullptr;
    if (threaded) {
      threading::max_threads_task(parallel_build_threads(size),
                                  [&]() { root = this->build_nodes(indices, r_bounds, true); });
    }
    else {
      root = this->build_nodes(indices, r_bounds, false);
    }
    return root;
  }

 private:
  /* -------------------------------------------------------------------- */
  /** \name Build
   * \{ */

  Node *build_single_leaf(const Span<int> indices, Bounds<T> &r_bounds)
  {
    if (indices.is_empty()) {
      std::iota(indices_.begin(), indices_.end(), 0);
    }
    else if (indices.data() != indices_.data()) {
      indices_.copy_from(indices);
    }

    r_bounds = Bounds<T>(points_[indices_.first()]);
    for (const int index : indices_) {
      math::min_max(points_[index], r_bounds.min, r_bounds.max);
    }

    Node *root = allocator_.allocate<Node>();
    root->children = nullptr;
    root->leaf.start = 0;
    root->leaf.end = int(indices_.size());
    return root;
  }

  Node *build_nodes(const Span<int> indices, Bounds<T> &r_bounds, const bool threaded)
  {
    const int size = int(indices_.size());
    Array<BuildPoint, 256> build_points(size);

    build_points_ = build_points.as_mutable_span();
    const Bounds<BuildPoint> bounds = this->init_points(indices, threaded);

    pool_.batch_size = node_batch_size(size, leaf_size_);
    pool_.allocator = &allocator_;
    if (threaded) {
      thread_allocators_.emplace();
    }
    NodeAllocator &allocator = threaded ? thread_allocators_->local() : pool_;
    Node *root = allocator.allocate(1);
    this->build_node(root, 0, size, bounds, allocator, threaded);

    /* The pool of the build itself took its batches from #allocator_ already, only the ones the
     * threads filled have buffers of their own to hand over. */
    if (thread_allocators_.has_value()) {
      for (NodeAllocator &allocator : *thread_allocators_) {
        allocator_.transfer_ownership_from(allocator.owned_allocator);
      }
    }

    for (const int dim : IndexRange(dimensions)) {
      r_bounds.min[dim] = bounds.min[dim];
      r_bounds.max[dim] = bounds.max[dim];
    }

    /* The points are in the order the tree visits them now, so the indices they carry are the
     * order to keep. The coordinates are not, since a search reads #points_. */
    this->collect_indices(threaded);
    return root;
  }

  /**
   * The build reorders a copy of the coordinates rather than an array of indices. That makes every
   * pass over a node sequential, and lets subtrees become cache resident as soon as they are small
   * enough. Each point carries its index in the element after its coordinates, so reordering the
   * points reorders the indices with them, in the same cache lines rather than in a second array.
   */
  Bounds<BuildPoint> init_points(const Span<int> indices, const bool threaded)
  {
    const int size = int(indices_.size());
    const auto fill = [&](const int start, const int end) {
      Bounds<BuildPoint> bounds(this->build_point(indices, start));
      for (const int i : IndexRange(start, end - start)) {
        const BuildPoint point = this->build_point(indices, i);
        build_points_[i] = point;
        bounds.min = math::min(bounds.min, point);
        bounds.max = math::max(bounds.max, point);
      }
      return bounds;
    };

    const int blocks = threaded ? block_num(size) : 0;
    if (blocks < 2) {
      return fill(0, size);
    }

    Array<Bounds<BuildPoint>, NODE_MAX_BLOCKS> block_bounds(blocks);
    threading::parallel_for(IndexRange(blocks), 1, [&](const IndexRange range) {
      for (const int block : range) {
        block_bounds[block] = fill(block_begin(size, blocks, block),
                                   block_begin(size, blocks, block + 1));
      }
    });
    return merge_bounds(block_bounds);
  }

  BuildPoint build_point(const Span<int> indices, const int i) const
  {
    const int index = indices.is_empty() ? i : indices[i];
    BuildPoint point;
    for (const int dim : IndexRange(dimensions)) {
      point[dim] = points_[index][dim];
    }
    point[dimensions] = std::bit_cast<value_type>(index);
    return point;
  }

  /** Take the index each point carries, in the order the build left them in. */
  void collect_indices(const bool threaded)
  {
    const int size = int(indices_.size());
    const auto fill = [&](const int start, const int end) {
      for (const int i : IndexRange(start, end - start)) {
        indices_[i] = std::bit_cast<int>(build_points_[i][dimensions]);
      }
    };

    const int blocks = threaded ? block_num(size) : 0;
    if (blocks < 2) {
      fill(0, size);
      return;
    }
    threading::parallel_for(IndexRange(blocks), 1, [&](const IndexRange range) {
      for (const int block : range) {
        fill(block_begin(size, blocks, block), block_begin(size, blocks, block + 1));
      }
    });
  }

  /**
   * Build the node covering [start, end), of whose points \a bounds must be the exact bounds.
   * Splitting around the middle of those rather than of a cell that can be much larger keeps the
   * tree shallow where the points are unevenly spread.
   */
  void build_node(Node *node,
                  const int start,
                  const int end,
                  const Bounds<BuildPoint> &bounds,
                  NodeAllocator &allocator,
                  const bool threaded)
  {
    if (end - start <= leaf_size_) {
      node->children = nullptr;
      node->leaf.start = start;
      node->leaf.end = end;
      return;
    }

    /* Split the widest extent of the points in half. */
    const BuildPoint extent = bounds.max - bounds.min;
    int dim = 0;
    for (const int other : IndexRange(1, dimensions - 1)) {
      if (extent[other] > extent[dim]) {
        dim = other;
      }
    }
    const value_type split_value = bounds.min[dim] + extent[dim] / 2;

    /* Split the node a block at a time, which a threaded build spreads over its threads. For
     * determinism, only the range size is used to check whether to use the blocked partitioning,
     * not whether threading happens to be enabled. Below this size there are enough subtrees to
     * keep the threads busy on their own, and the blocks would only be extra work. */
    const bool split_blocks = int64_t(end - start) * NODE_BLOCK_FRACTION > build_points_.size();

    int middle = split_blocks ? this->partition_points_in_blocks(start, end, dim, split_value) :
                                partition_points(build_points_, start, end, dim, split_value);
    if (middle == start || middle == end) {
      /* Everything ended up on one side, which means the coordinates of the points are within
       * rounding distance of each other in every dimension. Split them in the middle so that the
       * recursion still terminates. */
      middle = start + (end - start) / 2;
    }

    Bounds<BuildPoint> left_bounds, right_bounds;
    if (split_blocks) {
      left_bounds = this->compute_bounds_in_blocks(start, middle);
      right_bounds = this->compute_bounds_in_blocks(middle, end);
    }
    else {
      left_bounds = compute_bounds(build_points_, start, middle);
      right_bounds = compute_bounds(build_points_, middle, end);
    }

    node->dim = dim;
    node->children = allocator.allocate(2);
    if (threaded && end - start > PARALLEL_SUBTREE_GRAIN) {
      threading::parallel_invoke(
          [&]() {
            this->build_node(
                &node->children[0], start, middle, left_bounds, thread_allocators_->local(), true);
          },
          [&]() {
            this->build_node(
                &node->children[1], middle, end, right_bounds, thread_allocators_->local(), true);
          });
    }
    else {
      this->build_node(&node->children[0], start, middle, left_bounds, allocator, false);
      this->build_node(&node->children[1], middle, end, right_bounds, allocator, false);
    }

    node->split.low = left_bounds.max[dim];
    node->split.high = right_bounds.min[dim];
  }

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Bounds
   * \{ */

  static Bounds<BuildPoint> compute_bounds(const Span<BuildPoint> points,
                                           const int start,
                                           const int end)
  {
    /* Straight to the loop rather than through #bounds::min_max: this runs twice per interior
     * node, and the profiler scope that one opens costs more than the bounds of a small node. */
    return bounds::detail::min_max_lanes(points.slice(start, end - start));
  }

  /** #compute_bounds with the range measured a block at a time, which threads can share. */
  Bounds<BuildPoint> compute_bounds_in_blocks(const int start, const int end)
  {
    const int count = end - start;
    const int blocks = block_num(count);
    if (blocks < 2) {
      return compute_bounds(build_points_, start, end);
    }

    Array<Bounds<BuildPoint>, NODE_MAX_BLOCKS> block_bounds(blocks);
    threading::parallel_for(IndexRange(blocks), 1, [&](const IndexRange range) {
      for (const int block : range) {
        block_bounds[block] = compute_bounds(build_points_,
                                             start + block_begin(count, blocks, block),
                                             start + block_begin(count, blocks, block + 1));
      }
    });
    return merge_bounds(block_bounds);
  }

  static Bounds<BuildPoint> merge_bounds(const Span<Bounds<BuildPoint>> block_bounds)
  {
    Bounds<BuildPoint> bounds = block_bounds.first();
    for (const Bounds<BuildPoint> &other : block_bounds.drop_front(1)) {
      bounds.min = math::min(bounds.min, other.min);
      bounds.max = math::max(bounds.max, other.max);
    }
    return bounds;
  }

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Partitioning
   * \{ */

  /**
   * Reorder the points in [start, end) so that those below \a split_value in \a dim come first,
   * and return where the rest starts.
   *
   * Deciding per point whether to swap it is a branch on the result of the comparison, which for
   * an evenly split node is unpredictable and so mispredicts on about every other point. This
   * collects the offsets of the misplaced points of a block without branching on the comparison,
   * and only then swaps them, which is a loop over exactly as many iterations as there are swaps.
   * Same approach as the partition in BlockQuicksort.
   */
  static int partition_points(const MutableSpan<BuildPoint> points,
                              const int start,
                              const int end,
                              const int dim,
                              const value_type split_value)
  {
    constexpr int block = PARTITION_BLOCK_SIZE;

    uint8_t offsets_l[block], offsets_r[block];
    int num_l = 0, num_r = 0, first_l = 0, first_r = 0;
    int l = start, r = end;

    while (r - l > 2 * block) {
      /* Offsets of the points of the left block that belong on the right, and the other way
       * around. The store always happens, only the counter advances conditionally. */
      if (num_l == 0) {
        first_l = 0;
        for (const int i : IndexRange(block)) {
          offsets_l[num_l] = uint8_t(i);
          num_l += int(!(points[l + i][dim] < split_value));
        }
      }
      if (num_r == 0) {
        first_r = 0;
        for (const int i : IndexRange(block)) {
          offsets_r[num_r] = uint8_t(i);
          num_r += int(points[r - 1 - i][dim] < split_value);
        }
      }

      const int num = std::min(num_l, num_r);
      for (const int i : IndexRange(num)) {
        std::swap(points[l + offsets_l[first_l + i]], points[r - 1 - offsets_r[first_r + i]]);
      }
      num_l -= num;
      num_r -= num;
      first_l += num;
      first_r += num;

      /* A block is done once none of its points are misplaced any more. */
      if (num_l == 0) {
        l += block;
      }
      if (num_r == 0) {
        r -= block;
      }
    }

    return partition_points_small(points, l, r, dim, split_value);
  }

  /**
   * #partition_points for a range that fits in a temporary buffer, which is the case for the
   * last levels of the tree. Copying the points out and back in is cheaper than the swaps that
   * staying in place needs, and keeps those levels branchless as well.
   */
  static int partition_points_small(const MutableSpan<BuildPoint> points,
                                    const int start,
                                    const int end,
                                    const int dim,
                                    const value_type split_value)
  {
    const int count = end - start;
    BLI_assert(count <= 2 * PARTITION_BLOCK_SIZE);

    std::array<BuildPoint, 2 * PARTITION_BLOCK_SIZE> buffer;
    /* Points below the split fill the buffer from the front, the others from the back. Only the
     * position depends on the comparison, so there is nothing to mispredict. */
    int l = 0, r = count;
    for (const int i : IndexRange(start, count)) {
      /* Read the coordinate from the array rather than from a copy of the point: indexing a
       * vector by a value only known at runtime means it has to be in memory, and a copy would
       * have to be spilled there every iteration. */
      const bool less = points[i][dim] < split_value;
      const BuildPoint point = points[i];
      /* Written to both ends, of which only one is kept. Picking the position instead lets a
       * compiler turn it back into a branch on the comparison, which is the whole thing this
       * avoids, and both ends are a few cache lines apart at most. */
      buffer[l] = point;
      buffer[r - 1] = point;
      l += int(less);
      r -= int(!less);
    }

    MutableSpan<BuildPoint> range = points.slice(start, count);
    for (const int i : IndexRange(count)) {
      range[i] = buffer[i];
    }
    return start + l;
  }

  /**
   * #partition_points with the range split a block at a time, which can be spread over threads.
   * Which blocks the range is cut into follows from its size alone, so this leaves the points in
   * the same order however many threads run it.
   *
   * Each block is partitioned on its own, which leaves every block split but the blocks
   * themselves interleaved: points above the split sit in blocks that belong entirely below it,
   * and the other way around. There are as many of one as of the other, so the fix up is to swap
   * them pairwise, which spreads over threads as well. This is how embree partitions primitives
   * when building a BVH.
   */
  int partition_points_in_blocks(const int start,
                                 const int end,
                                 const int dim,
                                 const value_type split_value)
  {
    const MutableSpan<BuildPoint> points = build_points_;
    const int count = end - start;
    const int blocks = block_num(count);
    if (blocks < 2) {
      return partition_points(points, start, end, dim, split_value);
    }

    int begin[NODE_MAX_BLOCKS + 1];
    int middle[NODE_MAX_BLOCKS];
    for (const int block : IndexRange(blocks + 1)) {
      begin[block] = start + block_begin(count, blocks, block);
    }

    threading::parallel_for(IndexRange(blocks), 1, [&](const IndexRange range) {
      for (const int block : range) {
        middle[block] = partition_points(points, begin[block], begin[block + 1], dim, split_value);
      }
    });

    /* Where the split ends up once the blocks are put in order. */
    int split = start;
    for (const int block : IndexRange(blocks)) {
      split += middle[block] - begin[block];
    }

    /* The points that are on the wrong side of it. A block lies either before or after the
     * split, so it contributes to one of the two lists at most. */
    IndexRange high[NODE_MAX_BLOCKS], low[NODE_MAX_BLOCKS];
    int high_num = 0, low_num = 0;
    for (const int block : IndexRange(blocks)) {
      if (middle[block] < split) {
        const int last = std::min(begin[block + 1], split);
        if (middle[block] < last) {
          high[high_num++] = IndexRange::from_begin_end(middle[block], last);
        }
      }
      else if (middle[block] > split) {
        const int first = std::max(begin[block], split);
        if (first < middle[block]) {
          low[low_num++] = IndexRange::from_begin_end(first, middle[block]);
        }
      }
    }

    /* Pair the two lists up into chunks that can be swapped independently. Each step finishes a
     * range on one side or the other, so there are fewer chunks than there are ranges. */
    struct Swap {
      int from;
      int to;
      int num;
    };
    Swap swaps[2 * NODE_MAX_BLOCKS];
    int swaps_num = 0;
    for (int h = 0, l = 0, h_done = 0, l_done = 0; h < high_num && l < low_num;) {
      const int num = std::min(int(high[h].size()) - h_done, int(low[l].size()) - l_done);
      swaps[swaps_num++] = {int(high[h].start()) + h_done, int(low[l].start()) + l_done, num};
      h_done += num;
      l_done += num;
      if (h_done == int(high[h].size())) {
        h++;
        h_done = 0;
      }
      if (l_done == int(low[l].size())) {
        l++;
        l_done = 0;
      }
    }

    threading::parallel_for(IndexRange(swaps_num), 1, [&](const IndexRange range) {
      for (const int i : range) {
        const Swap &swap = swaps[i];
        for (const int offset : IndexRange(swap.num)) {
          std::swap(points[swap.from + offset], points[swap.to + offset]);
        }
      }
    });

    return split;
  }

  static int block_num(const int count)
  {
    return std::min(count / NODE_BLOCK_SIZE, NODE_MAX_BLOCKS);
  }

  static int block_begin(const int count, const int blocks, const int block)
  {
    return int(int64_t(count) * block / blocks);
  }

  /** \} */
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name #KDTreeNew
 * \{ */

template<typename T>
void KDTreeNew<T>::build(const Span<T> points,
                         const Span<int> indices,
                         const IndexMask *mask,
                         const int size,
                         const int leaf_size,
                         LinearAllocator<> &allocator)
{
  points_ = points.data();
  if (size == 0) {
    return;
  }
  const MutableSpan<int> tree_indices = allocator.allocate_array<int>(size);

  Span<int> source;
  if (mask) {
    mask->to_indices<int>(tree_indices);
    source = tree_indices;
  }
  else {
    source = indices;
  }

  TreeBuilder<T> builder(points, tree_indices, leaf_size, allocator);
  root_ = builder.build(source, bounds_);
  tree_indices_ = tree_indices.data();
  size_ = size;

#ifndef NDEBUG
  /* Only the pointer to the points is kept, so nothing can check a search result against their
   * number later. Everything the tree can return comes from here. */
  for (const int index : tree_indices) {
    BLI_assert(index >= 0 && index < points.size());
  }
#endif
}

template<typename T>
LinearAllocator<> &KDTreeNew<T>::make_allocator(const int points_num,
                                                const int leaf_size,
                                                void *&r_owned)
{
  /* Allocate enough for the allocator itself, the indices, and the first set of nodes. */
  const int64_t indices_size = int64_t(points_num) * sizeof(int);
  const int64_t nodes_size = int64_t(TreeBuilder<T>::node_batch_size(points_num, leaf_size)) *
                             sizeof(Node);
  /* Room for the allocator to align each of the two allocations it hands out of this. */
  const int64_t alignment_slack = 2 * alignof(std::max_align_t);
  const int64_t rest = indices_size + nodes_size + alignment_slack;
  const int64_t size = sizeof(LinearAllocator<>) + rest;

  void *buffer = MEM_new_uninitialized_aligned(size, alignof(LinearAllocator<>), __func__);
  r_owned = buffer;
  LinearAllocator<> *allocator = new (buffer) LinearAllocator<>();
  allocator->provide_buffer(static_cast<char *>(buffer) + sizeof(LinearAllocator<>), rest);
  return *allocator;
}

template<typename T> KDTreeNew<T>::~KDTreeNew()
{
  /* An allocator the caller passed is only borrowed, and outlives the tree. The tree's own sits
   * at the start of the buffer it was built in. */
  if (owned_buffer_ != nullptr) {
    std::destroy_at(static_cast<LinearAllocator<> *>(owned_buffer_));
    MEM_delete_void(owned_buffer_);
  }
}

template<typename T>
KDTreeNew<T>::KDTreeNew(const Span<T> points, LinearAllocator<> &allocator, const int leaf_size)
{
  this->build(points, Span<int>(), nullptr, int(points.size()), std::max(leaf_size, 1), allocator);
}

template<typename T>
KDTreeNew<T>::KDTreeNew(const Span<T> points,
                        const IndexMask &mask,
                        LinearAllocator<> &allocator,
                        const int leaf_size)
{
  const std::optional<IndexRange> range = mask.to_range();
  const bool every_point = range && *range == points.index_range();
  /* Avoid the indirection when the mask contains every point anyway. */
  this->build(points,
              Span<int>(),
              every_point ? nullptr : &mask,
              int(mask.size()),
              std::max(leaf_size, 1),
              allocator);
}

template<typename T>
KDTreeNew<T>::KDTreeNew(const Span<T> points,
                        const Span<int> indices,
                        LinearAllocator<> &allocator,
                        const int leaf_size)
{
  this->build(points, indices, nullptr, int(indices.size()), std::max(leaf_size, 1), allocator);
}

template<typename T> KDTreeNew<T>::KDTreeNew(const Span<T> points, const int leaf_size)
{
  this->build(points,
              Span<int>(),
              nullptr,
              int(points.size()),
              std::max(leaf_size, 1),
              make_allocator(int(points.size()), std::max(leaf_size, 1), owned_buffer_));
}

template<typename T>
KDTreeNew<T>::KDTreeNew(const Span<T> points, const IndexMask &mask, const int leaf_size)
{
  const std::optional<IndexRange> range = mask.to_range();
  const bool every_point = range && *range == points.index_range();
  this->build(points,
              Span<int>(),
              every_point ? nullptr : &mask,
              int(mask.size()),
              std::max(leaf_size, 1),
              make_allocator(int(mask.size()), std::max(leaf_size, 1), owned_buffer_));
}

template<typename T>
KDTreeNew<T>::KDTreeNew(const Span<T> points, const Span<int> indices, const int leaf_size)
{
  this->build(points,
              indices,
              nullptr,
              int(indices.size()),
              std::max(leaf_size, 1),
              make_allocator(int(indices.size()), std::max(leaf_size, 1), owned_buffer_));
}

template class KDTreeNew<float2>;
template class KDTreeNew<float3>;

/** \} */

/* -------------------------------------------------------------------- */
/** \name #calc_duplicates
 * \{ */

namespace kdtree {

template<typename T, typename ForeachFn>
static int calc_duplicates_impl(const KDTreeNew<T> &tree,
                                const typename T::base_type range,
                                const MutableSpan<int> duplicates,
                                const ForeachFn foreach_index)
{
  using value_type = typename T::base_type;
  const T *points = tree.points();
  int found = 0;
  foreach_index([&](const int index) {
    BLI_assert(index < duplicates.size());
    /* Skip points that are already merged into another point. Points that were preset to their
     * own index are not merged themselves, but can still be merged into. */
    if (duplicates[index] != -1 && duplicates[index] != index) {
      return;
    }
    const int found_before = found;
    tree.foreach_in_radius(
        points[index], range, [&](const int other, const value_type /*distance_sq*/) {
          if (other != index && duplicates[other] == -1) {
            duplicates[other] = index;
            found++;
          }
        });
    if (found != found_before) {
      /* Mark as a target so the merged points don't form chains. */
      duplicates[index] = index;
    }
  });
  return found;
}

template<typename T>
int calc_duplicates(const KDTreeNew<T> &tree,
                    const typename T::base_type range,
                    const Span<int> visit_order,
                    const MutableSpan<int> duplicates)
{
  BLI_assert(visit_order.size() == tree.size());
  return calc_duplicates_impl(tree, range, duplicates, [&](const auto fn) {
    for (const int index : visit_order) {
      fn(index);
    }
  });
}

template<typename T>
int calc_duplicates(const KDTreeNew<T> &tree,
                    const typename T::base_type range,
                    const IndexMask &visit_order,
                    const MutableSpan<int> duplicates)
{
  BLI_assert(visit_order.size() == tree.size());
  return calc_duplicates_impl(tree, range, duplicates, [&](const auto fn) {
    visit_order.foreach_index_optimized<int>(fn);
  });
}

template int calc_duplicates(const KDTreeNew<float2> &, float, Span<int>, MutableSpan<int>);
template int calc_duplicates(const KDTreeNew<float3> &, float, Span<int>, MutableSpan<int>);
template int calc_duplicates(const KDTreeNew<float2> &,
                             float,
                             const IndexMask &,
                             MutableSpan<int>);
template int calc_duplicates(const KDTreeNew<float3> &,
                             float,
                             const IndexMask &,
                             MutableSpan<int>);

}  // namespace kdtree

/** \} */

}  // namespace blender
