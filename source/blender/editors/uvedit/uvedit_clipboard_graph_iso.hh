/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (c) 2019 Stefano Quer.
 * Additional code, copyright 2022 Blender Foundation
 *
 * Originally 6846114 from https://github.com/stefanoquer/graphISO/blob/master/v3
 * graphISO: Tools to compute the Maximum Common Subgraph between two graphs.
 */

/** \file
 * \ingroup eduv
 */

#pragma once

#include "BLI_sys_types.h"

/* A thin representation of a "Graph" in graph theory. */
class GraphISO {
 public:
  GraphISO(int n);
  ~GraphISO();
  int n;
  uint8_t **adjmat;
  uint *label;
  mutable uint *degree;

  void add_edge(int v, int w);
  GraphISO *sort_vertices_by_degree() const;

 private:
  void calculate_degrees() const;
};

/**
 * Find the maximum common subgraph between two graphs.
 * (Can be used to find graph ismorphism.)
 * \return True when found.
 */
bool ED_uvedit_clipboard_maximum_common_subgraph(
    GraphISO *, GraphISO *, int solution[][2], int *solution_length, bool *r_search_abandoned);
