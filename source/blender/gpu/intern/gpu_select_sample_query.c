/*
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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * Interface for accessing gpu-related methods for selection. The semantics will be
 * similar to glRenderMode(GL_SELECT) since the goal is to maintain compatibility.
 */

#include <stdlib.h>

#include "GPU_immediate.h"
#include "GPU_draw.h"
#include "GPU_select.h"
#include "GPU_extensions.h"
#include "GPU_glew.h"

#include "MEM_guardedalloc.h"

#include "BLI_rect.h"

#include "BLI_utildefines.h"

#include "PIL_time.h"

#include "BKE_global.h"

#include "gpu_select_private.h"

/* Ad hoc number of queries to allocate to skip doing many glGenQueries */
#define ALLOC_QUERIES 200

typedef struct GPUQueryState {
  /* Tracks whether a query has been issued so that gpu_load_id can end the previous one */
  bool query_issued;
  /* array holding the OpenGL query identifiers */
  uint *queries;
  /* array holding the id corresponding to each query */
  uint *id;
  /* number of queries in *queries and *id */
  uint num_of_queries;
  /* index to the next query to start */
  uint active_query;
  /* cache on initialization */
  uint (*buffer)[4];
  /* buffer size (stores number of integers, for actual size multiply by sizeof integer)*/
  uint bufsize;
  /* mode of operation */
  char mode;
  uint index;
  int oldhits;
} GPUQueryState;

static GPUQueryState g_query_state = {0};

void gpu_select_query_begin(
    uint (*buffer)[4], uint bufsize, const rcti *input, char mode, int oldhits)
{
  float viewport[4];

  g_query_state.query_issued = false;
  g_query_state.active_query = 0;
  g_query_state.num_of_queries = 0;
  g_query_state.bufsize = bufsize;
  g_query_state.buffer = buffer;
  g_query_state.mode = mode;
  g_query_state.index = 0;
  g_query_state.oldhits = oldhits;

  g_query_state.num_of_queries = ALLOC_QUERIES;

  g_query_state.queries = MEM_mallocN(
      g_query_state.num_of_queries * sizeof(*g_query_state.queries), "gpu selection queries");
  g_query_state.id = MEM_mallocN(g_query_state.num_of_queries * sizeof(*g_query_state.id),
                                 "gpu selection ids");
  glGenQueries(g_query_state.num_of_queries, g_query_state.queries);

  gpuPushAttr(GPU_DEPTH_BUFFER_BIT | GPU_VIEWPORT_BIT | GPU_SCISSOR_BIT);
  /* disable writing to the framebuffer */
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

  /* In order to save some fill rate we minimize the viewport using rect.
   * We need to get the region of the viewport so that our geometry doesn't
   * get rejected before the depth test. Should probably cull rect against
   * the viewport but this is a rare case I think */
  glGetFloatv(GL_VIEWPORT, viewport);
  glViewport(viewport[0], viewport[1], BLI_rcti_size_x(input), BLI_rcti_size_y(input));

  /* occlusion queries operates on fragments that pass tests and since we are interested on all
   * objects in the view frustum independently of their order, we need to disable the depth test */
  if (mode == GPU_SELECT_ALL) {
    /* glQueries on Windows+Intel drivers only works with depth testing turned on.
     * See T62947 for details */
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glDepthMask(GL_TRUE);
  }
  else if (mode == GPU_SELECT_NEAREST_FIRST_PASS) {
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
  }
  else if (mode == GPU_SELECT_NEAREST_SECOND_PASS) {
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_EQUAL);
  }
}

bool gpu_select_query_load_id(uint id)
{
  if (g_query_state.query_issued) {
    glEndQuery(GL_SAMPLES_PASSED);
  }
  /* if required, allocate extra queries */
  if (g_query_state.active_query == g_query_state.num_of_queries) {
    g_query_state.num_of_queries += ALLOC_QUERIES;
    g_query_state.queries = MEM_reallocN(
        g_query_state.queries, g_query_state.num_of_queries * sizeof(*g_query_state.queries));
    g_query_state.id = MEM_reallocN(g_query_state.id,
                                    g_query_state.num_of_queries * sizeof(*g_query_state.id));
    glGenQueries(ALLOC_QUERIES, &g_query_state.queries[g_query_state.active_query]);
  }

  glBeginQuery(GL_SAMPLES_PASSED, g_query_state.queries[g_query_state.active_query]);
  g_query_state.id[g_query_state.active_query] = id;
  g_query_state.active_query++;
  g_query_state.query_issued = true;

  if (g_query_state.mode == GPU_SELECT_NEAREST_SECOND_PASS) {
    /* Second pass should never run if first pass fails, can read past 'bufsize' in this case. */
    BLI_assert(g_query_state.oldhits != -1);
    if (g_query_state.index < g_query_state.oldhits) {
      if (g_query_state.buffer[g_query_state.index][3] == id) {
        g_query_state.index++;
        return true;
      }
      else {
        return false;
      }
    }
  }

  return true;
}

uint gpu_select_query_end(void)
{
  int i;

  uint hits = 0;
  const uint maxhits = g_query_state.bufsize;

  if (g_query_state.query_issued) {
    glEndQuery(GL_SAMPLES_PASSED);
  }

  for (i = 0; i < g_query_state.active_query; i++) {
    uint result = 0;
    /* Wait until the result is available. */
    while (result == 0) {
      glGetQueryObjectuiv(g_query_state.queries[i], GL_QUERY_RESULT_AVAILABLE, &result);
      if (result == 0) {
        /* (fclem) Not sure if this is better than calling glGetQueryObjectuiv() indefinitely.
         * (brecht) Added debug test for lagging issue in T61474. */
        if (G.debug_value != 474) {
          PIL_sleep_ms(1);
        }
      }
    }
    glGetQueryObjectuiv(g_query_state.queries[i], GL_QUERY_RESULT, &result);
    if (result > 0) {
      if (g_query_state.mode != GPU_SELECT_NEAREST_SECOND_PASS) {

        if (hits < maxhits) {
          g_query_state.buffer[hits][0] = 1;
          g_query_state.buffer[hits][1] = 0xFFFF;
          g_query_state.buffer[hits][2] = 0xFFFF;
          g_query_state.buffer[hits][3] = g_query_state.id[i];

          hits++;
        }
        else {
          hits = -1;
          break;
        }
      }
      else {
        int j;
        /* search in buffer and make selected object first */
        for (j = 0; j < g_query_state.oldhits; j++) {
          if (g_query_state.buffer[j][3] == g_query_state.id[i]) {
            g_query_state.buffer[j][1] = 0;
            g_query_state.buffer[j][2] = 0;
          }
        }
        break;
      }
    }
  }

  glDeleteQueries(g_query_state.num_of_queries, g_query_state.queries);
  MEM_freeN(g_query_state.queries);
  MEM_freeN(g_query_state.id);
  gpuPopAttr();
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  return hits;
}
