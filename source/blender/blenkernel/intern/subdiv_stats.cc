/* SPDX-FileCopyrightText: 2018 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_subdiv.hh"

#include <cstdio>

#include "PIL_time.h"

void BKE_subdiv_stats_init(SubdivStats *stats)
{
  stats->topology_refiner_creation_time = 0.0;
  stats->subdiv_to_mesh_time = 0.0;
  stats->subdiv_to_mesh_geometry_time = 0.0;
  stats->evaluator_creation_time = 0.0;
  stats->evaluator_refine_time = 0.0;
  stats->subdiv_to_ccg_time = 0.0;
  stats->subdiv_to_ccg_elements_time = 0.0;
  stats->topology_compare_time = 0.0;
}

void BKE_subdiv_stats_begin(SubdivStats *stats, eSubdivStatsValue value)
{
  stats->begin_timestamp_[value] = PIL_check_seconds_timer();
}

void BKE_subdiv_stats_end(SubdivStats *stats, eSubdivStatsValue value)
{
  stats->values_[value] = PIL_check_seconds_timer() - stats->begin_timestamp_[value];
}

void BKE_subdiv_stats_reset(SubdivStats *stats, eSubdivStatsValue value)
{
  stats->values_[value] = 0.0;
}

void BKE_subdiv_stats_print(const SubdivStats *stats)
{
#define STATS_PRINT_TIME(stats, value, description) \
  do { \
    if ((stats)->value > 0.0) { \
      printf("  %s: %f (sec)\n", description, (stats)->value); \
    } \
  } while (false)

  printf("Subdivision surface statistics:\n");

  STATS_PRINT_TIME(stats, topology_refiner_creation_time, "Topology refiner creation time");
  STATS_PRINT_TIME(stats, subdiv_to_mesh_time, "Subdivision to mesh time");
  STATS_PRINT_TIME(stats, subdiv_to_mesh_geometry_time, "    Geometry time");
  STATS_PRINT_TIME(stats, evaluator_creation_time, "Evaluator creation time");
  STATS_PRINT_TIME(stats, evaluator_refine_time, "Evaluator refine time");
  STATS_PRINT_TIME(stats, subdiv_to_ccg_time, "Subdivision to CCG time");
  STATS_PRINT_TIME(stats, subdiv_to_ccg_elements_time, "    Elements time");
  STATS_PRINT_TIME(stats, topology_compare_time, "Topology comparison time");

#undef STATS_PRINT_TIME
}
