/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Expression evaluation has multiple phases:
 * 1. A coarse evaluation that tries to find segments which can be trivially evaluated. For
 *    example, taking the union of two overlapping ranges can be done in O(1) time.
 * 2. For all segments which can't be fully evaluated using coarse evaluation, an exact evaluation
 *    is done. This uses either an index-based or bit-based approach depending on a heuristic.
 * 3. Construct the final index mask based on the resulting intermediate segments.
 */

#include "BLI_array.hh"
#include "BLI_bit_group_vector.hh"
#include "BLI_bit_span_ops.hh"
#include "BLI_enumerable_thread_specific.hh"
#include "BLI_index_mask_expression.hh"
#include "BLI_stack.hh"
#include "BLI_strict_flags.h"
#include "BLI_task.hh"
#include "BLI_timeit.hh"

namespace blender::index_mask {

/**
 * Number of expression terms which don't require extra allocations in some places.
 */
constexpr int64_t inline_expr_array_size = 16;

/**
 * The result of the coarse evaluation for a specific index range.
 */
struct CoarseSegment {
  enum class Type {
    /**
     * Coarse evaluation couldn't fully resolve this segment. The segment requires another
     * evaluation that is more detailed.
     */
    Unknown,
    /** All indices in the segment are part of the result. */
    Full,
    /** The evaluated result of this segment is just the copy of an input index mask. */
    Copy,
  };
  Type type = Type::Unknown;
  IndexRange bounds;
  /** Mask used when the type is #Copy. */
  const IndexMask *mask = nullptr;
};

/** Contains the result of a coarse evaluation split into potentially many segments. */
struct CoarseResult {
  Vector<CoarseSegment> segments;
};

/** Used during coarse evaluation to split the full range into multiple segments. */
struct CourseBoundary {
  /**
   * The position of the boundary. The boundary is right before this index. So if this boundary is
   * a beginning of a segment, the index marks the first element. If it is the end, the index marks
   * the one-after-last position.
   */
  int64_t index;
  /** Whether this boundary is the beginning or end of the segment below. */
  bool is_begin;
  /** The segment this boundary comes from. */
  const CoarseSegment *segment;
};

/** For the difference operation, we need to know if a boundary belongs to the main term or not. */
struct DifferenceCourseBoundary : public CourseBoundary {
  bool is_main;
};

/**
 * Result of the expression evaluation within a specific index range. Sometimes this can be derived
 * directly from the coarse evaluation, but sometimes an additional exact evaluation is necessary.
 */
struct EvaluatedSegment {
  enum class Type {
    /** All indices in this segment are part of the evaluated index mask. */
    Full,
    /** The result in this segment is the same as what is contained in the #copy_mask below. */
    Copy,
    /** The result comes from exact evaluation and is a new set of indices. */
    Indices,
  };

  Type type = Type::Indices;
  IndexRange bounds;
  /** Only used when the type is #Type::Copy. */
  const IndexMask *copy_mask = nullptr;
  /** Only used when the type is #Type::Indices. */
  IndexMaskSegment indices;
};

/**
 * There are different ways to do the exact evaluation. Depending on the expression or data, one
 * or the other is more efficient.
 */
enum class ExactEvalMode {
  /**
   * Does the evaluation by working directly with arrays of sorted indices. This is usually best
   * when the expression does not have intermediate results, i.e. it is very simple.
   */
  Indices,
  /**
   * The evaluation works with bits. There is extra overhead to convert the input masks to bit
   * arrays and to convert the final result back into indices. In exchange, the actual expression
   * evaluation is significantly cheaper because it's just a bunch of bit operations. For larger
   * expressions, this is typically much more efficient.
   */
  Bits,
};

static void sort_course_boundaries(MutableSpan<CourseBoundary> boundaries)
{
  std::sort(boundaries.begin(),
            boundaries.end(),
            [](const CourseBoundary &a, const CourseBoundary &b) { return a.index < b.index; });
}

static void sort_course_boundaries(MutableSpan<DifferenceCourseBoundary> boundaries)
{
  std::sort(boundaries.begin(),
            boundaries.end(),
            [](const DifferenceCourseBoundary &a, const DifferenceCourseBoundary &b) {
              return a.index < b.index;
            });
}

/** Smaller segments should generally be merged together. */
static constexpr int64_t segment_size_threshold = 32;

/** Extends a previous full segment or appends a new one. */
static CoarseSegment &add_coarse_segment__full(CoarseSegment *prev_segment,
                                               const int64_t prev_boundary_index,
                                               const int64_t current_boundary_index,
                                               CoarseResult &result)
{
  const int64_t size = current_boundary_index - prev_boundary_index;
  if (prev_segment) {
    if (prev_segment->type == CoarseSegment::Type::Full &&
        prev_segment->bounds.one_after_last() == prev_boundary_index)
    {
      prev_segment->bounds = prev_segment->bounds.with_new_end(current_boundary_index);
      return *prev_segment;
    }
    if (current_boundary_index - prev_segment->bounds.start() < max_segment_size) {
      if (prev_segment->bounds.size() + size < segment_size_threshold) {
        /* Extend the previous segment because it's so small and change it into an unknown one. */
        prev_segment->bounds = prev_segment->bounds.with_new_end(current_boundary_index);
        prev_segment->type = CoarseSegment::Type::Unknown;
        return *prev_segment;
      }
    }
  }
  result.segments.append(
      {CoarseSegment::Type::Full, IndexRange::from_begin_size(prev_boundary_index, size)});
  return result.segments.last();
}

/** Extends a previous unknown segment or appends a new one. */
static CoarseSegment &add_coarse_segment__unknown(CoarseSegment *prev_segment,
                                                  const int64_t prev_boundary_index,
                                                  const int64_t current_boundary_index,
                                                  CoarseResult &result)
{
  if (prev_segment) {
    if (prev_segment->bounds.start() + segment_size_threshold >= prev_boundary_index) {
      /* The previous segment is very short, so extend it. */
      prev_segment->type = CoarseSegment::Type::Unknown;
      prev_segment->bounds = prev_segment->bounds.with_new_end(current_boundary_index);
      return *prev_segment;
    }
  }
  result.segments.append(
      {CoarseSegment::Type::Unknown,
       IndexRange::from_begin_end(prev_boundary_index, current_boundary_index)});
  return result.segments.last();
}

/** Extends a previous copy segment or appends a new one. */
static CoarseSegment &add_coarse_segment__copy(CoarseSegment *prev_segment,
                                               const int64_t prev_boundary_index,
                                               const int64_t current_boundary_index,
                                               const IndexMask &copy_from_mask,
                                               CoarseResult &result)
{
  if (prev_segment) {
    if (prev_segment->type == CoarseSegment::Type::Copy &&
        prev_segment->bounds.one_after_last() == prev_boundary_index &&
        prev_segment->mask == &copy_from_mask)
    {
      /* Can extend the previous copy segment. */
      prev_segment->bounds = prev_segment->bounds.with_new_end(current_boundary_index);
      return *prev_segment;
    }
    if (prev_segment->bounds.start() + segment_size_threshold >= current_boundary_index) {
      /* The previous and this segment together are very short, so better merge them together. */
      prev_segment->bounds = prev_segment->bounds.with_new_end(current_boundary_index);
      prev_segment->type = CoarseSegment::Type::Unknown;
      return *prev_segment;
    }
  }
  result.segments.append({CoarseSegment::Type::Copy,
                          IndexRange::from_begin_end(prev_boundary_index, current_boundary_index),
                          &copy_from_mask});
  return result.segments.last();
}

static void evaluate_coarse_union(const Span<CourseBoundary> boundaries, CoarseResult &r_result)
{
  if (boundaries.is_empty()) {
    return;
  }

  CoarseResult &result = r_result;
  CoarseSegment *prev_segment = nullptr;
  Vector<const CoarseSegment *, 16> active_segments;
  int64_t prev_boundary_index = boundaries[0].index;

  for (const CourseBoundary &boundary : boundaries) {
    if (prev_boundary_index < boundary.index) {
      /* Compute some properties of the input segments that were active between the current and the
       * previous boundary. */
      bool has_full = false;
      bool has_unknown = false;
      bool copy_from_single_mask = true;
      const IndexMask *copy_from_mask = nullptr;
      for (const CoarseSegment *active_segment : active_segments) {
        switch (active_segment->type) {
          case CoarseSegment::Type::Unknown: {
            has_unknown = true;
            break;
          }
          case CoarseSegment::Type::Full: {
            has_full = true;
            break;
          }
          case CoarseSegment::Type::Copy: {
            if (copy_from_mask != nullptr && copy_from_mask != active_segment->mask) {
              copy_from_single_mask = false;
            }
            copy_from_mask = active_segment->mask;
            break;
          }
        }
      }
      /* Determine the resulting coarse segment type based on the properties computed above. */
      if (has_full) {
        prev_segment = &add_coarse_segment__full(
            prev_segment, prev_boundary_index, boundary.index, result);
      }
      else if (has_unknown || !copy_from_single_mask) {
        prev_segment = &add_coarse_segment__unknown(
            prev_segment, prev_boundary_index, boundary.index, result);
      }
      else if (copy_from_mask != nullptr && copy_from_single_mask) {
        prev_segment = &add_coarse_segment__copy(
            prev_segment, prev_boundary_index, boundary.index, *copy_from_mask, result);
      }

      prev_boundary_index = boundary.index;
    }

    /* Update active segments. */
    if (boundary.is_begin) {
      active_segments.append(boundary.segment);
    }
    else {
      active_segments.remove_first_occurrence_and_reorder(boundary.segment);
    }
  }
}

static void evaluate_coarse_intersection(const Span<CourseBoundary> boundaries,
                                         const int64_t terms_num,
                                         CoarseResult &r_result)
{
  if (boundaries.is_empty()) {
    return;
  }

  CoarseResult &result = r_result;
  CoarseSegment *prev_segment = nullptr;
  Vector<const CoarseSegment *, 16> active_segments;
  int64_t prev_boundary_index = boundaries[0].index;

  for (const CourseBoundary &boundary : boundaries) {
    if (prev_boundary_index < boundary.index) {
      /* Only if one segment of each term is active, it's possible that the output contains
       * anything. */
      if (active_segments.size() == terms_num) {
        /* Compute some properties of the input segments that were active between the current and
         * previous boundary. */
        int full_count = 0;
        int unknown_count = 0;
        int copy_count = 0;
        bool copy_from_single_mask = true;
        const IndexMask *copy_from_mask = nullptr;
        for (const CoarseSegment *active_segment : active_segments) {
          switch (active_segment->type) {
            case CoarseSegment::Type::Unknown: {
              unknown_count++;
              break;
            }
            case CoarseSegment::Type::Full: {
              full_count++;
              break;
            }
            case CoarseSegment::Type::Copy: {
              copy_count++;
              if (copy_from_mask != nullptr && copy_from_mask != active_segment->mask) {
                copy_from_single_mask = false;
              }
              copy_from_mask = active_segment->mask;
              break;
            }
          }
        }
        /* Determine the resulting coarse segment type based on the properties computed above. */
        BLI_assert(full_count + unknown_count + copy_count == terms_num);
        if (full_count == terms_num) {
          prev_segment = &add_coarse_segment__full(
              prev_segment, prev_boundary_index, boundary.index, result);
        }
        else if (unknown_count > 0 || copy_count < terms_num || !copy_from_single_mask) {
          prev_segment = &add_coarse_segment__unknown(
              prev_segment, prev_boundary_index, boundary.index, result);
        }
        else if (copy_count == terms_num && copy_from_single_mask) {
          prev_segment = &add_coarse_segment__copy(
              prev_segment, prev_boundary_index, boundary.index, *copy_from_mask, result);
        }
      }

      prev_boundary_index = boundary.index;
    }

    /* Update active segments. */
    if (boundary.is_begin) {
      active_segments.append(boundary.segment);
    }
    else {
      active_segments.remove_first_occurrence_and_reorder(boundary.segment);
    }
  }
}

static void evaluate_coarse_difference(const Span<DifferenceCourseBoundary> boundaries,
                                       CoarseResult &r_result)
{
  if (boundaries.is_empty()) {
    return;
  }

  CoarseResult &result = r_result;
  CoarseSegment *prev_segment = nullptr;
  Vector<const CoarseSegment *> active_main_segments;
  Vector<const CoarseSegment *, 16> active_subtract_segments;
  int64_t prev_boundary_index = boundaries[0].index;

  for (const DifferenceCourseBoundary &boundary : boundaries) {
    if (prev_boundary_index < boundary.index) {
      /* There is only one main term, so at most one main segment can be active at once. */
      BLI_assert(active_main_segments.size() <= 1);
      if (active_main_segments.size() == 1) {
        const CoarseSegment &active_main_segment = *active_main_segments[0];
        /* Compute some properties of the input segments that were active between the current and
         * the previous boundary. */
        bool has_subtract_full = false;
        bool has_subtract_same_mask = false;
        for (const CoarseSegment *active_subtract_segment : active_subtract_segments) {
          switch (active_subtract_segment->type) {
            case CoarseSegment::Type::Unknown: {
              break;
            }
            case CoarseSegment::Type::Full: {
              has_subtract_full = true;
              break;
            }
            case CoarseSegment::Type::Copy: {
              if (active_main_segment.type == CoarseSegment::Type::Copy) {
                if (active_main_segment.mask == active_subtract_segment->mask) {
                  has_subtract_same_mask = true;
                }
              }
              break;
            }
          }
        }
        /* Determine the resulting coarse segment type based on the properties computed above. */
        if (has_subtract_full) {
          /* Do nothing, the resulting segment is empty for the current range. */
        }
        else {
          switch (active_main_segment.type) {
            case CoarseSegment::Type::Unknown: {
              prev_segment = &add_coarse_segment__unknown(
                  prev_segment, prev_boundary_index, boundary.index, result);
              break;
            }
            case CoarseSegment::Type::Full: {
              if (active_subtract_segments.is_empty()) {
                prev_segment = &add_coarse_segment__full(
                    prev_segment, prev_boundary_index, boundary.index, result);
              }
              else {
                prev_segment = &add_coarse_segment__unknown(
                    prev_segment, prev_boundary_index, boundary.index, result);
              }
              break;
            }
            case CoarseSegment::Type::Copy: {
              if (active_subtract_segments.is_empty()) {
                prev_segment = &add_coarse_segment__copy(prev_segment,
                                                         prev_boundary_index,
                                                         boundary.index,
                                                         *active_main_segment.mask,
                                                         result);
              }
              else if (has_subtract_same_mask) {
                /* Do nothing, subtracting a mask from itself results in an empty mask. */
              }
              else {
                prev_segment = &add_coarse_segment__unknown(
                    prev_segment, prev_boundary_index, boundary.index, result);
              }
              break;
            }
          }
        }
      }

      prev_boundary_index = boundary.index;
    }

    /* Update active segments. */
    if (boundary.is_main) {
      if (boundary.is_begin) {
        active_main_segments.append(boundary.segment);
      }
      else {
        active_main_segments.remove_first_occurrence_and_reorder(boundary.segment);
      }
    }
    else {
      if (boundary.is_begin) {
        active_subtract_segments.append(boundary.segment);
      }
      else {
        active_subtract_segments.remove_first_occurrence_and_reorder(boundary.segment);
      }
    }
  }
}

/**
 * The coarse evaluation only looks at the index masks as a whole within the given bounds. This
 * limitation allows it to do many operations in constant time independent of the number of indices
 * within each mask. For example, it can detect that two full index masks that overlap result in a
 * new full index mask when the union of intersection is computed.
 *
 * For more complex index-masks, coarse evaluation outputs segments with type
 * #CoarseSegment::Type::Unknown. Those segments can be evaluated in more detail afterwards.
 *
 * \param root_expression: Expression to be evaluated.
 * \param eval_order: Pre-computed evaluation order. All children of a term must come before
 *   the term itself.
 * \param eval_bounds: If given, the evaluation is restriced to those bounds. Otherwise, the full
 *   referenced masks are used.
 */
static CoarseResult evaluate_coarse(const Expr &root_expression,
                                    const Span<const Expr *> eval_order,
                                    const std::optional<IndexRange> eval_bounds = std::nullopt)
{
  /* An expression result for each intermediate expression. */
  Array<std::optional<CoarseResult>, inline_expr_array_size> expression_results(
      root_expression.expression_array_size());

  /* Process expressions in a pre-determined order. */
  for (const Expr *expression : eval_order) {
    CoarseResult &expr_result = expression_results[expression->index].emplace();
    switch (expression->type) {
      case Expr::Type::Atomic: {
        const AtomicExpr &expr = expression->as_atomic();

        IndexMask mask;
        if (eval_bounds.has_value()) {
          mask = expr.mask->slice_content(*eval_bounds);
        }
        else {
          mask = *expr.mask;
        }

        if (!mask.is_empty()) {
          const IndexRange bounds = mask.bounds();
          if (const std::optional<IndexRange> range = mask.to_range()) {
            expr_result.segments.append({CoarseSegment::Type::Full, bounds});
          }
          else {
            expr_result.segments.append({CoarseSegment::Type::Copy, bounds, expr.mask});
          }
        }
        break;
      }
      case Expr::Type::Union: {
        const UnionExpr &expr = expression->as_union();
        Vector<CourseBoundary, 16> boundaries;
        for (const Expr *term : expr.terms) {
          const CoarseResult &term_result = *expression_results[term->index];
          for (const CoarseSegment &segment : term_result.segments) {
            boundaries.append({segment.bounds.first(), true, &segment});
            boundaries.append({segment.bounds.one_after_last(), false, &segment});
          }
        }
        sort_course_boundaries(boundaries);
        evaluate_coarse_union(boundaries, expr_result);
        break;
      }
      case Expr::Type::Intersection: {
        const IntersectionExpr &expr = expression->as_intersection();
        Vector<CourseBoundary, 16> boundaries;
        for (const Expr *term : expr.terms) {
          const CoarseResult &term_result = *expression_results[term->index];
          for (const CoarseSegment &segment : term_result.segments) {
            boundaries.append({segment.bounds.first(), true, &segment});
            boundaries.append({segment.bounds.one_after_last(), false, &segment});
          }
        }
        sort_course_boundaries(boundaries);
        evaluate_coarse_intersection(boundaries, expr.terms.size(), expr_result);
        break;
      }
      case Expr::Type::Difference: {
        const DifferenceExpr &expr = expression->as_difference();
        Vector<DifferenceCourseBoundary, 16> boundaries;
        const CoarseResult &main_term_result = *expression_results[expr.terms[0]->index];
        for (const CoarseSegment &segment : main_term_result.segments) {
          boundaries.append({{segment.bounds.first(), true, &segment}, true});
          boundaries.append({{segment.bounds.one_after_last(), false, &segment}, true});
        }
        for (const Expr *term : expr.terms.as_span().drop_front(1)) {
          const CoarseResult &term_result = *expression_results[term->index];
          for (const CoarseSegment &segment : term_result.segments) {
            boundaries.append({{segment.bounds.first(), true, &segment}, false});
            boundaries.append({{segment.bounds.one_after_last(), false, &segment}, false});
          }
        }
        sort_course_boundaries(boundaries);
        evaluate_coarse_difference(boundaries, expr_result);
        break;
      }
    }
  }

  CoarseResult &final_result = *expression_results[root_expression.index];
  return std::move(final_result);
}

static Span<int16_t> bits_to_indices(const BoundedBitSpan bits, LinearAllocator<> &allocator)
{
  /* TODO: Could first count the number of set bits. */
  Vector<int16_t, max_segment_size> indices_vec;
  bits::foreach_1_index(bits, [&](const int64_t i) {
    BLI_assert(i < max_segment_size);
    indices_vec.append_unchecked(int16_t(i));
  });
  return allocator.construct_array_copy<int16_t>(indices_vec);
}

/**
 * Does an exact evaluation of the expression within the given bounds. The evaluation generally
 * works in three steps:
 * 1. Convert input indices into bit spans.
 * 2. Use bit operations to evaluate the expression.
 * 3. Convert resulting bit span back to indices.
 *
 * The trade-off here is that the actual expression evaluation is much faster but the conversions
 * take some extra time. Therefore, this approach is best when the evaluation would otherwise take
 * longer than the conversions which is usually the case for non-trivial expressions.
 */
static IndexMaskSegment evaluate_exact_with_bits(const Expr &root_expression,
                                                 LinearAllocator<> &allocator,
                                                 const IndexRange bounds,
                                                 const Span<const Expr *> eval_order)
{
  BLI_assert(bounds.size() <= max_segment_size);
  const int64_t bounds_min = bounds.start();
  const int expr_array_size = root_expression.expression_array_size();

  /* Make bit span sizes a multiple of `BitsPerInt`. This allows the bit-wise operations to run a
   * bit more efficiently, because only full integers are processed. */
  const int64_t ints_in_bounds = ceil_division(bounds.size(), bits::BitsPerInt);
  BitGroupVector<16 * 1024> expression_results(
      expr_array_size, ints_in_bounds * bits::BitsPerInt, false);

  for (const Expr *expression : eval_order) {
    MutableBoundedBitSpan expr_result = expression_results[expression->index];
    switch (expression->type) {
      case Expr::Type::Atomic: {
        const AtomicExpr &expr = expression->as_atomic();
        const IndexMask mask = expr.mask->slice_content(bounds);
        mask.to_bits(expr_result, -bounds_min);
        break;
      }
      case Expr::Type::Union: {
        for (const Expr *term : expression->terms) {
          expr_result |= expression_results[term->index];
        }
        break;
      }
      case Expr::Type::Intersection: {
        bits::copy_from_or(expr_result, expression_results[expression->terms[0]->index]);
        for (const Expr *term : expression->terms.as_span().drop_front(1)) {
          expr_result &= expression_results[term->index];
        }
        break;
      }
      case Expr::Type::Difference: {
        bits::copy_from_or(expr_result, expression_results[expression->terms[0]->index]);
        for (const Expr *term : expression->terms.as_span().drop_front(1)) {
          bits::mix_into_first_expr(
              [](const bits::BitInt a, const bits::BitInt b) { return a & ~b; },
              expr_result,
              expression_results[term->index]);
        }
        break;
      }
    }
  }
  const BoundedBitSpan final_bits = expression_results[root_expression.index];
  const Span<int16_t> indices = bits_to_indices(final_bits, allocator);
  return IndexMaskSegment(bounds_min, indices);
}

/** Compute a new set of indices that is the union of the given segments. */
static IndexMaskSegment union_index_mask_segments(const Span<IndexMaskSegment> segments,
                                                  const int64_t bounds_min,
                                                  int16_t *r_values)
{
  if (segments.is_empty()) {
    return {};
  }
  if (segments.size() == 1) {
    return segments[0];
  }
  if (segments.size() == 2) {
    const IndexMaskSegment a = segments[0].shift(-bounds_min);
    const IndexMaskSegment b = segments[1].shift(-bounds_min);
    const int64_t size = std::set_union(a.begin(), a.end(), b.begin(), b.end(), r_values) -
                         r_values;
    return {bounds_min, {r_values, size}};
  }

  /* Sort input segments by their size, so that smaller segments are unioned first. This results in
   * smaller intermediate arrays and thus less work overall. */
  Vector<IndexMaskSegment> sorted_segments(segments);
  std::sort(
      sorted_segments.begin(),
      sorted_segments.end(),
      [](const IndexMaskSegment &a, const IndexMaskSegment &b) { return a.size() < b.size(); });

  std::array<int16_t, max_segment_size> tmp_indices;
  /* Can use r_values for temporary values because if it's large enough for the final result, it's
   * also large enough for intermediate results. */
  int16_t *buffer_a = r_values;
  int16_t *buffer_b = tmp_indices.data();

  if (sorted_segments.size() % 2 == 1) {
    /* Swap buffers so that the result is in #r_values in the end. */
    std::swap(buffer_a, buffer_b);
  }

  int64_t count = 0;
  {
    /* Initial union. */
    const IndexMaskSegment a = sorted_segments[0].shift(-bounds_min);
    const IndexMaskSegment b = sorted_segments[1].shift(-bounds_min);
    int16_t *dst = buffer_a;
    count = std::set_union(a.begin(), a.end(), b.begin(), b.end(), dst) - dst;
  }

  /* Union one input into the result at a time. In theory, one could write an algorithm that unions
   * multiple sorted arrays at once, but that's more complex and it's not obvious that it would be
   * faster in the end. */
  for (const int64_t segment_i : sorted_segments.index_range().drop_front(2)) {
    const int16_t *a = buffer_a;
    const IndexMaskSegment b = sorted_segments[segment_i].shift(-bounds_min);
    int16_t *dst = buffer_b;
    count = std::set_union(a, a + count, b.begin(), b.end(), dst) - dst;
    std::swap(buffer_a, buffer_b);
  }
  return {bounds_min, {r_values, count}};
}

/** Compute a new set of indices that is the intersection of the given segments. */
static IndexMaskSegment intersect_index_mask_segments(const Span<IndexMaskSegment> segments,
                                                      const int64_t bounds_min,
                                                      int16_t *r_values)
{
  if (segments.is_empty()) {
    return {};
  }
  if (segments.size() == 1) {
    return segments[0];
  }
  if (segments.size() == 2) {
    const IndexMaskSegment a = segments[0].shift(-bounds_min);
    const IndexMaskSegment b = segments[1].shift(-bounds_min);
    const int64_t size = std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), r_values) -
                         r_values;
    return {bounds_min, {r_values, size}};
  }

  /* Intersect smaller segments first, because then the intermediate results will generally be
   * smaller. */
  Vector<IndexMaskSegment> sorted_segments(segments);
  std::sort(
      sorted_segments.begin(),
      sorted_segments.end(),
      [](const IndexMaskSegment &a, const IndexMaskSegment &b) { return a.size() < b.size(); });

  std::array<int16_t, max_segment_size> tmp_indices_1;
  std::array<int16_t, max_segment_size> tmp_indices_2;
  int16_t *buffer_a = tmp_indices_1.data();
  int16_t *buffer_b = tmp_indices_2.data();

  int64_t count = 0;
  {
    /* Initial intersection. */
    const IndexMaskSegment a = sorted_segments[0].shift(-bounds_min);
    const IndexMaskSegment b = sorted_segments[1].shift(-bounds_min);
    int16_t *dst = buffer_a;
    count = std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), dst) - dst;
  }

  for (const int64_t segment_i : sorted_segments.index_range().drop_front(2)) {
    const int16_t *a = buffer_a;
    const IndexMaskSegment b = sorted_segments[segment_i].shift(-bounds_min);
    /* The result of the final intersection should be written directly to #r_values to avoid an
     * additional copy in the end. */
    int16_t *dst = (segment_i == sorted_segments.size() - 1) ? r_values : buffer_b;
    count = std::set_intersection(a, a + count, b.begin(), b.end(), dst) - dst;
    std::swap(buffer_a, buffer_b);
  }
  return {bounds_min, {r_values, count}};
}

/**
 * Compute a new set of indices that is the difference between the main-segment and all the
 * subtract-segments.
 */
static IndexMaskSegment difference_index_mask_segments(
    const IndexMaskSegment main_segment,
    const Span<IndexMaskSegment> subtract_segments,
    const int64_t bounds_min,
    int16_t *r_values)
{
  if (main_segment.is_empty()) {
    return {};
  }
  if (subtract_segments.is_empty()) {
    return main_segment;
  }
  if (subtract_segments.size() == 1) {
    const IndexMaskSegment shifted_main_segment = main_segment.shift(-bounds_min);
    const IndexMaskSegment subtract_segment = subtract_segments[0].shift(-bounds_min);
    const int64_t size = std::set_difference(shifted_main_segment.begin(),
                                             shifted_main_segment.end(),
                                             subtract_segment.begin(),
                                             subtract_segment.end(),
                                             r_values) -
                         r_values;
    return {bounds_min, {r_values, size}};
  }

  int64_t subtract_count = 0;
  for (const IndexMaskSegment &segment : subtract_segments) {
    subtract_count += segment.size();
  }
  if (subtract_count < main_segment.size() / 2) {
    /* Can be more efficient to union all the subtract indices first before computing the
     * difference. This avoids potentially multiple larger intermediate arrays. */
    std::array<int16_t, max_segment_size> union_indices;
    const IndexMaskSegment shifted_main_segment = main_segment.shift(-bounds_min);
    const IndexMaskSegment unioned_subtract_segment =
        union_index_mask_segments(subtract_segments, bounds_min, union_indices.data())
            .shift(-bounds_min);
    const int64_t size = std::set_difference(shifted_main_segment.begin(),
                                             shifted_main_segment.end(),
                                             unioned_subtract_segment.begin(),
                                             unioned_subtract_segment.end(),
                                             r_values) -
                         r_values;
    return {bounds_min, {r_values, size}};
  }

  /* Sort larger segments to the front. This way the intermediate arrays are likely smaller. */
  Vector<IndexMaskSegment> sorted_subtract_segments(subtract_segments);
  std::sort(
      sorted_subtract_segments.begin(),
      sorted_subtract_segments.end(),
      [](const IndexMaskSegment &a, const IndexMaskSegment &b) { return a.size() > b.size(); });

  std::array<int16_t, max_segment_size> tmp_indices_1;
  std::array<int16_t, max_segment_size> tmp_indices_2;
  int16_t *buffer_a = tmp_indices_1.data();
  int16_t *buffer_b = tmp_indices_2.data();

  int64_t count = 0;
  {
    /* Initial difference. */
    const IndexMaskSegment shifted_main_segment = main_segment.shift(-bounds_min);
    const IndexMaskSegment subtract_segment = sorted_subtract_segments[0].shift(-bounds_min);
    int16_t *dst = buffer_a;
    count = std::set_difference(shifted_main_segment.begin(),
                                shifted_main_segment.end(),
                                subtract_segment.begin(),
                                subtract_segment.end(),
                                dst) -
            dst;
  }

  for (const int64_t segment_i : sorted_subtract_segments.index_range().drop_front(1)) {
    const IndexMaskSegment &subtract_segment = sorted_subtract_segments[segment_i].shift(
        -bounds_min);
    /* The final result should be written directly to #r_values to avoid an additional copy. */
    int16_t *dst = (segment_i == sorted_subtract_segments.size() - 1) ? r_values : buffer_b;
    count = std::set_difference(buffer_a,
                                buffer_a + count,
                                subtract_segment.begin(),
                                subtract_segment.end(),
                                dst) -
            dst;
    std::swap(buffer_a, buffer_b);
  }
  return {bounds_min, {r_values, count}};
}

/**
 * Does an exact evaluation of the expression with in the given bounds. The evaluation builds on
 * top of algorithms like `std::set_union`. This approach is especially useful if the expression is
 * simple and doesn't have many intermediate values.
 */
static IndexMaskSegment evaluate_exact_with_indices(const Expr &root_expression,
                                                    LinearAllocator<> &allocator,
                                                    const IndexRange bounds,
                                                    const Span<const Expr *> eval_order)
{
  BLI_assert(bounds.size() <= max_segment_size);
  const int64_t bounds_min = bounds.start();
  const int expr_array_size = root_expression.expression_array_size();
  Array<IndexMaskSegment, inline_expr_array_size> results(expr_array_size);
  for (const Expr *expression : eval_order) {
    switch (expression->type) {
      case Expr::Type::Atomic: {
        const AtomicExpr &expr = expression->as_atomic();
        const IndexMask mask = expr.mask->slice_content(bounds);
        /* The caller should make sure that the bounds are aligned to segment bounds. */
        BLI_assert(mask.segments_num() <= 1);
        if (mask.segments_num() == 1) {
          results[expression->index] = mask.segment(0);
        }
        break;
      }
      case Expr::Type::Union: {
        const UnionExpr &expr = expression->as_union();
        Array<IndexMaskSegment> term_segments(expr.terms.size());
        int64_t result_size_upper_bound = 0;
        bool used_short_circuit = false;
        for (const int64_t term_i : expr.terms.index_range()) {
          const Expr &term = *expr.terms[term_i];
          const IndexMaskSegment term_segment = results[term.index];
          if (term_segment.size() == bounds.size()) {
            /* Can skip computing the union if we know that one of the inputs contains all possible
             * indices already.  */
            results[expression->index] = term_segment;
            used_short_circuit = true;
            break;
          }
          term_segments[term_i] = term_segment;
          result_size_upper_bound += term_segment.size();
        }
        if (used_short_circuit) {
          break;
        }
        result_size_upper_bound = std::min(result_size_upper_bound, bounds.size());
        MutableSpan<int16_t> dst = allocator.allocate_array<int16_t>(result_size_upper_bound);
        const IndexMaskSegment result_segment = union_index_mask_segments(
            term_segments, bounds_min, dst.data());
        allocator.free_end_of_previous_allocation(dst.size_in_bytes(),
                                                  result_segment.base_span().end());
        results[expression->index] = result_segment;
        break;
      }
      case Expr::Type::Intersection: {
        const IntersectionExpr &expr = expression->as_intersection();
        Array<IndexMaskSegment> term_segments(expr.terms.size());
        int64_t result_size_upper_bound = bounds.size();
        bool used_short_circuit = false;
        for (const int64_t term_i : expr.terms.index_range()) {
          const Expr &term = *expr.terms[term_i];
          const IndexMaskSegment term_segment = results[term.index];
          if (term_segment.is_empty()) {
            /* Can skip computing the intersection if we know that one of the inputs is empty. */
            results[expression->index] = {};
            used_short_circuit = true;
            break;
          }
          result_size_upper_bound = std::min(result_size_upper_bound, term_segment.size());
          term_segments[term_i] = term_segment;
        }
        if (used_short_circuit) {
          break;
        }
        MutableSpan<int16_t> dst = allocator.allocate_array<int16_t>(result_size_upper_bound);
        const IndexMaskSegment result_segment = intersect_index_mask_segments(
            term_segments, bounds_min, dst.data());
        allocator.free_end_of_previous_allocation(dst.size_in_bytes(),
                                                  result_segment.base_span().end());
        results[expression->index] = result_segment;
        break;
      }
      case Expr::Type::Difference: {
        const DifferenceExpr &expr = expression->as_difference();
        const Expr &main_term = *expr.terms[0];
        const IndexMaskSegment main_segment = results[main_term.index];
        if (main_segment.is_empty()) {
          /* Can skip the computation of the main segment is empty. */
          results[expression->index] = {};
          break;
        }
        int64_t result_size_upper_bound = main_segment.size();
        bool used_short_circuit = false;
        Array<IndexMaskSegment> subtract_segments(expr.terms.size() - 1);
        for (const int64_t term_i : expr.terms.index_range().drop_front(1)) {
          const Expr &subtract_term = *expr.terms[term_i];
          const IndexMaskSegment term_segment = results[subtract_term.index];
          if (term_segment.size() == bounds.size()) {
            /* Can skip computing the difference if we know that one of the subtract-terms is
             * full. */
            results[expression->index] = {};
            used_short_circuit = true;
            break;
          }
          result_size_upper_bound = std::min(result_size_upper_bound,
                                             bounds.size() - term_segment.size());
          subtract_segments[term_i - 1] = term_segment;
        }
        if (used_short_circuit) {
          break;
        }
        MutableSpan<int16_t> dst = allocator.allocate_array<int16_t>(result_size_upper_bound);
        const IndexMaskSegment result_segment = difference_index_mask_segments(
            main_segment, subtract_segments, bounds_min, dst.data());
        allocator.free_end_of_previous_allocation(dst.size_in_bytes(),
                                                  result_segment.base_span().end());
        results[expression->index] = result_segment;
        break;
      }
    }
  }
  return results[root_expression.index];
}

/**
 * Turn the evaluated segments into index mask segments that are then used to initialize the
 * resulting index mask.
 */
static Vector<IndexMaskSegment> build_result_mask_segments(
    const Span<EvaluatedSegment> evaluated_segments)
{
  const std::array<int16_t, max_segment_size> &static_indices_array = get_static_indices_array();

  Vector<IndexMaskSegment> result_mask_segments;
  for (const EvaluatedSegment &evaluated_segment : evaluated_segments) {
    switch (evaluated_segment.type) {
      case EvaluatedSegment::Type::Full: {
        const int64_t full_size = evaluated_segment.bounds.size();
        for (int64_t i = 0; i < full_size; i += max_segment_size) {
          const int64_t size = std::min(i + max_segment_size, full_size) - i;
          result_mask_segments.append(IndexMaskSegment(
              evaluated_segment.bounds.first() + i, Span(static_indices_array).take_front(size)));
        }
        break;
      }
      case EvaluatedSegment::Type::Copy: {
        const IndexMask sliced_mask = evaluated_segment.copy_mask->slice_content(
            evaluated_segment.bounds);
        sliced_mask.foreach_segment(
            [&](const IndexMaskSegment &segment) { result_mask_segments.append(segment); });
        break;
      }
      case EvaluatedSegment::Type::Indices: {
        result_mask_segments.append(evaluated_segment.indices);
        break;
      }
    }
  }
  return result_mask_segments;
}

/**
 * Computes an evaluation order of the expression. The important aspect is that all child terms
 * come before the term that uses them.
 */
static Vector<const Expr *, inline_expr_array_size> compute_eval_order(const Expr &root_expression)
{
  Vector<const Expr *, inline_expr_array_size> eval_order;
  if (root_expression.type == Expr::Type::Atomic) {
    eval_order.append(&root_expression);
    return eval_order;
  }

  Array<bool, inline_expr_array_size> is_evaluated_states(root_expression.expression_array_size(),
                                                          false);
  Stack<const Expr *, inline_expr_array_size> expr_stack;
  expr_stack.push(&root_expression);

  while (!expr_stack.is_empty()) {
    const Expr &expression = *expr_stack.peek();
    bool &is_evaluated = is_evaluated_states[expression.index];
    if (is_evaluated) {
      expr_stack.pop();
      continue;
    }
    bool all_terms_evaluated = true;
    for (const Expr *term : expression.terms) {
      bool &term_evaluated = is_evaluated_states[term->index];
      if (!term_evaluated) {
        if (term->type == Expr::Type::Atomic) {
          eval_order.append(term);
          term_evaluated = true;
        }
        else {
          expr_stack.push(term);
          all_terms_evaluated = false;
        }
      }
    }
    if (all_terms_evaluated) {
      eval_order.append(&expression);
      is_evaluated = true;
      expr_stack.pop();
    }
  }

  return eval_order;
}

/** Uses a heuristic to decide which exact evaluation mode probably works best. */
static ExactEvalMode determine_exact_eval_mode(const Expr &root_expression)
{
  for (const Expr *term : root_expression.terms) {
    if (!term->terms.is_empty()) {
      /* Use bits when there are nested expressions as this is often faster. */
      return ExactEvalMode::Bits;
    }
  }
  return ExactEvalMode::Indices;
}

static void evaluate_coarse_and_split_until_segments_are_short(
    const Expr &root_expression,
    const Span<const Expr *> eval_order,
    Vector<EvaluatedSegment, 16> &r_evaluated_segments,
    Vector<IndexRange, 16> &r_short_unknown_segments)
{
  /* Coarse evaluation splits the full range into segments. Long segments are split up and get
   * another coarse evaluation. Short segments will be evaluated exactly. */
  Stack<IndexRange, 16> long_unknown_segments;

  /* The point at which a range starts being "short". */
  const int64_t coarse_segment_size_threshold = max_segment_size;

  /* Checks the coarse results and inserts its segments into either `long_unknown_segments` for
   * further coarse evaluation, `r_short_unknown_segments` for exact evaluation or
   * `r_evaluated_segments` if no further evaluation is necessary. */
  auto handle_coarse_result = [&](const CoarseResult &coarse_result) {
    for (const CoarseSegment &segment : coarse_result.segments) {
      switch (segment.type) {
        case CoarseSegment::Type::Unknown: {
          if (segment.bounds.size() > coarse_segment_size_threshold) {
            long_unknown_segments.push(segment.bounds);
          }
          else {
            r_short_unknown_segments.append(segment.bounds);
          }
          break;
        }
        case CoarseSegment::Type::Copy: {
          BLI_assert(segment.mask);
          r_evaluated_segments.append(
              {EvaluatedSegment::Type::Copy, segment.bounds, segment.mask});
          break;
        }
        case CoarseSegment::Type::Full: {
          r_evaluated_segments.append({EvaluatedSegment::Type::Full, segment.bounds});
          break;
        }
      }
    }
  };

  /* Initial coarse evaluation without any explicit bounds. The bounds are implied by the index
   * masks used in the expression. */
  const CoarseResult initial_coarse_result = evaluate_coarse(root_expression, eval_order);
  handle_coarse_result(initial_coarse_result);

  /* Do coarse evaluation until all unknown segments are short enough to do exact evaluation. */
  while (!long_unknown_segments.is_empty()) {
    const IndexRange unknown_bounds = long_unknown_segments.pop();
    const int64_t split_pos = unknown_bounds.size() / 2;
    const IndexRange left_half = unknown_bounds.take_front(split_pos);
    const IndexRange right_half = unknown_bounds.drop_front(split_pos);
    const CoarseResult left_result = evaluate_coarse(root_expression, eval_order, left_half);
    const CoarseResult right_result = evaluate_coarse(root_expression, eval_order, right_half);
    handle_coarse_result(left_result);
    handle_coarse_result(right_result);
  }
}

static void evaluate_short_unknown_segments_exactly(
    const Expr &root_expression,
    const ExactEvalMode exact_eval_mode,
    const Span<const Expr *> eval_order,
    const Span<IndexRange> short_unknown_segments,
    IndexMaskMemory &memory,
    Vector<EvaluatedSegment, 16> &r_evaluated_segments)
{
  /* Evaluate a segment exactly. */
  auto evaluate_unknown_segment = [&](const IndexRange bounds,
                                      LinearAllocator<> &allocator,
                                      Vector<EvaluatedSegment, 16> &r_local_evaluated_segments) {
    /* Use the predetermined evaluation mode. */
    switch (exact_eval_mode) {
      case ExactEvalMode::Bits: {
        const IndexMaskSegment indices = evaluate_exact_with_bits(
            root_expression, allocator, bounds, eval_order);
        if (!indices.is_empty()) {
          r_local_evaluated_segments.append(
              {EvaluatedSegment::Type::Indices, bounds, nullptr, indices});
        }
        break;
      }
      case ExactEvalMode::Indices: {
        /* #evaluate_exact_with_indices requires that all index masks have a single segment in the
         * provided bounds. So split up the range into subranges first if necessary. */
        Vector<int64_t, 16> split_indices;
        /* Always adding the beginning and end of the bounds simplifies the code below. */
        split_indices.extend({bounds.first(), bounds.one_after_last()});
        for (const int64_t eval_order_i : eval_order.index_range()) {
          const Expr &expr = *eval_order[eval_order_i];
          if (expr.type != Expr::Type::Atomic) {
            continue;
          }
          const AtomicExpr &atomic_expr = expr.as_atomic();
          const IndexMask mask = atomic_expr.mask->slice_content(bounds);
          const int64_t segments_num = mask.segments_num();
          if (segments_num <= 1) {
            /* This mask only has a single segment in the bounds anyway, so no extra split-position
             * is necessary. */
            continue;
          }
          /* Split at the beginning of each segment. Skipping the first, because that does not need
           * an extra split position. Alternatively, one could also split at the end of each
           * segment except the last one. It doesn't matter much. */
          for (const int64_t segment_i : IndexRange(segments_num).drop_front(1)) {
            const IndexMaskSegment segment = mask.segment(segment_i);
            split_indices.append(segment[0]);
          }
        }
        std::sort(split_indices.begin(), split_indices.end());
        for (const int64_t boundary_i : split_indices.index_range().drop_back(1)) {
          const IndexRange sub_bounds = IndexRange::from_begin_end(split_indices[boundary_i],
                                                                   split_indices[boundary_i + 1]);
          if (sub_bounds.is_empty()) {
            continue;
          }
          const IndexMaskSegment indices = evaluate_exact_with_indices(
              root_expression, allocator, sub_bounds, eval_order);
          if (!indices.is_empty()) {
            r_local_evaluated_segments.append(
                {EvaluatedSegment::Type::Indices, sub_bounds, nullptr, indices});
          }
        }
        break;
      }
    }
  };

  /* Decide whether multi-threading should be used or not. There is some extra overhead even when
   * just attempting to use multi-threading. */
  const int64_t unknown_segment_eval_grain_size = 8;
  if (short_unknown_segments.size() < unknown_segment_eval_grain_size) {
    for (const IndexRange &bounds : short_unknown_segments) {
      evaluate_unknown_segment(bounds, memory, r_evaluated_segments);
    }
  }
  else {
    /* Do exact evaluation in multiple threads. The allocators and evaluated segments created by
     * each thread are merged in the end.  */
    struct LocalData {
      LinearAllocator<> allocator;
      Vector<EvaluatedSegment, 16> evaluated_segments;
    };
    threading::EnumerableThreadSpecific<LocalData> data_by_thread;
    threading::parallel_for(short_unknown_segments.index_range(),
                            unknown_segment_eval_grain_size,
                            [&](const IndexRange range) {
                              LocalData &data = data_by_thread.local();
                              for (const IndexRange &bounds : short_unknown_segments.slice(range))
                              {
                                evaluate_unknown_segment(
                                    bounds, data.allocator, data.evaluated_segments);
                              }
                            });
    for (LocalData &data : data_by_thread) {
      if (!data.evaluated_segments.is_empty()) {
        r_evaluated_segments.extend(data.evaluated_segments);
        memory.transfer_ownership_from(data.allocator);
      }
    }
  }
}

static IndexMask evaluated_segments_to_index_mask(MutableSpan<EvaluatedSegment> evaluated_segments,
                                                  IndexMaskMemory &memory)
{
  if (evaluated_segments.is_empty()) {
    return {};
  }
  if (evaluated_segments.size() == 1) {
    const EvaluatedSegment &evaluated_segment = evaluated_segments[0];
    switch (evaluated_segment.type) {
      case EvaluatedSegment::Type::Full: {
        return IndexMask(IndexRange(evaluated_segment.bounds));
      }
      case EvaluatedSegment::Type::Copy: {
        return evaluated_segment.copy_mask->slice_content(evaluated_segment.bounds);
      }
      case EvaluatedSegment::Type::Indices: {
        return IndexMask::from_segments({evaluated_segment.indices}, memory);
      }
    }
  }

  std::sort(evaluated_segments.begin(),
            evaluated_segments.end(),
            [](const EvaluatedSegment &a, const EvaluatedSegment &b) {
              return a.bounds.start() < b.bounds.start();
            });

  Vector<IndexMaskSegment> result_segments = build_result_mask_segments(evaluated_segments);
  return IndexMask::from_segments(result_segments, memory);
}

static IndexMask evaluate_expression_impl(const Expr &root_expression,
                                          IndexMaskMemory &memory,
                                          const ExactEvalMode exact_eval_mode)
{
  /* Precompute the evaluation order here, because it's used potentially many times throughout the
   * algorithm. */
  const Vector<const Expr *, inline_expr_array_size> eval_order = compute_eval_order(
      root_expression);

  /* Non-overlapping evaluated segments which become the resulting index mask in the end. Note that
   * these segments are only sorted in the end. */
  Vector<EvaluatedSegment, 16> evaluated_segments;
  Vector<IndexRange, 16> short_unknown_segments;

  evaluate_coarse_and_split_until_segments_are_short(
      root_expression, eval_order, evaluated_segments, short_unknown_segments);
  evaluate_short_unknown_segments_exactly(root_expression,
                                          exact_eval_mode,
                                          eval_order,
                                          short_unknown_segments,
                                          memory,
                                          evaluated_segments);
  return evaluated_segments_to_index_mask(evaluated_segments, memory);
}

IndexMask evaluate_expression(const Expr &expression, IndexMaskMemory &memory)
{
  const ExactEvalMode exact_eval_mode = determine_exact_eval_mode(expression);
  IndexMask mask = evaluate_expression_impl(expression, memory, exact_eval_mode);
#ifndef NDEBUG
  {
    /* Check that both exact eval modes have the same result. */
    const ExactEvalMode other_exact_eval_mode = (exact_eval_mode == ExactEvalMode::Bits) ?
                                                    ExactEvalMode::Indices :
                                                    ExactEvalMode::Bits;
    IndexMask other_mask = evaluate_expression_impl(expression, memory, other_exact_eval_mode);
    BLI_assert(mask == other_mask);
  }
#endif
  return mask;
}

const UnionExpr &ExprBuilder::merge(const Span<Term> terms)
{
  Vector<const Expr *> term_expressions;
  for (const Term &term : terms) {
    term_expressions.append(&this->term_to_expr(term));
  }
  UnionExpr &expr = scope_.construct<UnionExpr>();
  expr.type = Expr::Type::Union;
  expr.index = expr_count_++;
  expr.terms = std::move(term_expressions);
  return expr;
}

const DifferenceExpr &ExprBuilder::subtract(const Term &main_term, const Span<Term> subtract_terms)
{
  Vector<const Expr *> term_expressions;
  term_expressions.append(&this->term_to_expr(main_term));
  for (const Term &subtract_term : subtract_terms) {
    term_expressions.append(&this->term_to_expr(subtract_term));
  }
  DifferenceExpr &expr = scope_.construct<DifferenceExpr>();
  expr.type = Expr::Type::Difference;
  expr.index = expr_count_++;
  expr.terms = std::move(term_expressions);
  return expr;
}

const IntersectionExpr &ExprBuilder::intersect(const Span<Term> terms)
{
  Vector<const Expr *> term_expressions;
  for (const Term &term : terms) {
    term_expressions.append(&this->term_to_expr(term));
  }
  IntersectionExpr &expr = scope_.construct<IntersectionExpr>();
  expr.type = Expr::Type::Intersection;
  expr.index += expr_count_++;
  expr.terms = std::move(term_expressions);
  return expr;
}

const Expr &ExprBuilder::term_to_expr(const Term &term)
{
  if (const Expr *const *expr = std::get_if<const Expr *>(&term)) {
    return **expr;
  }
  AtomicExpr &expr = scope_.construct<AtomicExpr>();
  expr.type = Expr::Type::Atomic;
  expr.index = expr_count_++;
  if (const IndexRange *range = std::get_if<IndexRange>(&term)) {
    expr.mask = &scope_.construct<IndexMask>(*range);
  }
  else {
    expr.mask = std::get<const IndexMask *>(term);
  }
  return expr;
}

}  // namespace blender::index_mask
