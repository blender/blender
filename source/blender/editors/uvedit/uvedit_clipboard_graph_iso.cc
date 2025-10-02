/* SPDX-FileCopyrightText: 2019 Stefano Quer
 * SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Originally 6846114 from https://github.com/stefanoquer/graphISO/blob/master/v3
 * graphISO: Tools to compute the Maximum Common Subgraph between two graphs.
 */

#include "uvedit_clipboard_graph_iso.hh"

#include "BLI_assert.h"

#include "MEM_guardedalloc.h"

#include <algorithm>
#include <climits>

#define L 0
#define R 1
#define LL 2
#define RL 3
#define ADJ 4
#define P 5
#define W 6
#define IRL 7

#define BDS 8

GraphISO::GraphISO(int n)
{
  this->n = n;
  label = static_cast<uint *>(MEM_mallocN(n * sizeof *label, __func__));
  adjmat = static_cast<uint8_t **>(MEM_mallocN(n * sizeof *adjmat, __func__));

  /* \note Allocation of `n * n` bytes total! */

  for (int i = 0; i < n; i++) {
    /* Caution, are you trying to change the representation of adjmat?
     * Consider `blender::Vector<std::pair<int, int>> adjmat;` instead.
     * Better still is to use a different algorithm. See for example:
     * https://www.uni-ulm.de/fileadmin/website_uni_ulm/iui.inst.190/Mitarbeiter/toran/beatcs09.pdf
     */
    adjmat[i] = static_cast<uchar *>(MEM_callocN(n * sizeof *adjmat[i], __func__));
  }
  degree = nullptr;
}

GraphISO::~GraphISO()
{
  for (int i = 0; i < n; i++) {
    MEM_freeN(adjmat[i]);
  }
  MEM_freeN(adjmat);
  MEM_freeN(label);
  if (degree) {
    MEM_freeN(degree);
  }
}

void GraphISO::add_edge(int v, int w)
{
  BLI_assert(v != w);
  adjmat[v][w] = 1;
  adjmat[w][v] = 1;
}

void GraphISO::calculate_degrees() const
{
  if (degree) {
    return;
  }
  degree = static_cast<uint *>(MEM_mallocN(n * sizeof *degree, __func__));
  for (int v = 0; v < n; v++) {
    int row_count = 0;
    for (int w = 0; w < n; w++) {
      if (adjmat[v][w]) {
        row_count++;
      }
    }
    degree[v] = row_count;
  }
}

class GraphISO_DegreeCompare {
 public:
  GraphISO_DegreeCompare(const GraphISO *g)
  {
    this->g = g;
  }
  const GraphISO *g;

  bool operator()(int i, int j) const
  {
    return g->degree[i] < g->degree[j];
  }
};

GraphISO *GraphISO::sort_vertices_by_degree() const
{
  calculate_degrees();

  int *vv = static_cast<int *>(MEM_mallocN(n * sizeof *vv, __func__));
  for (int i = 0; i < n; i++) {
    vv[i] = i;
  }
  /* Currently ordering iso_verts by degree.
   * Instead should order iso_verts by frequency of degree. */
  GraphISO_DegreeCompare compare_obj(this);
  std::sort(vv, vv + n, compare_obj);

  GraphISO *subg = new GraphISO(n);
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      subg->adjmat[i][j] = adjmat[vv[i]][vv[j]];
    }
  }
  for (int i = 0; i < n; i++) {
    subg->label[i] = label[vv[i]];
  }
  subg->calculate_degrees();

  MEM_freeN(vv);
  return subg;
}

static void update_incumbent(uint8_t cur[][2], int inc[][2], int cur_pos, int *inc_pos)
{
  if (cur_pos > *inc_pos) {
    *inc_pos = cur_pos;
    for (int i = 0; i < cur_pos; i++) {
      inc[i][L] = cur[i][L];
      inc[i][R] = cur[i][R];
    }
  }
}

static void add_bidomain(uint8_t domains[][BDS],
                         int *bd_pos,
                         uint8_t left_i,
                         uint8_t right_i,
                         uint8_t left_len,
                         uint8_t right_len,
                         uint8_t is_adjacent,
                         uint8_t cur_pos)
{
  domains[*bd_pos][L] = left_i;
  domains[*bd_pos][R] = right_i;
  domains[*bd_pos][LL] = left_len;
  domains[*bd_pos][RL] = right_len;
  domains[*bd_pos][ADJ] = is_adjacent;
  domains[*bd_pos][P] = cur_pos;
  domains[*bd_pos][W] = UINT8_MAX;
  domains[*bd_pos][IRL] = right_len;
  (*bd_pos)++;
}

static int calc_bound(const uint8_t domains[][BDS], int bd_pos, int cur_pos)
{
  int bound = 0;
  for (int i = bd_pos - 1; i >= 0 && domains[i][P] == cur_pos; i--) {
    bound += std::min(domains[i][LL], domains[i][IRL]);
  }
  return bound;
}

static int partition(uint8_t *arr, int start, int len, const uint8_t *adjrow)
{
  int i = 0;
  for (int j = 0; j < len; j++) {
    if (adjrow[arr[start + j]]) {
      std::swap(arr[start + i], arr[start + j]);
      i++;
    }
  }
  return i;
}

static void generate_next_domains(uint8_t domains[][BDS],
                                  int *bd_pos,
                                  int cur_pos,
                                  uint8_t *left,
                                  uint8_t *right,
                                  uint8_t v,
                                  uint8_t w,
                                  int inc_pos,
                                  uint8_t **adjmat0,
                                  uint8_t **adjmat1)
{
  int i;
  int bd_backup = *bd_pos;
  int bound = 0;
  uint8_t *bd;
  for (i = *bd_pos - 1, bd = &domains[i][L]; i >= 0 && bd[P] == cur_pos - 1;
       i--, bd = &domains[i][L])
  {

    uint8_t l_len = partition(left, bd[L], bd[LL], adjmat0[v]);
    uint8_t r_len = partition(right, bd[R], bd[RL], adjmat1[w]);

    if (bd[LL] - l_len && bd[RL] - r_len) {
      add_bidomain(domains,
                   bd_pos,
                   bd[L] + l_len,
                   bd[R] + r_len,
                   bd[LL] - l_len,
                   bd[RL] - r_len,
                   bd[ADJ],
                   uint8_t(cur_pos));
      bound += std::min(bd[LL] - l_len, bd[RL] - r_len);
    }
    if (l_len && r_len) {
      add_bidomain(domains, bd_pos, bd[L], bd[R], l_len, r_len, true, uint8_t(cur_pos));
      bound += std::min(l_len, r_len);
    }
  }
  if (cur_pos + bound <= inc_pos) {
    *bd_pos = bd_backup;
  }
}

static uint8_t select_next_v(uint8_t *left, uint8_t *bd)
{
  uint8_t min = UINT8_MAX;
  uint8_t idx = UINT8_MAX;
  if (bd[RL] != bd[IRL]) {
    return left[bd[L] + bd[LL]];
  }
  for (uint8_t i = 0; i < bd[LL]; i++) {
    if (left[bd[L] + i] < min) {
      min = left[bd[L] + i];
      idx = i;
    }
  }
  std::swap(left[bd[L] + idx], left[bd[L] + bd[LL] - 1]);
  bd[LL]--;
  bd[RL]--;
  return min;
}

static uint8_t find_min_value(const uint8_t *arr, uint8_t start_idx, uint8_t len)
{
  uint8_t min_v = UINT8_MAX;
  for (int i = 0; i < len; i++) {
    min_v = std::min(arr[start_idx + i], min_v);
  }
  return min_v;
}

static void select_bidomain(uint8_t domains[][BDS],
                            int bd_pos,
                            const uint8_t *left,
                            int current_matching_size,
                            bool connected)
{
  int i;
  int min_size = INT_MAX;
  int min_tie_breaker = INT_MAX;
  int best = INT_MAX;
  uint8_t *bd;
  for (i = bd_pos - 1, bd = &domains[i][L]; i >= 0 && bd[P] == current_matching_size;
       i--, bd = &domains[i][L])
  {
    if (connected && current_matching_size > 0 && !bd[ADJ]) {
      continue;
    }
    int len = bd[LL] > bd[RL] ? bd[LL] : bd[RL];
    if (len < min_size) {
      min_size = len;
      min_tie_breaker = find_min_value(left, bd[L], bd[LL]);
      best = i;
    }
    else if (len == min_size) {
      int tie_breaker = find_min_value(left, bd[L], bd[LL]);
      if (tie_breaker < min_tie_breaker) {
        min_tie_breaker = tie_breaker;
        best = i;
      }
    }
  }
  if (best != INT_MAX && best != bd_pos - 1) {
    uint8_t tmp[BDS];
    for (i = 0; i < BDS; i++) {
      tmp[i] = domains[best][i];
    }
    for (i = 0; i < BDS; i++) {
      domains[best][i] = domains[bd_pos - 1][i];
    }
    for (i = 0; i < BDS; i++) {
      domains[bd_pos - 1][i] = tmp[i];
    }
  }
}

static uint8_t select_next_w(const uint8_t *right, uint8_t *bd)
{
  uint8_t min = UINT8_MAX;
  uint8_t idx = UINT8_MAX;
  for (uint8_t i = 0; i < bd[RL] + 1; i++) {
    if ((right[bd[R] + i] > bd[W] || bd[W] == UINT8_MAX) && right[bd[R] + i] < min) {
      min = right[bd[R] + i];
      idx = i;
    }
  }
  if (idx == UINT8_MAX) {
    bd[RL]++;
  }
  return idx;
}

static void maximum_common_subgraph_internal(int incumbent[][2],
                                             int *inc_pos,
                                             uint8_t **adjmat0,
                                             int n0,
                                             uint8_t **adjmat1,
                                             int n1,
                                             bool *r_search_abandoned)
{
  int min = std::min(n0, n1);

  uint8_t (*cur)[2] = (uint8_t (*)[2])MEM_mallocN(min * sizeof(*cur), __func__);
  uint8_t (*domains)[BDS] = (uint8_t (*)[8])MEM_mallocN(min * min * sizeof(*domains), __func__);
  uint8_t *left = static_cast<uint8_t *>(MEM_mallocN(n0 * sizeof *left, __func__));
  uint8_t *right = static_cast<uint8_t *>(MEM_mallocN(n1 * sizeof *right, __func__));

  uint8_t v, w, *bd;
  int bd_pos = 0;
  for (int i = 0; i < n0; i++) {
    left[i] = i;
  }
  for (int i = 0; i < n1; i++) {
    right[i] = i;
  }
  add_bidomain(domains, &bd_pos, 0, 0, n0, n1, 0, 0);

  int iteration_count = 0;

  while (bd_pos > 0) {
    if (iteration_count++ > 10000000) {
      /* Unlikely to find a solution, may as well give up.
       * Can occur with moderate sized inputs where the graph has lots of symmetry, e.g. a cube
       * subdivided 3x times.
       */
      *r_search_abandoned = true;
      *inc_pos = 0;
      break;
    }
    bd = &domains[bd_pos - 1][L];
    if (calc_bound(domains, bd_pos, bd[P]) + bd[P] <= *inc_pos ||
        (bd[LL] == 0 && bd[RL] == bd[IRL]))
    {
      bd_pos--;
    }
    else {
      const bool connected = false;
      select_bidomain(domains, bd_pos, left, domains[bd_pos - 1][P], connected);
      v = select_next_v(left, bd);
      if ((bd[W] = select_next_w(right, bd)) != UINT8_MAX) {
        w = right[bd[R] + bd[W]]; /* Swap the W after the bottom of the current right domain. */
        right[bd[R] + bd[W]] = right[bd[R] + bd[RL]];
        right[bd[R] + bd[RL]] = w;
        bd[W] = w; /* Store the W used for this iteration. */
        cur[bd[P]][L] = v;
        cur[bd[P]][R] = w;
        update_incumbent(cur, incumbent, bd[P] + uint8_t(1), inc_pos);
        generate_next_domains(
            domains, &bd_pos, bd[P] + 1, left, right, v, w, *inc_pos, adjmat0, adjmat1);
      }
    }
  }

  MEM_freeN(cur);
  MEM_freeN(domains);
  MEM_freeN(right);
  MEM_freeN(left);
}

static bool check_automorphism(const GraphISO *g0,
                               const GraphISO *g1,
                               int solution[][2],
                               int *solution_length)
{
  if (g0->n != g1->n) {
    return false;
  }
  for (int i = 0; i < g0->n; i++) {
    if (g0->label[i] != g1->label[i]) {
      return false;
    }
    for (int j = 0; j < g0->n; j++) {
      if (g0->adjmat[i][j] != g1->adjmat[i][j]) {
        return false;
      }
    }
    solution[i][0] = i;
    solution[i][1] = i;
  }
  *solution_length = g0->n;
  return true;
}

bool ED_uvedit_clipboard_maximum_common_subgraph(GraphISO *g0_input,
                                                 GraphISO *g1_input,
                                                 int solution[][2],
                                                 int *solution_length,
                                                 bool *r_search_abandoned)
{
  if (check_automorphism(g0_input, g1_input, solution, solution_length)) {
    return true;
  }

  int n0 = g0_input->n;
  int n1 = g1_input->n;

  int min_size = std::min(n0, n1);
  if (min_size >= UINT8_MAX - 2) {
    return false;
  }

  GraphISO *g0 = g0_input->sort_vertices_by_degree();
  GraphISO *g1 = g1_input->sort_vertices_by_degree();

  int sol_len = 0;
  maximum_common_subgraph_internal(
      solution, &sol_len, g0->adjmat, n0, g1->adjmat, n1, r_search_abandoned);
  *solution_length = sol_len;

  bool result = (sol_len == n0);
  if (result) {
    for (int i = 0; i < sol_len; i++) {
      solution[i][0] = g0->label[solution[i][0]]; /* Index from input. */
      solution[i][1] = g1->label[solution[i][1]];
    }
  }

  delete g1;
  delete g0;

  return result;
}
