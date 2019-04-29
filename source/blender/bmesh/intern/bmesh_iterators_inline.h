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
 */

/** \file
 * \ingroup bmesh
 *
 * BMesh inline iterator functions.
 */

#ifndef __BMESH_ITERATORS_INLINE_H__
#define __BMESH_ITERATORS_INLINE_H__

/* inline here optimizes out the switch statement when called with
 * constant values (which is very common), nicer for loop-in-loop situations */

/**
 * \brief Iterator Step
 *
 * Calls an iterators step function to return the next element.
 */
ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1) BLI_INLINE void *BM_iter_step(BMIter *iter)
{
  return iter->step(iter);
}

/**
 * \brief Iterator Init
 *
 * Takes a bmesh iterator structure and fills
 * it with the appropriate function pointers based
 * upon its type.
 */
ATTR_NONNULL(1)
BLI_INLINE bool BM_iter_init(BMIter *iter, BMesh *bm, const char itype, void *data)
{
  /* int argtype; */
  iter->itype = itype;

  /* inlining optimizes out this switch when called with the defined type */
  switch ((BMIterType)itype) {
    case BM_VERTS_OF_MESH:
      BLI_assert(bm != NULL);
      BLI_assert(data == NULL);
      iter->begin = (BMIter__begin_cb)bmiter__elem_of_mesh_begin;
      iter->step = (BMIter__step_cb)bmiter__elem_of_mesh_step;
      iter->data.elem_of_mesh.pooliter.pool = bm->vpool;
      break;
    case BM_EDGES_OF_MESH:
      BLI_assert(bm != NULL);
      BLI_assert(data == NULL);
      iter->begin = (BMIter__begin_cb)bmiter__elem_of_mesh_begin;
      iter->step = (BMIter__step_cb)bmiter__elem_of_mesh_step;
      iter->data.elem_of_mesh.pooliter.pool = bm->epool;
      break;
    case BM_FACES_OF_MESH:
      BLI_assert(bm != NULL);
      BLI_assert(data == NULL);
      iter->begin = (BMIter__begin_cb)bmiter__elem_of_mesh_begin;
      iter->step = (BMIter__step_cb)bmiter__elem_of_mesh_step;
      iter->data.elem_of_mesh.pooliter.pool = bm->fpool;
      break;
    case BM_EDGES_OF_VERT:
      BLI_assert(data != NULL);
      BLI_assert(((BMElem *)data)->head.htype == BM_VERT);
      iter->begin = (BMIter__begin_cb)bmiter__edge_of_vert_begin;
      iter->step = (BMIter__step_cb)bmiter__edge_of_vert_step;
      iter->data.edge_of_vert.vdata = (BMVert *)data;
      break;
    case BM_FACES_OF_VERT:
      BLI_assert(data != NULL);
      BLI_assert(((BMElem *)data)->head.htype == BM_VERT);
      iter->begin = (BMIter__begin_cb)bmiter__face_of_vert_begin;
      iter->step = (BMIter__step_cb)bmiter__face_of_vert_step;
      iter->data.face_of_vert.vdata = (BMVert *)data;
      break;
    case BM_LOOPS_OF_VERT:
      BLI_assert(data != NULL);
      BLI_assert(((BMElem *)data)->head.htype == BM_VERT);
      iter->begin = (BMIter__begin_cb)bmiter__loop_of_vert_begin;
      iter->step = (BMIter__step_cb)bmiter__loop_of_vert_step;
      iter->data.loop_of_vert.vdata = (BMVert *)data;
      break;
    case BM_VERTS_OF_EDGE:
      BLI_assert(data != NULL);
      BLI_assert(((BMElem *)data)->head.htype == BM_EDGE);
      iter->begin = (BMIter__begin_cb)bmiter__vert_of_edge_begin;
      iter->step = (BMIter__step_cb)bmiter__vert_of_edge_step;
      iter->data.vert_of_edge.edata = (BMEdge *)data;
      break;
    case BM_FACES_OF_EDGE:
      BLI_assert(data != NULL);
      BLI_assert(((BMElem *)data)->head.htype == BM_EDGE);
      iter->begin = (BMIter__begin_cb)bmiter__face_of_edge_begin;
      iter->step = (BMIter__step_cb)bmiter__face_of_edge_step;
      iter->data.face_of_edge.edata = (BMEdge *)data;
      break;
    case BM_VERTS_OF_FACE:
      BLI_assert(data != NULL);
      BLI_assert(((BMElem *)data)->head.htype == BM_FACE);
      iter->begin = (BMIter__begin_cb)bmiter__vert_of_face_begin;
      iter->step = (BMIter__step_cb)bmiter__vert_of_face_step;
      iter->data.vert_of_face.pdata = (BMFace *)data;
      break;
    case BM_EDGES_OF_FACE:
      BLI_assert(data != NULL);
      BLI_assert(((BMElem *)data)->head.htype == BM_FACE);
      iter->begin = (BMIter__begin_cb)bmiter__edge_of_face_begin;
      iter->step = (BMIter__step_cb)bmiter__edge_of_face_step;
      iter->data.edge_of_face.pdata = (BMFace *)data;
      break;
    case BM_LOOPS_OF_FACE:
      BLI_assert(data != NULL);
      BLI_assert(((BMElem *)data)->head.htype == BM_FACE);
      iter->begin = (BMIter__begin_cb)bmiter__loop_of_face_begin;
      iter->step = (BMIter__step_cb)bmiter__loop_of_face_step;
      iter->data.loop_of_face.pdata = (BMFace *)data;
      break;
    case BM_LOOPS_OF_LOOP:
      BLI_assert(data != NULL);
      BLI_assert(((BMElem *)data)->head.htype == BM_LOOP);
      iter->begin = (BMIter__begin_cb)bmiter__loop_of_loop_begin;
      iter->step = (BMIter__step_cb)bmiter__loop_of_loop_step;
      iter->data.loop_of_loop.ldata = (BMLoop *)data;
      break;
    case BM_LOOPS_OF_EDGE:
      BLI_assert(data != NULL);
      BLI_assert(((BMElem *)data)->head.htype == BM_EDGE);
      iter->begin = (BMIter__begin_cb)bmiter__loop_of_edge_begin;
      iter->step = (BMIter__step_cb)bmiter__loop_of_edge_step;
      iter->data.loop_of_edge.edata = (BMEdge *)data;
      break;
    default:
      /* should never happen */
      BLI_assert(0);
      return false;
      break;
  }

  iter->begin(iter);

  return true;
}

/**
 * \brief Iterator New
 *
 * Takes a bmesh iterator structure and fills
 * it with the appropriate function pointers based
 * upon its type and then calls BMeshIter_step()
 * to return the first element of the iterator.
 */
ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1) BLI_INLINE
    void *BM_iter_new(BMIter *iter, BMesh *bm, const char itype, void *data)
{
  if (LIKELY(BM_iter_init(iter, bm, itype, data))) {
    return BM_iter_step(iter);
  }
  else {
    return NULL;
  }
}

/**
 * \brief Parallel (threaded) iterator,
 * only available for most basic itertypes (verts/edges/faces of mesh).
 *
 * Uses BLI_task_parallel_mempool to iterate over all items of underlying matching mempool.
 *
 * \note You have to include BLI_task.h before BMesh includes to be able to use this function!
 */

#ifdef __BLI_TASK_H__

ATTR_NONNULL(1)
BLI_INLINE void BM_iter_parallel(BMesh *bm,
                                 const char itype,
                                 TaskParallelMempoolFunc func,
                                 void *userdata,
                                 const bool use_threading)
{
  /* inlining optimizes out this switch when called with the defined type */
  switch ((BMIterType)itype) {
    case BM_VERTS_OF_MESH:
      BLI_task_parallel_mempool(bm->vpool, userdata, func, use_threading);
      break;
    case BM_EDGES_OF_MESH:
      BLI_task_parallel_mempool(bm->epool, userdata, func, use_threading);
      break;
    case BM_FACES_OF_MESH:
      BLI_task_parallel_mempool(bm->fpool, userdata, func, use_threading);
      break;
    default:
      /* should never happen */
      BLI_assert(0);
      break;
  }
}

#endif /* __BLI_TASK_H__ */

#endif /* __BMESH_ITERATORS_INLINE_H__ */
