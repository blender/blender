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
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup editors
 */

#include <stdio.h>
#include <stdlib.h>
/* #include <time.h> */

#include <math.h>

#include "MEM_guardedalloc.h"

#include "MOD_lineart.h"

#include "BLI_math.h"

#include "lineart_intern.h"

/* Line art memory and list helper */

void *lineart_list_append_pointer_pool(ListBase *h, LineartStaticMemPool *smp, void *data)
{
  LinkData *lip;
  if (h == NULL) {
    return 0;
  }
  lip = lineart_mem_aquire(smp, sizeof(LinkData));
  lip->data = data;
  BLI_addtail(h, lip);
  return lip;
}
void *lineart_list_append_pointer_pool_sized(ListBase *h,
                                             LineartStaticMemPool *smp,
                                             void *data,
                                             int size)
{
  LinkData *lip;
  if (h == NULL) {
    return 0;
  }
  lip = lineart_mem_aquire(smp, size);
  lip->data = data;
  BLI_addtail(h, lip);
  return lip;
}

void *lineart_list_pop_pointer_no_free(ListBase *h)
{
  LinkData *lip;
  void *rev = 0;
  if (h == NULL) {
    return 0;
  }
  lip = BLI_pophead(h);
  rev = lip ? lip->data : 0;
  return rev;
}
void lineart_list_remove_pointer_item_no_free(ListBase *h, LinkData *lip)
{
  BLI_remlink(h, (void *)lip);
}

LineartStaticMemPoolNode *lineart_mem_new_static_pool(LineartStaticMemPool *smp, size_t size)
{
  size_t set_size = size;
  if (set_size < LRT_MEMORY_POOL_64MB) {
    set_size = LRT_MEMORY_POOL_64MB; /* Prevent too many small allocations. */
  }
  size_t total_size = size + sizeof(LineartStaticMemPoolNode);
  LineartStaticMemPoolNode *smpn = MEM_callocN(total_size, "mempool");
  smpn->size = total_size;
  smpn->used_byte = sizeof(LineartStaticMemPoolNode);
  BLI_addhead(&smp->pools, smpn);
  return smpn;
}
void *lineart_mem_aquire(LineartStaticMemPool *smp, size_t size)
{
  LineartStaticMemPoolNode *smpn = smp->pools.first;
  void *ret;

  if (!smpn || (smpn->used_byte + size) > smpn->size) {
    smpn = lineart_mem_new_static_pool(smp, size);
  }

  ret = ((unsigned char *)smpn) + smpn->used_byte;

  smpn->used_byte += size;

  return ret;
}
void *lineart_mem_aquire_thread(LineartStaticMemPool *smp, size_t size)
{
  void *ret;

  BLI_spin_lock(&smp->lock_mem);

  LineartStaticMemPoolNode *smpn = smp->pools.first;

  if (!smpn || (smpn->used_byte + size) > smpn->size) {
    smpn = lineart_mem_new_static_pool(smp, size);
  }

  ret = ((unsigned char *)smpn) + smpn->used_byte;

  smpn->used_byte += size;

  BLI_spin_unlock(&smp->lock_mem);

  return ret;
}
void lineart_mem_destroy(LineartStaticMemPool *smp)
{
  LineartStaticMemPoolNode *smpn;
  while ((smpn = BLI_pophead(&smp->pools)) != NULL) {
    MEM_freeN(smpn);
  }
}

void lineart_prepend_edge_direct(LineartEdge **first, void *node)
{
  LineartEdge *e_n = (LineartEdge *)node;
  e_n->next = (*first);
  (*first) = e_n;
}

void lineart_prepend_pool(LinkNode **first, LineartStaticMemPool *smp, void *link)
{
  LinkNode *ln = lineart_mem_aquire_thread(smp, sizeof(LinkNode));
  ln->next = (*first);
  ln->link = link;
  (*first) = ln;
}

/* =======================================================================[str] */

void lineart_matrix_perspective_44d(
    double (*mProjection)[4], double fFov_rad, double fAspect, double zMin, double zMax)
{
  double yMax;
  double yMin;
  double xMin;
  double xMax;

  if (fAspect < 1) {
    yMax = zMin * tan(fFov_rad * 0.5f);
    yMin = -yMax;
    xMin = yMin * fAspect;
    xMax = -xMin;
  }
  else {
    xMax = zMin * tan(fFov_rad * 0.5f);
    xMin = -xMax;
    yMin = xMin / fAspect;
    yMax = -yMin;
  }

  unit_m4_db(mProjection);

  mProjection[0][0] = (2.0f * zMin) / (xMax - xMin);
  mProjection[1][1] = (2.0f * zMin) / (yMax - yMin);
  mProjection[2][0] = (xMax + xMin) / (xMax - xMin);
  mProjection[2][1] = (yMax + yMin) / (yMax - yMin);
  mProjection[2][2] = -((zMax + zMin) / (zMax - zMin));
  mProjection[2][3] = -1.0f;
  mProjection[3][2] = -((2.0f * (zMax * zMin)) / (zMax - zMin));
  mProjection[3][3] = 0.0f;
}
void lineart_matrix_ortho_44d(double (*mProjection)[4],
                              double xMin,
                              double xMax,
                              double yMin,
                              double yMax,
                              double zMin,
                              double zMax)
{
  unit_m4_db(mProjection);

  mProjection[0][0] = 2.0f / (xMax - xMin);
  mProjection[1][1] = 2.0f / (yMax - yMin);
  mProjection[2][2] = -2.0f / (zMax - zMin);
  mProjection[3][0] = -((xMax + xMin) / (xMax - xMin));
  mProjection[3][1] = -((yMax + yMin) / (yMax - yMin));
  mProjection[3][2] = -((zMax + zMin) / (zMax - zMin));
  mProjection[3][3] = 1.0f;
}

void lineart_count_and_print_render_buffer_memory(LineartRenderBuffer *rb)
{
  size_t total = 0;
  size_t sum_this = 0;
  size_t count_this = 0;

  LISTBASE_FOREACH (LineartStaticMemPoolNode *, smpn, &rb->render_data_pool.pools) {
    count_this++;
    sum_this += LRT_MEMORY_POOL_64MB;
  }
  printf("LANPR Memory allocated %zu Standalone nodes, total %zu Bytes.\n", count_this, sum_this);
  total += sum_this;
  sum_this = 0;
  count_this = 0;

  LISTBASE_FOREACH (LineartElementLinkNode *, reln, &rb->line_buffer_pointers) {
    count_this++;
    sum_this += reln->element_count * sizeof(LineartEdge);
  }
  printf("             allocated %zu edge blocks, total %zu Bytes.\n", count_this, sum_this);
  total += sum_this;
  sum_this = 0;
  count_this = 0;

  LISTBASE_FOREACH (LineartElementLinkNode *, reln, &rb->triangle_buffer_pointers) {
    count_this++;
    sum_this += reln->element_count * rb->triangle_size;
  }
  printf("             allocated %zu triangle blocks, total %zu Bytes.\n", count_this, sum_this);
  total += sum_this;
  sum_this = 0;
  count_this = 0;
}
