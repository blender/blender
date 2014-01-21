/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Campbell Barton
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/PIL_time_utildefines.h
 *  \ingroup bli
 *  \brief Utility defines for timing/benchmarks.
 */

#ifndef __PIL_TIME_UTILDEFINES_H__
#define __PIL_TIME_UTILDEFINES_H__

#include "PIL_time.h"  /* for PIL_check_seconds_timer */
#include "BLI_utildefines.h"  /* for AT */

#define TIMEIT_START(var)                                                     \
	{                                                                         \
		double _timeit_##var = PIL_check_seconds_timer();                     \
		printf("time start (" #var "):  " AT "\n");                           \
		fflush(stdout);                                                       \
		{ (void)0

/**
 * \return the time since TIMEIT_START was called.
 */
#define TIMEIT_VALUE(var) (float)(PIL_check_seconds_timer() - _timeit_##var)

#define TIMEIT_VALUE_PRINT(var)                                               \
	{                                                                         \
		printf("time update(" #var "): %.6f" "  " AT "\n", TIMEIT_VALUE(var));\
		fflush(stdout);                                                       \
	} (void)0

#define TIMEIT_END(var)                                                       \
	}                                                                         \
	printf("time end   (" #var "): %.6f" "  " AT "\n", TIMEIT_VALUE(var));    \
	fflush(stdout);                                                           \
} (void)0

/**
 * Given some function/expression:
 *   TIMEIT_BENCH(some_function(), some_unique_description);
 */
#define TIMEIT_BENCH(expr, id)                                                \
	{                                                                         \
		TIMEIT_START(id);                                                     \
		(expr);                                                               \
		TIMEIT_END(id);                                                       \
	} (void)0

#define TIMEIT_BLOCK_INIT(id)                                                 \
	double _timeit_var_##id = 0

#define TIMEIT_BLOCK_START(id)                                                \
	{                                                                         \
		double _timeit_block_start_##id = PIL_check_seconds_timer();          \
		{ (void)0

#define TIMEIT_BLOCK_END(id)                                                  \
		}                                                                     \
		_timeit_var_##id += (PIL_check_seconds_timer() -                      \
		                     _timeit_block_start_##id);                       \
	} (void)0

#define TIMEIT_BLOCK_STATS(id)                                                \
	{                                                                         \
		printf("%s time (in seconds): %f\n", #id, _timeit_var_##id);          \
		fflush(stdout);                                                       \
	} (void)0

#endif  /* __PIL_TIME_UTILDEFINES_H__ */
