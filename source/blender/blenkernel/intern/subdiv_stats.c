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
 * The Original Code is Copyright (C) 2018 by Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/subdiv_stats.c
 *  \ingroup bke
 */

#include "BKE_subdiv.h"

#include <stdio.h>

#include "PIL_time.h"

void BKE_subdiv_stats_init(SubdivStats *stats)
{
	stats->topology_refiner_creation_time = 0.0;
	stats->subdiv_to_mesh_time = 0.0;
	stats->evaluator_creation_time = 0.0;
	stats->evaluator_refine_time = 0.0;
}

void BKE_subdiv_stats_begin(SubdivStats *stats, eSubdivStatsValue value)
{
	stats->begin_timestamp_[value] = PIL_check_seconds_timer();
}

void BKE_subdiv_stats_end(SubdivStats *stats, eSubdivStatsValue value)
{
	stats->values_[value] =
	        PIL_check_seconds_timer() - stats->begin_timestamp_[value];
}

void BKE_subdiv_stats_print(const SubdivStats *stats)
{
#define STATS_PRINT_TIME(stats, value, description)                 \
	do {                                                            \
		if ((stats)->value > 0.0) {                                 \
		  printf("  %s: %f (sec)\n", description, (stats)->value);  \
		}                                                           \
	} while (false)

	printf("Subdivision surface statistics:\n");

	STATS_PRINT_TIME(stats,
	                 topology_refiner_creation_time,
	                 "Topology refiner creation time");
	STATS_PRINT_TIME(stats,
	                 subdiv_to_mesh_time,
	                 "Subdivision to mesh time");
	STATS_PRINT_TIME(stats,
	                 evaluator_creation_time,
	                 "Evaluator creation time");
	STATS_PRINT_TIME(stats,
	                 evaluator_refine_time,
	                 "Evaluator refine time");

#undef STATS_PRINT_TIME
}
