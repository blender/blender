/*
 * Copyright (c) 2016, Campbell Barton.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * Curve Re-fitting Method
 * =======================
 *
 * This is a more processor intensive method of fitting,
 * compared to #curve_fit_cubic_to_points_db, and works as follows:
 *
 * - First iteratively remove all points under the error threshold.
 * - If corner calculation is enabled:
 *   - Find adjacent knots that exceed the angle limit
 *   - Find a 'split' point between the knots (could include the knots)
 *   - If copying the tangents to this split point doesn't exceed the error threshold:
 *     - Assign the tangents of the two knots to the split point, define it as a corner.
 *       (after this, we have many points which are too close).
 * - Run a re-fit pass, where knots are re-positioned between their adjacent knots
 *   when their re-fit position has a lower 'error'.
 *   While re-fitting, remove knots that fall below the error threshold.
 */

#ifdef _MSC_VER
#  define _USE_MATH_DEFINES
#endif

#include <math.h>
#include <float.h>
#include <stdbool.h>
#include <assert.h>

#include <string.h>
#include <stdlib.h>

typedef unsigned int uint;

#include "curve_fit_inline.h"
#include "../curve_fit_nd.h"

#include "generic_heap.h"

#ifdef _MSC_VER
#  define alloca(size) _alloca(size)
#endif

#if !defined(_MSC_VER)
#  define USE_VLA
#endif

#ifdef USE_VLA
#  ifdef __GNUC__
#    pragma GCC diagnostic ignored "-Wvla"
#  endif
#else
#  ifdef __GNUC__
#    pragma GCC diagnostic error "-Wvla"
#  endif
#endif

/* adjust the knots after simplifying */
#define USE_KNOT_REFIT
/* remove knots under the error threshold while re-fitting */
#define USE_KNOT_REFIT_REMOVE
/* detect corners over an angle threshold */
#define USE_CORNER_DETECT
/* avoid re-calculating lengths multiple times */
#define USE_LENGTH_CACHE
/* use pool allocator */
#define USE_TPOOL


#define SPLIT_POINT_INVALID ((uint)-1)

#define MAX2(x, y) ((x) > (y) ? (x) : (y))

#define SQUARE(a) ((a) * (a))

#ifdef __GNUC__
#  define UNLIKELY(x)     __builtin_expect(!!(x), 0)
#else
#  define UNLIKELY(x)     (x)
#endif

struct PointData {
	const double *points;
	uint          points_len;
#ifdef USE_LENGTH_CACHE
	const double *points_length_cache;
#endif
};

struct Knot {
	struct Knot *next, *prev;

	HeapNode *heap_node;

	/**
	 * Currently the same, access as different for now
	 * since we may want to support different point/knot indices
	 */
	uint index;

	uint can_remove : 1;
	uint is_removed : 1;
	uint is_corner  : 1;

	double handles[2];
	/**
	 * Store the error value, to see if we can improve on it
	 * (without having to re-calculate each time)
	 *
	 * This is the error between this knot and the next */
	double error_sq_next;

	/* Initially point to contiguous memory, however we may re-assign */
	double *tan[2];
};


struct KnotRemoveState {
	uint index;
	/* Handles for prev/next knots */
	double handles[2];
};

#ifdef USE_TPOOL
/* rstate_* pool allocator */
#define TPOOL_IMPL_PREFIX  rstate
#define TPOOL_ALLOC_TYPE   struct KnotRemoveState
#define TPOOL_STRUCT       ElemPool_KnotRemoveState
#include "generic_alloc_impl.h"
#undef TPOOL_IMPL_PREFIX
#undef TPOOL_ALLOC_TYPE
#undef TPOOL_STRUCT
#endif  /* USE_TPOOL */

#ifdef USE_KNOT_REFIT
struct KnotRefitState {
	uint index;
	/** When SPLIT_POINT_INVALID - remove this item */
	uint index_refit;
	/** Handles for prev/next knots */
	double handles_prev[2], handles_next[2];
	double error_sq[2];
};

#ifdef USE_TPOOL
/* refit_* pool allocator */
#define TPOOL_IMPL_PREFIX  refit
#define TPOOL_ALLOC_TYPE   struct KnotRefitState
#define TPOOL_STRUCT       ElemPool_KnotRefitState
#include "generic_alloc_impl.h"
#undef TPOOL_IMPL_PREFIX
#undef TPOOL_ALLOC_TYPE
#undef TPOOL_STRUCT
#endif  /* USE_TPOOL */
#endif  /* USE_KNOT_REFIT */


#ifdef USE_CORNER_DETECT
/** Result of collapsing a corner. */
struct KnotCornerState {
	uint index;
	/* Merge adjacent handles into this one (may be shared with the 'index') */
	uint index_adjacent[2];

	/* Handles for prev/next knots */
	double handles_prev[2], handles_next[2];
	double error_sq[2];
};

/* refit_* pool allocator */
#ifdef USE_TPOOL
#define TPOOL_IMPL_PREFIX  corner
#define TPOOL_ALLOC_TYPE   struct KnotCornerState
#define TPOOL_STRUCT       ElemPool_KnotCornerState
#include "generic_alloc_impl.h"
#undef TPOOL_IMPL_PREFIX
#undef TPOOL_ALLOC_TYPE
#undef TPOOL_STRUCT
#endif  /* USE_TPOOL */
#endif  /* USE_CORNER_DETECT */


/* Utility functions */

#if defined(USE_KNOT_REFIT) && !defined(USE_KNOT_REFIT_REMOVE)
/**
 * Find the most distant point between the 2 knots.
 */
static uint knot_find_split_point(
        const struct PointData *pd,
        const struct Knot *knot_l, const struct Knot *knot_r,
        const uint knots_len,
        const uint dims)
{
	uint split_point = SPLIT_POINT_INVALID;
	double split_point_dist_best = -DBL_MAX;

	const double *offset = &pd->points[knot_l->index * dims];

#ifdef USE_VLA
	double v_plane[dims];
	double v_proj[dims];
	double v_offset[dims];
#else
	double *v_plane =   alloca(sizeof(double) * dims);
	double *v_proj =    alloca(sizeof(double) * dims);
	double *v_offset =  alloca(sizeof(double) * dims);
#endif

	sub_vn_vnvn(
	        v_plane,
	        &pd->points[knot_l->index * dims],
	        &pd->points[knot_r->index * dims],
	        dims);

	normalize_vn(v_plane, dims);

	const uint knots_end = knots_len - 1;
	const struct Knot *k_step = knot_l;
	do {
		if (k_step->index != knots_end) {
			k_step += 1;
		}
		else {
			/* wrap around */
			k_step = k_step - knots_end;
		}

		if (k_step != knot_r) {
			sub_vn_vnvn(v_offset, &pd->points[k_step->index * dims], offset, dims);
			project_plane_vn_vnvn_normalized(v_proj, v_offset, v_plane, dims);

			double split_point_dist_test = len_squared_vn(v_proj, dims);
			if (split_point_dist_test > split_point_dist_best) {
				split_point_dist_best = split_point_dist_test;
				split_point = k_step->index;
			}
		}
		else {
			break;
		}

	} while (true);

	return split_point;
}
#endif  /* USE_KNOT_REFIT && !USE_KNOT_REFIT_REMOVE */


#ifdef USE_CORNER_DETECT
/**
 * Find the knot furthest from the line between \a knot_l & \a knot_r.
 * This is to be used as a split point.
 */
static uint knot_find_split_point_on_axis(
        const struct PointData *pd,
        const struct Knot *knot_l, const struct Knot *knot_r,
        const uint knots_len,
        const double *plane_no,
        const uint dims)
{
	uint split_point = SPLIT_POINT_INVALID;
	double split_point_dist_best = -DBL_MAX;

	const uint knots_end = knots_len - 1;
	const struct Knot *k_step = knot_l;
	do {
		if (k_step->index != knots_end) {
			k_step += 1;
		}
		else {
			/* wrap around */
			k_step = k_step - knots_end;
		}

		if (k_step != knot_r) {
			double split_point_dist_test = dot_vnvn(plane_no, &pd->points[k_step->index * dims], dims);
			if (split_point_dist_test > split_point_dist_best) {
				split_point_dist_best = split_point_dist_test;
				split_point = k_step->index;
			}
		}
		else {
			break;
		}

	} while (true);

	return split_point;
}
#endif  /* USE_CORNER_DETECT */


static double knot_remove_error_value(
        const double *tan_l, const double *tan_r,
        const double *points_offset, const uint points_offset_len,
        const double *points_offset_length_cache,
        const uint dims,
        /* Avoid having to re-calculate again */
        double r_handle_factors[2], uint *r_error_index)
{
	double error_sq = FLT_MAX;

#ifdef USE_VLA
	double handle_factor_l[dims];
	double handle_factor_r[dims];
#else
	double *handle_factor_l = alloca(sizeof(double) * dims);
	double *handle_factor_r = alloca(sizeof(double) * dims);
#endif

	curve_fit_cubic_to_points_single_db(
	        points_offset, points_offset_len, points_offset_length_cache, dims, 0.0,
	        tan_l, tan_r,
	        handle_factor_l, handle_factor_r,
	        &error_sq, r_error_index);

	assert(error_sq != FLT_MAX);

	isub_vnvn(handle_factor_l, points_offset, dims);
	r_handle_factors[0] = dot_vnvn(tan_l, handle_factor_l, dims);

	isub_vnvn(handle_factor_r, &points_offset[(points_offset_len - 1) * dims], dims);
	r_handle_factors[1] = dot_vnvn(tan_r, handle_factor_r, dims);

	return error_sq;
}

static double knot_calc_curve_error_value(
        const struct PointData *pd,
        const struct Knot *knot_l, const struct Knot *knot_r,
        const double *tan_l, const double *tan_r,
        const uint dims,
        double r_handle_factors[2])
{
	const uint points_offset_len = ((knot_l->index < knot_r->index) ?
	        (knot_r->index - knot_l->index) :
	        ((knot_r->index + pd->points_len) - knot_l->index)) + 1;

	if (points_offset_len != 2) {
		uint error_index_dummy;
		return knot_remove_error_value(
		        tan_l, tan_r,
		        &pd->points[knot_l->index * dims], points_offset_len,
#ifdef USE_LENGTH_CACHE
		        &pd->points_length_cache[knot_l->index],
#else
		        NULL,
#endif
		        dims,
		        r_handle_factors, &error_index_dummy);
	}
	else {
		/* No points between, use 1/3 handle length with no error as a fallback. */
		assert(points_offset_len == 2);
#ifdef USE_LENGTH_CACHE
		r_handle_factors[0] = r_handle_factors[1] = pd->points_length_cache[knot_l->index] / 3.0;
#else
		r_handle_factors[0] = r_handle_factors[1] = len_vnvn(
		        &pd->points[(knot_l->index + 0) * dims],
		        &pd->points[(knot_l->index + 1) * dims], dims) / 3.0;
#endif
		return 0.0;
	}
}

#ifdef USE_KNOT_REFIT_REMOVE

static double knot_calc_curve_error_value_and_index(
        const struct PointData *pd,
        const struct Knot *knot_l, const struct Knot *knot_r,
        const double *tan_l, const double *tan_r,
        const uint dims,
        double r_handle_factors[2],
        uint *r_error_index)
{
	const uint points_offset_len = ((knot_l->index < knot_r->index) ?
	        (knot_r->index - knot_l->index) :
	        ((knot_r->index + pd->points_len) - knot_l->index)) + 1;

	if (points_offset_len != 2) {
		const double error_sq = knot_remove_error_value(
		        tan_l, tan_r,
		        &pd->points[knot_l->index * dims], points_offset_len,
#ifdef USE_LENGTH_CACHE
		        &pd->points_length_cache[knot_l->index],
#else
		        NULL,
#endif
		        dims,
		        r_handle_factors, r_error_index);

		/* Adjust the offset index to the global index & wrap if needed. */
		*r_error_index += knot_l->index;
		if (*r_error_index >= pd->points_len) {
			*r_error_index -= pd->points_len;
		}

		return error_sq;
	}
	else {
		/* No points between, use 1/3 handle length with no error as a fallback. */
		assert(points_offset_len == 2);
#ifdef USE_LENGTH_CACHE
		r_handle_factors[0] = r_handle_factors[1] = pd->points_length_cache[knot_l->index] / 3.0;
#else
		r_handle_factors[0] = r_handle_factors[1] = len_vnvn(
		        &pd->points[(knot_l->index + 0) * dims],
		        &pd->points[(knot_l->index + 1) * dims], dims) / 3.0;
#endif
		*r_error_index = 0;
		return 0.0;
	}
}
#endif  /* USE_KNOT_REFIT_REMOVE */

struct KnotRemove_Params {
	Heap *heap;
	const struct PointData *pd;
#ifdef USE_TPOOL
	struct ElemPool_KnotRemoveState *epool;
#endif
};

static void knot_remove_error_recalculate(
        struct KnotRemove_Params *p,
        struct Knot *k, const double error_sq_max,
        const uint dims)
{
	assert(k->can_remove);
	double handles[2];

	const double cost_sq = knot_calc_curve_error_value(
	        p->pd, k->prev, k->next,
	        k->prev->tan[1], k->next->tan[0],
	        dims,
	        handles);

	if (cost_sq < error_sq_max) {
		struct KnotRemoveState *r;
		if (k->heap_node) {
			r = HEAP_node_ptr(k->heap_node);
			HEAP_remove(p->heap, k->heap_node);
		}
		else {
#ifdef USE_TPOOL
			r = rstate_pool_elem_alloc(p->epool);
#else
			r = malloc(sizeof(*r));
#endif

			r->index = k->index;
		}

		r->handles[0] = handles[0];
		r->handles[1] = handles[1];

		k->heap_node = HEAP_insert(p->heap, cost_sq, r);
	}
	else {
		if (k->heap_node) {
			struct KnotRemoveState *r;
			r = HEAP_node_ptr(k->heap_node);
			HEAP_remove(p->heap, k->heap_node);

#ifdef USE_TPOOL
			rstate_pool_elem_free(p->epool, r);
#else
			free(r);
#endif

			k->heap_node = NULL;
		}
	}
}

/**
 * Return length after being reduced.
 */
static uint curve_incremental_simplify(
        const struct PointData *pd,
        struct Knot *knots, const uint knots_len, uint knots_len_remaining,
        double error_sq_max, const uint dims)
{

#ifdef USE_TPOOL
	struct ElemPool_KnotRemoveState epool;

	rstate_pool_create(&epool, 0);
#endif

	Heap *heap = HEAP_new(knots_len_remaining);

	struct KnotRemove_Params params = {
	    .pd = pd,
	    .heap = heap,
#ifdef USE_TPOOL
	    .epool = &epool,
#endif
	};

	for (uint i = 0; i < knots_len; i++) {
		struct Knot *k = &knots[i];
		if (k->can_remove && (k->is_removed == false) && (k->is_corner == false)) {
			knot_remove_error_recalculate(&params, k, error_sq_max, dims);
		}
	}

	while (HEAP_is_empty(heap) == false) {
		struct Knot *k;

		{
			const double error_sq = HEAP_top_value(heap);
			struct KnotRemoveState *r = HEAP_popmin(heap);
			k = &knots[r->index];
			k->heap_node = NULL;
			k->prev->handles[1] = r->handles[0];
			k->next->handles[0] = r->handles[1];

			k->prev->error_sq_next = error_sq;

#ifdef USE_TPOOL
			rstate_pool_elem_free(&epool, r);
#else
			free(r);
#endif
		}

		if (UNLIKELY(knots_len_remaining <= 2)) {
			continue;
		}

		struct Knot *k_prev = k->prev;
		struct Knot *k_next = k->next;

		/* Remove ourselves */
		k_next->prev = k_prev;
		k_prev->next = k_next;

		k->next = NULL;
		k->prev = NULL;
		k->is_removed = true;

		if (k_prev->can_remove && (k_prev->is_corner == false) && (k_prev->prev && k_prev->next)) {
			knot_remove_error_recalculate(&params, k_prev, error_sq_max, dims);
		}

		if (k_next->can_remove && (k_next->is_corner == false) && (k_next->prev && k_next->next)) {
			knot_remove_error_recalculate(&params, k_next, error_sq_max, dims);
		}

		knots_len_remaining -= 1;
	}

#ifdef USE_TPOOL
	rstate_pool_destroy(&epool);
#endif

	HEAP_free(heap, free);

	return knots_len_remaining;
}


#ifdef USE_KNOT_REFIT

struct KnotRefit_Params {
	Heap *heap;
	const struct PointData *pd;
#ifdef USE_TPOOL
	struct ElemPool_KnotRefitState *epool;
#endif
};

static void knot_refit_error_recalculate(
        struct KnotRefit_Params *p,
        struct Knot *knots, const uint knots_len,
        struct Knot *k,
        const double error_sq_max,
        const uint dims)
{
	assert(k->can_remove);

#ifdef USE_KNOT_REFIT_REMOVE
	(void)knots_len;

	uint refit_index = SPLIT_POINT_INVALID;
	{
		double handles[2];

		/* First check if we can remove, this allows to refit and remove as we go. */
		const double cost_sq = knot_calc_curve_error_value_and_index(
		        p->pd, k->prev, k->next,
		        k->prev->tan[1], k->next->tan[0],
		        dims,
		        handles, &refit_index);

		if (cost_sq < error_sq_max) {
			struct KnotRefitState *r;
			if (k->heap_node) {
				r = HEAP_node_ptr(k->heap_node);
				HEAP_remove(p->heap, k->heap_node);
			}
			else {
#ifdef USE_TPOOL
				r = refit_pool_elem_alloc(p->epool);
#else
				r = malloc(sizeof(*r));
#endif
				r->index = k->index;
			}

			r->index_refit = SPLIT_POINT_INVALID;

			r->handles_prev[0] = handles[0];
			r->handles_prev[1] = 0.0;  /* unused */
			r->handles_next[0] = 0.0;  /* unused */
			r->handles_next[1] = handles[1];

			r->error_sq[0] = r->error_sq[1] = cost_sq;

			/* Always perform removal before refitting, (make a negative number) */
			k->heap_node = HEAP_insert(p->heap, cost_sq - error_sq_max, r);

			return;
		}
	}
#else
	(void)error_sq_max;

	const uint refit_index = knot_find_split_point(
	         p->pd, k->prev, k->next,
	         knots_len,
	         dims);

#endif  /* USE_KNOT_REFIT_REMOVE */

	if ((refit_index == SPLIT_POINT_INVALID) ||
	    (refit_index == k->index))
	{
		goto remove;
	}

	struct Knot *k_refit = &knots[refit_index];

	const double cost_sq_src_max = MAX2(k->prev->error_sq_next, k->error_sq_next);
	assert(cost_sq_src_max <= error_sq_max);

	double cost_sq_dst[2];
	double handles_prev[2], handles_next[2];

	if ((((cost_sq_dst[0] = knot_calc_curve_error_value(
	           p->pd, k->prev, k_refit,
	           k->prev->tan[1], k_refit->tan[0],
	           dims,
	           handles_prev)) < cost_sq_src_max) &&
	     ((cost_sq_dst[1] = knot_calc_curve_error_value(
	           p->pd, k_refit, k->next,
	           k_refit->tan[1], k->next->tan[0],
	           dims,
	           handles_next)) < cost_sq_src_max)))
	{
		{
			struct KnotRefitState *r;
			if (k->heap_node) {
				r = HEAP_node_ptr(k->heap_node);
				HEAP_remove(p->heap, k->heap_node);
			}
			else {
#ifdef USE_TPOOL
				r = refit_pool_elem_alloc(p->epool);
#else
				r = malloc(sizeof(*r));
#endif
				r->index = k->index;
			}

			r->index_refit = refit_index;

			r->handles_prev[0] = handles_prev[0];
			r->handles_prev[1] = handles_prev[1];

			r->handles_next[0] = handles_next[0];
			r->handles_next[1] = handles_next[1];

			r->error_sq[0] = cost_sq_dst[0];
			r->error_sq[1] = cost_sq_dst[1];

			const double cost_sq_dst_max = MAX2(cost_sq_dst[0], cost_sq_dst[1]);

			assert(cost_sq_dst_max < cost_sq_src_max);

			/* Weight for the greatest improvement */
			k->heap_node = HEAP_insert(p->heap, cost_sq_src_max - cost_sq_dst_max, r);
		}
	}
	else {
remove:
		if (k->heap_node) {
			struct KnotRefitState *r;
			r = HEAP_node_ptr(k->heap_node);
			HEAP_remove(p->heap, k->heap_node);

#ifdef USE_TPOOL
			refit_pool_elem_free(p->epool, r);
#else
			free(r);
#endif

			k->heap_node = NULL;
		}
	}
}

/**
 * Re-adjust the curves by re-fitting points.
 * test the error from moving using points between the adjacent.
 */
static uint curve_incremental_simplify_refit(
        const struct PointData *pd,
        struct Knot *knots, const uint knots_len, uint knots_len_remaining,
        const double error_sq_max,
        const uint dims)
{
#ifdef USE_TPOOL
	struct ElemPool_KnotRefitState epool;

	refit_pool_create(&epool, 0);
#endif

	Heap *heap = HEAP_new(knots_len_remaining);

	struct KnotRefit_Params params = {
	    .pd = pd,
	    .heap = heap,
#ifdef USE_TPOOL
	    .epool = &epool,
#endif
	};

	for (uint i = 0; i < knots_len; i++) {
		struct Knot *k = &knots[i];
		if (k->can_remove &&
		    (k->is_removed == false) &&
		    (k->is_corner == false) &&
		    (k->prev && k->next))
		{
			knot_refit_error_recalculate(&params, knots, knots_len, k, error_sq_max, dims);
		}
	}

	while (HEAP_is_empty(heap) == false) {
		struct Knot *k_old, *k_refit;

		{
			struct KnotRefitState *r = HEAP_popmin(heap);
			k_old = &knots[r->index];
			k_old->heap_node = NULL;

#ifdef USE_KNOT_REFIT_REMOVE
			if (r->index_refit == SPLIT_POINT_INVALID) {
				k_refit = NULL;
			}
			else
#endif
			{
				k_refit = &knots[r->index_refit];
				k_refit->handles[0] = r->handles_prev[1];
				k_refit->handles[1] = r->handles_next[0];
			}

			k_old->prev->handles[1] = r->handles_prev[0];
			k_old->next->handles[0] = r->handles_next[1];

#ifdef USE_TPOOL
			refit_pool_elem_free(&epool, r);
#else
			free(r);
#endif
		}

		if (UNLIKELY(knots_len_remaining <= 2)) {
			continue;
		}

		struct Knot *k_prev = k_old->prev;
		struct Knot *k_next = k_old->next;

		k_old->next = NULL;
		k_old->prev = NULL;
		k_old->is_removed = true;

#ifdef USE_KNOT_REFIT_REMOVE
		if (k_refit == NULL) {
			k_next->prev = k_prev;
			k_prev->next = k_next;

			knots_len_remaining -= 1;
		}
		else
#endif
		{
			/* Remove ourselves */
			k_next->prev = k_refit;
			k_prev->next = k_refit;

			k_refit->prev = k_prev;
			k_refit->next = k_next;
			k_refit->is_removed = false;
		}

		if (k_prev->can_remove && (k_prev->is_corner == false) && (k_prev->prev && k_prev->next)) {
			knot_refit_error_recalculate(&params, knots, knots_len, k_prev, error_sq_max, dims);
		}

		if (k_next->can_remove && (k_next->is_corner == false) && (k_next->prev && k_next->next)) {
			knot_refit_error_recalculate(&params, knots, knots_len, k_next, error_sq_max, dims);
		}
	}

#ifdef USE_TPOOL
	refit_pool_destroy(&epool);
#endif

	HEAP_free(heap, free);

	return knots_len_remaining;
}

#endif  /* USE_KNOT_REFIT */


#ifdef USE_CORNER_DETECT

struct KnotCorner_Params {
	Heap *heap;
	const struct PointData *pd;
#ifdef USE_TPOOL
	struct ElemPool_KnotCornerState *epool;
#endif
};

/**
 * (Re)calculate the error incurred from turning this into a corner.
 */
static void knot_corner_error_recalculate(
        struct KnotCorner_Params *p,
        struct Knot *k_split,
        struct Knot *k_prev, struct Knot *k_next,
        const double error_sq_max,
        const uint dims)
{
	assert(k_prev->can_remove && k_next->can_remove);

	double handles_prev[2], handles_next[2];
	/* Test skipping 'k_prev' by using points (k_prev->prev to k_split) */
	double cost_sq_dst[2];

	if (((cost_sq_dst[0] = knot_calc_curve_error_value(
	          p->pd, k_prev, k_split,
	          k_prev->tan[1], k_prev->tan[1],
	          dims,
	          handles_prev)) < error_sq_max) &&
	    ((cost_sq_dst[1] = knot_calc_curve_error_value(
	          p->pd, k_split, k_next,
	          k_next->tan[0], k_next->tan[0],
	          dims,
	          handles_next)) < error_sq_max))
	{
		struct KnotCornerState *c;
		if (k_split->heap_node) {
			c = HEAP_node_ptr(k_split->heap_node);
			HEAP_remove(p->heap, k_split->heap_node);
		}
		else {
#ifdef USE_TPOOL
			c = corner_pool_elem_alloc(p->epool);
#else
			c = malloc(sizeof(*c));
#endif
			c->index = k_split->index;
		}

		c->index_adjacent[0] = k_prev->index;
		c->index_adjacent[1] = k_next->index;

		/* Need to store handle lengths for both sides */
		c->handles_prev[0] = handles_prev[0];
		c->handles_prev[1] = handles_prev[1];

		c->handles_next[0] = handles_next[0];
		c->handles_next[1] = handles_next[1];

		c->error_sq[0] = cost_sq_dst[0];
		c->error_sq[1] = cost_sq_dst[1];

		const double cost_max_sq = MAX2(cost_sq_dst[0], cost_sq_dst[1]);
		k_split->heap_node = HEAP_insert(p->heap, cost_max_sq, c);
	}
	else {
		if (k_split->heap_node) {
			struct KnotCornerState *c;
			c = HEAP_node_ptr(k_split->heap_node);
			HEAP_remove(p->heap, k_split->heap_node);
#ifdef USE_TPOOL
			corner_pool_elem_free(p->epool, c);
#else
			free(c);
#endif
			k_split->heap_node = NULL;
		}
	}
}


/**
 * Attempt to collapse close knots into corners,
 * as long as they fall below the error threshold.
 */
static uint curve_incremental_simplify_corners(
        const struct PointData *pd,
        struct Knot *knots, const uint knots_len, uint knots_len_remaining,
        const double error_sq_max, const double error_sq_collapse_max,
        const double corner_angle,
        const uint dims,
        uint *r_corner_index_len)
{
#ifdef USE_TPOOL
	struct ElemPool_KnotCornerState epool;

	corner_pool_create(&epool, 0);
#endif

	Heap *heap = HEAP_new(0);

	struct KnotCorner_Params params = {
	    .pd = pd,
	    .heap = heap,
#ifdef USE_TPOOL
	    .epool = &epool,
#endif
	};

#ifdef USE_VLA
	double plane_no[dims];
	double k_proj_ref[dims];
	double k_proj_split[dims];
#else
	double *plane_no =       alloca(sizeof(double) * dims);
	double *k_proj_ref =     alloca(sizeof(double) * dims);
	double *k_proj_split =   alloca(sizeof(double) * dims);
#endif

	const double corner_angle_cos = cos(corner_angle);

	uint corner_index_len = 0;

	for (uint i = 0; i < knots_len; i++) {
		if ((knots[i].is_removed == false) &&
		    (knots[i].can_remove == true) &&
		    (knots[i].next && knots[i].next->can_remove))
		{
			struct Knot *k_prev = &knots[i];
			struct Knot *k_next = k_prev->next;

			/* Angle outside threshold */
			if (dot_vnvn(k_prev->tan[0], k_next->tan[1], dims) < corner_angle_cos) {
				/* Measure distance projected onto a plane,
				 * since the points may be offset along their own tangents. */
				sub_vn_vnvn(plane_no, k_next->tan[0], k_prev->tan[1], dims);

				/* Compare 2x so as to allow both to be changed by maximum of error_sq_max */
				const uint split_index = knot_find_split_point_on_axis(
				        pd, k_prev, k_next,
				        knots_len,
				        plane_no,
				        dims);

				if (split_index != SPLIT_POINT_INVALID) {
					const double *co_prev  = &params.pd->points[k_prev->index * dims];
					const double *co_next  = &params.pd->points[k_next->index * dims];
					const double *co_split = &params.pd->points[split_index * dims];

					project_vn_vnvn_normalized(k_proj_ref,   co_prev, k_prev->tan[1], dims);
					project_vn_vnvn_normalized(k_proj_split, co_split, k_prev->tan[1], dims);

					if (len_squared_vnvn(k_proj_ref, k_proj_split, dims) < error_sq_collapse_max) {

						project_vn_vnvn_normalized(k_proj_ref,   co_next, k_next->tan[0], dims);
						project_vn_vnvn_normalized(k_proj_split, co_split, k_next->tan[0], dims);

						if (len_squared_vnvn(k_proj_ref, k_proj_split, dims) < error_sq_collapse_max) {

							struct Knot *k_split = &knots[split_index];

							knot_corner_error_recalculate(
							        &params,
							        k_split, k_prev, k_next,
							        error_sq_max,
							        dims);
						}
					}
				}
			}
		}
	}

	while (HEAP_is_empty(heap) == false) {
		struct KnotCornerState *c = HEAP_popmin(heap);

		struct Knot *k_split = &knots[c->index];

		/* Remove while collapsing */
		struct Knot *k_prev  = &knots[c->index_adjacent[0]];
		struct Knot *k_next  = &knots[c->index_adjacent[1]];

		/* Insert */
		k_split->is_removed = false;
		k_split->prev = k_prev;
		k_split->next = k_next;
		k_prev->next = k_split;
		k_next->prev = k_split;

		/* Update tangents */
		k_split->tan[0] = k_prev->tan[1];
		k_split->tan[1] = k_next->tan[0];

		/* Own handles */
		k_prev->handles[1]  = c->handles_prev[0];
		k_split->handles[0] = c->handles_prev[1];
		k_split->handles[1] = c->handles_next[0];
		k_next->handles[0]  = c->handles_next[1];

		k_prev->error_sq_next  = c->error_sq[0];
		k_split->error_sq_next = c->error_sq[1];

		k_split->heap_node = NULL;

#ifdef USE_TPOOL
		corner_pool_elem_free(&epool, c);
#else
		free(c);
#endif

		k_split->is_corner = true;

		knots_len_remaining++;

		corner_index_len++;
	}

#ifdef USE_TPOOL
	corner_pool_destroy(&epool);
#endif

	HEAP_free(heap, free);

	*r_corner_index_len = corner_index_len;

	return knots_len_remaining;
}

#endif  /* USE_CORNER_DETECT */

int curve_fit_cubic_to_points_refit_db(
        const double *points,
        const uint    points_len,
        const uint    dims,
        const double  error_threshold,
        const uint    calc_flag,
        const uint   *corners,
        const uint    corners_len,
        const double  corner_angle,

        double **r_cubic_array, uint *r_cubic_array_len,
        uint   **r_cubic_orig_index,
        uint   **r_corner_index_array, uint *r_corner_index_len)
{
	const uint knots_len = points_len;
	struct Knot *knots = malloc(sizeof(struct Knot) * knots_len);

#ifndef USE_CORNER_DETECT
	(void)r_corner_index_array;
	(void)r_corner_index_len;
#endif

(void)corners;
(void)corners_len;

	const bool is_cyclic = (calc_flag & CURVE_FIT_CALC_CYCLIC) != 0 && (points_len > 2);
#ifdef USE_CORNER_DETECT
	const bool use_corner = (corner_angle < M_PI);
#else
	(void)corner_angle;
#endif

	/* Over alloc the list x2 for cyclic curves,
	 * so we can evaluate across the start/end */
	double *points_alloc = NULL;
	if (is_cyclic) {
		points_alloc = malloc((sizeof(double) * points_len * dims) * 2);
		memcpy(points_alloc,                       points,       sizeof(double) * points_len * dims);
		memcpy(points_alloc + (points_len * dims), points_alloc, sizeof(double) * points_len * dims);
		points = points_alloc;
	}

	double *tangents = malloc(sizeof(double) * knots_len * 2 * dims);

	{
		double *t_step = tangents;
		for (uint i = 0; i < knots_len; i++) {
			knots[i].next = (knots + i) + 1;
			knots[i].prev = (knots + i) - 1;

			knots[i].heap_node = NULL;
			knots[i].index = i;
			knots[i].can_remove = true;
			knots[i].is_removed = false;
			knots[i].is_corner = false;
			knots[i].error_sq_next = 0.0;
			knots[i].tan[0] = t_step; t_step += dims;
			knots[i].tan[1] = t_step; t_step += dims;
		}
		assert(t_step == &tangents[knots_len * 2 * dims]);
	}

	if (is_cyclic) {
		knots[0].prev = &knots[knots_len - 1];
		knots[knots_len - 1].next = &knots[0];
	}
	else {
		knots[0].prev = NULL;
		knots[knots_len - 1].next = NULL;

		/* always keep end-points */
		knots[0].can_remove = false;
		knots[knots_len - 1].can_remove = false;
	}

#ifdef USE_LENGTH_CACHE
	double *points_length_cache = malloc(sizeof(double) * points_len * (is_cyclic ? 2 : 1));
#endif

	/* Initialize tangents,
	 * also set the values for knot handles since some may not collapse. */
	{
#ifdef USE_VLA
		double tan_prev[dims];
		double tan_next[dims];
#else
		double *tan_prev = alloca(sizeof(double) * dims);
		double *tan_next = alloca(sizeof(double) * dims);
#endif
		double len_prev, len_next;

#if 0
		/* 2x normalize calculations, but correct */

		for (uint i = 0; i < knots_len; i++) {
			Knot *k = &knots[i];

			if (k->prev) {
				sub_vn_vnvn(tan_prev, &points[k->prev->index * dims], &points[k->index * dims], dims);
#ifdef USE_LENGTH_CACHE
				points_length_cache[i] =
#endif
				len_prev = normalize_vn(tan_prev, dims);
			}
			else {
				zero_vn(tan_prev, dims);
				len_prev = 0.0;
			}

			if (k->next) {
				sub_vn_vnvn(tan_next, &points[k->index * dims], &points[k->next->index * dims], dims);
				len_next = normalize_vn(tan_next, dims);
			}
			else {
				zero_vn(tan_next, dims);
				len_next = 0.0;
			}

			add_vn_vnvn(k->tan[0], tan_prev, tan_next, dims);
			normalize_vn(k->tan[0], dims);
			copy_vnvn(k->tan[1], k->tan[0], dims);
			k->handles[0] = len_prev /  3;
			k->handles[1] = len_next / -3;
		}
#else
		if (knots_len < 2) {
			/* NOP, set dummy values */
			for (uint i = 0; i < knots_len; i++) {
				struct Knot *k = &knots[i];
				zero_vn(k->tan[0], dims);
				zero_vn(k->tan[1], dims);
				k->handles[0] = 0.0;
				k->handles[1] = 0.0;
#ifdef USE_LENGTH_CACHE
				points_length_cache[i] = 0.0;
#endif
			}
		}
		else if (is_cyclic) {
			len_prev = normalize_vn_vnvn(
			        tan_prev, &points[(knots_len - 2) * dims], &points[(knots_len - 1) * dims], dims);
			for (uint i_curr = knots_len - 1, i_next = 0; i_next < knots_len; i_curr = i_next++) {
				struct Knot *k = &knots[i_curr];
#ifdef USE_LENGTH_CACHE
				points_length_cache[i_next] =
#endif
				len_next = normalize_vn_vnvn(tan_next, &points[i_curr * dims], &points[i_next * dims], dims);

				add_vn_vnvn(k->tan[0], tan_prev, tan_next, dims);
				normalize_vn(k->tan[0], dims);
				copy_vnvn(k->tan[1], k->tan[0], dims);
				k->handles[0] = len_prev /  3;
				k->handles[1] = len_next / -3;

				copy_vnvn(tan_prev, tan_next, dims);
				len_prev = len_next;
			}
		}
		else {
#ifdef USE_LENGTH_CACHE
			points_length_cache[0] = 0.0;
			points_length_cache[1] =
#endif
			len_prev = normalize_vn_vnvn(
			        tan_prev, &points[0 * dims], &points[1 * dims], dims);
			copy_vnvn(knots[0].tan[0], tan_prev, dims);
			copy_vnvn(knots[0].tan[1], tan_prev, dims);
			knots[0].handles[0] = len_prev /  3;
			knots[0].handles[1] = len_prev / -3;

			for (uint i_curr = 1, i_next = 2; i_next < knots_len; i_curr = i_next++) {
				struct Knot *k = &knots[i_curr];

#ifdef USE_LENGTH_CACHE
				points_length_cache[i_next] =
#endif
				len_next = normalize_vn_vnvn(tan_next, &points[i_curr * dims], &points[i_next * dims], dims);

				add_vn_vnvn(k->tan[0], tan_prev, tan_next, dims);
				normalize_vn(k->tan[0], dims);
				copy_vnvn(k->tan[1], k->tan[0], dims);
				k->handles[0] = len_prev /  3;
				k->handles[1] = len_next / -3;

				copy_vnvn(tan_prev, tan_next, dims);
				len_prev = len_next;
			}
			copy_vnvn(knots[knots_len - 1].tan[0], tan_next, dims);
			copy_vnvn(knots[knots_len - 1].tan[1], tan_next, dims);

			knots[knots_len - 1].handles[0] = len_next /  3;
			knots[knots_len - 1].handles[1] = len_next / -3;
		}
#endif
	}

#ifdef USE_LENGTH_CACHE
	if (is_cyclic) {
		memcpy(&points_length_cache[points_len], points_length_cache, sizeof(double) * points_len);
	}
#endif


#if 0
	for (uint i = 0; i < knots_len; i++) {
		Knot *k = &knots[i];
		printf("TAN %.8f %.8f %.8f %.8f\n", k->tan[0][0], k->tan[0][1], k->tan[1][0], k->tan[0][1]);
	}
#endif

	const struct PointData pd = {
		.points = points,
		.points_len = points_len,
#ifdef USE_LENGTH_CACHE
		.points_length_cache = points_length_cache,
#endif
	};

	uint knots_len_remaining = knots_len;

	/* 'curve_incremental_simplify_refit' can be called here, but its very slow
	 * just remove all within the threshold first. */
	knots_len_remaining = curve_incremental_simplify(
	        &pd, knots, knots_len, knots_len_remaining,
	        SQUARE(error_threshold), dims);

#ifdef USE_CORNER_DETECT
	if (use_corner) {

#ifdef DEBUG
		for (uint i = 0; i < knots_len; i++) {
			assert(knots[i].heap_node == NULL);
		}
#endif

		knots_len_remaining = curve_incremental_simplify_corners(
		        &pd, knots, knots_len, knots_len_remaining,
		        SQUARE(error_threshold), SQUARE(error_threshold * 3),
		        corner_angle,
		        dims,
		        r_corner_index_len);
	}
#endif  /* USE_CORNER_DETECT */

#ifdef USE_KNOT_REFIT
	knots_len_remaining = curve_incremental_simplify_refit(
	        &pd, knots, knots_len, knots_len_remaining,
	        SQUARE(error_threshold),
	        dims);
#endif  /* USE_KNOT_REFIT */


#ifdef USE_CORNER_DETECT
	if (use_corner) {
		if (is_cyclic == false) {
			*r_corner_index_len += 2;
		}

		uint *corner_index_array = malloc(sizeof(uint) * (*r_corner_index_len));
		uint k_index = 0, c_index = 0;
		uint i = 0;

		if (is_cyclic == false) {
			corner_index_array[c_index++] = k_index;
			k_index++;
			i++;
		}

		for (; i < knots_len; i++) {
			if (knots[i].is_removed == false) {
				if (knots[i].is_corner == true) {
					corner_index_array[c_index++] = k_index;
				}
				k_index++;
			}
		}

		if (is_cyclic == false) {
			corner_index_array[c_index++] = k_index;
			k_index++;
		}

		assert(c_index == *r_corner_index_len);
		*r_corner_index_array = corner_index_array;
	}
#endif  /* USE_CORNER_DETECT */

#ifdef USE_LENGTH_CACHE
	free(points_length_cache);
#endif

	uint *cubic_orig_index = NULL;

	if (r_cubic_orig_index) {
		cubic_orig_index = malloc(sizeof(uint) * knots_len_remaining);
	}

	struct Knot *knots_first = NULL;
	{
		struct Knot *k;
		for (uint i = 0; i < knots_len; i++) {
			if (knots[i].is_removed == false) {
				knots_first = &knots[i];
				break;
			}
		}

		if (cubic_orig_index) {
			k = knots_first;
			for (uint i = 0; i < knots_len_remaining; i++, k = k->next) {
				cubic_orig_index[i] = k->index;
			}
		}
	}

	/* Correct unused handle endpoints - not essential, but nice behavior */
	if (is_cyclic == false) {
		struct Knot *knots_last = knots_first;
		while (knots_last->next) {
			knots_last = knots_last->next;
		}
		knots_first->handles[0] = -knots_first->handles[1];
		knots_last->handles[1]  = -knots_last->handles[0];
	}

	/* 3x for one knot and two handles */
	double *cubic_array = malloc(sizeof(double) * knots_len_remaining * 3 * dims);

	{
		double *c_step = cubic_array;
		struct Knot *k = knots_first;
		for (uint i = 0; i < knots_len_remaining; i++, k = k->next) {
			const double *p = &points[k->index * dims];

			madd_vn_vnvn_fl(c_step, p, k->tan[0], k->handles[0], dims);
			c_step += dims;
			copy_vnvn(c_step, p, dims);
			c_step += dims;
			madd_vn_vnvn_fl(c_step, p, k->tan[1], k->handles[1], dims);
			c_step += dims;
		}
		assert(c_step == &cubic_array[knots_len_remaining * 3 * dims]);
	}

	if (points_alloc) {
		free(points_alloc);
		points_alloc = NULL;
	}

	free(knots);
	free(tangents);

	if (r_cubic_orig_index) {
		*r_cubic_orig_index = cubic_orig_index;
	}

	*r_cubic_array = cubic_array;
	*r_cubic_array_len = knots_len_remaining;

	return 0;
}


int curve_fit_cubic_to_points_refit_fl(
        const float          *points,
        const unsigned int    points_len,
        const unsigned int    dims,
        const float           error_threshold,
        const unsigned int    calc_flag,
        const unsigned int   *corners,
        unsigned int          corners_len,
        const float           corner_angle,

        float **r_cubic_array, unsigned int *r_cubic_array_len,
        unsigned int   **r_cubic_orig_index,
        unsigned int   **r_corner_index_array, unsigned int *r_corner_index_len)
{
	const uint points_flat_len = points_len * dims;
	double *points_db = malloc(sizeof(double) * points_flat_len);

	copy_vndb_vnfl(points_db, points, points_flat_len);

	double *cubic_array_db = NULL;
	float  *cubic_array_fl = NULL;
	uint    cubic_array_len = 0;

	int result = curve_fit_cubic_to_points_refit_db(
	        points_db, points_len, dims, error_threshold, calc_flag, corners, corners_len,
	        corner_angle,
	        &cubic_array_db, &cubic_array_len,
	        r_cubic_orig_index,
	        r_corner_index_array, r_corner_index_len);
	free(points_db);

	if (!result) {
		uint cubic_array_flat_len = cubic_array_len * 3 * dims;
		cubic_array_fl = malloc(sizeof(float) * cubic_array_flat_len);
		for (uint i = 0; i < cubic_array_flat_len; i++) {
			cubic_array_fl[i] = (float)cubic_array_db[i];
		}
		free(cubic_array_db);
	}

	*r_cubic_array = cubic_array_fl;
	*r_cubic_array_len = cubic_array_len;

	return result;
}

