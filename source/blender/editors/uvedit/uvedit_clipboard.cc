/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eduv
 *
 * Attempt to find a graph isomorphism between the topology of two different UV islands.
 *
 * \note On terminology, for the purposes of this file:
 * * An iso_graph is a "Graph" in Graph Theory.
 *  * An iso_graph has an unordered set of iso_verts.
 *  * An iso_graph has an unordered set of iso_edges.
 * * An iso_vert is a "Vertex" in Graph Theory
 *   * Each iso_vert has a label.
 * * An iso_edge is an "Edge" in Graph Theory
 *   * Each iso_edge connects two iso_verts.
 *   * An iso_edge is undirected.
 */

#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_editmesh.hh"
#include "BKE_layer.hh"
#include "BKE_mesh_mapping.hh" /* UvElementMap */
#include "BKE_report.hh"

#include "DEG_depsgraph.hh"

#include "ED_mesh.hh"
#include "ED_screen.hh"

#include "WM_api.hh"

#include "uvedit_clipboard_graph_iso.hh"
#include "uvedit_intern.hh" /* Own include. */

using blender::Vector;

void UV_clipboard_free();

class UV_ClipboardBuffer {
 public:
  ~UV_ClipboardBuffer();

  void append(UvElementMap *element_map, const int cd_loop_uv_offset);
  /**
   * \return True when found.
   */
  bool find_isomorphism(UvElementMap *dest_element_map,
                        int island_index,
                        int cd_loop_uv_offset,
                        blender::Vector<int> &r_label,
                        bool *r_search_abandoned);

  void write_uvs(UvElementMap *element_map,
                 int island_index,
                 const int cd_loop_uv_offset,
                 const blender::Vector<int> &label);

 private:
  blender::Vector<GraphISO *> graph;
  blender::Vector<int> offset;
  blender::Vector<std::pair<float, float>> uv;
};

static UV_ClipboardBuffer *uv_clipboard = nullptr;

UV_ClipboardBuffer::~UV_ClipboardBuffer()
{
  for (const int64_t index : graph.index_range()) {
    delete graph[index];
  }
  graph.clear();
  offset.clear();
  uv.clear();
}

/* Given a `BMLoop`, possibly belonging to an island in a `UvElementMap`,
 * return the `iso_index` corresponding to it's representation
 * in the `iso_graph`.
 *
 * If the `BMLoop` is not part of the `iso_graph`, return -1.
 */
static int iso_index_for_loop(const BMLoop *loop,
                              UvElementMap *element_map,
                              const int island_index)
{
  UvElement *element = BM_uv_element_get(element_map, loop);
  if (!element) {
    return -1; /* Either unselected, or a different island. */
  }
  const int index = BM_uv_element_get_unique_index(element_map, element);
  const int base_index = BM_uv_element_get_unique_index(
      element_map, element_map->storage + element_map->island_indices[island_index]);
  return index - base_index;
}

/* Add an `iso_edge` to an `iso_graph` between two BMLoops.
 */
static void add_iso_edge(
    GraphISO *graph, BMLoop *loop_v, BMLoop *loop_w, UvElementMap *element_map, int island_index)
{
  BLI_assert(loop_v->f == loop_w->f); /* Ensure on the same face. */
  const int index_v = iso_index_for_loop(loop_v, element_map, island_index);
  const int index_w = iso_index_for_loop(loop_w, element_map, island_index);
  BLI_assert(index_v != index_w);
  if (index_v == -1 || index_w == -1) {
    return; /* Unselected. */
  }

  BLI_assert(0 <= index_v && index_v < graph->n);
  BLI_assert(0 <= index_w && index_w < graph->n);

  graph->add_edge(index_v, index_w);
}

/* Build an `iso_graph` representation of an island of a `UvElementMap`.
 */
static GraphISO *build_iso_graph(UvElementMap *element_map,
                                 const int island_index,
                                 int /*cd_loop_uv_offset*/)
{
  GraphISO *g = new GraphISO(element_map->island_total_unique_uvs[island_index]);
  for (int i = 0; i < g->n; i++) {
    g->label[i] = i;
  }

  const int i0 = element_map->island_indices[island_index];
  const int i1 = i0 + element_map->island_total_uvs[island_index];

  /* Add iso_edges. */
  for (int i = i0; i < i1; i++) {
    const UvElement *element = element_map->storage + i;
    /* Look forward around the current face. */
    add_iso_edge(g, element->l, element->l->next, element_map, island_index);

    /* Look backward around the current face.
     * (Required for certain vertex selection cases.)
     */
    add_iso_edge(g, element->l->prev, element->l, element_map, island_index);
  }

  /* TODO: call g->sort_vertices_by_degree() */

  return g;
}

/* Convert each island inside an `element_map` into an `iso_graph`, and append them to the
 * clipboard buffer. */
void UV_ClipboardBuffer::append(UvElementMap *element_map, const int cd_loop_uv_offset)
{
  for (int island_index = 0; island_index < element_map->total_islands; island_index++) {
    offset.append(uv.size());
    graph.append(build_iso_graph(element_map, island_index, cd_loop_uv_offset));

    /* TODO: Consider iterating over `BM_uv_element_map_ensure_unique_index` instead. */
    for (int j = 0; j < element_map->island_total_uvs[island_index]; j++) {
      UvElement *element = element_map->storage + element_map->island_indices[island_index] + j;
      if (!element->separate) {
        continue;
      }
      float *luv = BM_ELEM_CD_GET_FLOAT_P(element->l, cd_loop_uv_offset);
      uv.append(std::make_pair(luv[0], luv[1]));
    }
  }
}

/* Write UVs back to an island. */
void UV_ClipboardBuffer::write_uvs(UvElementMap *element_map,
                                   int island_index,
                                   const int cd_loop_uv_offset,
                                   const blender::Vector<int> &label)
{
  BLI_assert(label.size() == element_map->island_total_unique_uvs[island_index]);

  /* TODO: Consider iterating over `BM_uv_element_map_ensure_unique_index` instead. */
  int unique_uv = 0;
  for (int j = 0; j < element_map->island_total_uvs[island_index]; j++) {
    int k = element_map->island_indices[island_index] + j;
    UvElement *element = element_map->storage + k;
    if (!element->separate) {
      continue;
    }
    BLI_assert(0 <= unique_uv);
    BLI_assert(unique_uv < label.size());
    const std::pair<float, float> &source_uv = uv_clipboard->uv[label[unique_uv]];
    while (element) {
      float *luv = BM_ELEM_CD_GET_FLOAT_P(element->l, cd_loop_uv_offset);
      luv[0] = source_uv.first;
      luv[1] = source_uv.second;
      element = element->next;
      if (!element || element->separate) {
        break;
      }
    }
    unique_uv++;
  }
  BLI_assert(unique_uv == label.size());
}

/**
 * Call the external isomorphism solver.
 * \return True when found.
 */
static bool find_isomorphism(UvElementMap *dest,
                             const int dest_island_index,
                             GraphISO *graph_source,
                             const int cd_loop_uv_offset,
                             blender::Vector<int> &r_label,
                             bool *r_search_abandoned)
{

  const int island_total_unique_uvs = dest->island_total_unique_uvs[dest_island_index];
  if (island_total_unique_uvs != graph_source->n) {
    return false; /* Isomorphisms can't differ in |iso_vert|. */
  }
  r_label.resize(island_total_unique_uvs);

  GraphISO *graph_dest = build_iso_graph(dest, dest_island_index, cd_loop_uv_offset);

  int (*solution)[2] = (int (*)[2])MEM_mallocN(graph_source->n * sizeof(*solution), __func__);
  int solution_length = 0;
  const bool found = ED_uvedit_clipboard_maximum_common_subgraph(
      graph_source, graph_dest, solution, &solution_length, r_search_abandoned);

  /* TODO: Implement "Best Effort" / "Nearest Match" paste functionality here. */

  if (found) {
    BLI_assert(solution_length == dest->island_total_unique_uvs[dest_island_index]);
    for (int i = 0; i < solution_length; i++) {
      int index_s = solution[i][0];
      int index_t = solution[i][1];
      BLI_assert(0 <= index_s && index_s < solution_length);
      BLI_assert(0 <= index_t && index_t < solution_length);
      r_label[index_t] = index_s;
    }
  }

  MEM_SAFE_FREE(solution);
  delete graph_dest;
  return found;
}

bool UV_ClipboardBuffer::find_isomorphism(UvElementMap *dest_element_map,
                                          const int dest_island_index,
                                          const int cd_loop_uv_offset,
                                          blender::Vector<int> &r_label,
                                          bool *r_search_abandoned)
{
  for (const int64_t source_island_index : graph.index_range()) {
    if (::find_isomorphism(dest_element_map,
                           dest_island_index,
                           graph[source_island_index],
                           cd_loop_uv_offset,
                           r_label,
                           r_search_abandoned))
    {
      const int island_total_unique_uvs =
          dest_element_map->island_total_unique_uvs[dest_island_index];
      const int island_offset = offset[source_island_index];
      BLI_assert(island_total_unique_uvs == r_label.size());
      for (int i = 0; i < island_total_unique_uvs; i++) {
        r_label[i] += island_offset; /* TODO: (minor optimization) Defer offset. */
      }

      /* TODO: There may be more than one match. How to choose between them? */
      return true;
    }
  }

  return false;
}

static wmOperatorStatus uv_copy_exec(bContext *C, wmOperator * /*op*/)
{
  UV_clipboard_free();
  uv_clipboard = new UV_ClipboardBuffer();

  ViewLayer *view_layer = CTX_data_view_layer(C);
  Scene *scene = CTX_data_scene(C);

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, nullptr);

  for (Object *ob : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(ob);

    const bool use_seams = false;
    UvElementMap *element_map = BM_uv_element_map_create(
        em->bm, scene, true, false, use_seams, true);
    if (element_map) {
      const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_PROP_FLOAT2);
      uv_clipboard->append(element_map, cd_loop_uv_offset);
    }
    BM_uv_element_map_free(element_map);
  }

  /* TODO: Serialize `UvClipboard` to system clipboard. */

  return OPERATOR_FINISHED;
}

static wmOperatorStatus uv_paste_exec(bContext *C, wmOperator *op)
{
  /* TODO: Restore `UvClipboard` from system clipboard. */
  if (!uv_clipboard) {
    return OPERATOR_FINISHED; /* Nothing to do. */
  }
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Scene *scene = CTX_data_scene(C);

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, view_layer, nullptr);

  bool changed_multi = false;
  int complicated_search = 0;
  int total_search = 0;
  for (Object *ob : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(ob);

    const bool use_seams = false;
    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_PROP_FLOAT2);

    UvElementMap *dest_element_map = BM_uv_element_map_create(
        em->bm, scene, true, false, use_seams, true);

    if (!dest_element_map) {
      continue;
    }

    bool changed = false;

    for (int i = 0; i < dest_element_map->total_islands; i++) {
      total_search++;
      blender::Vector<int> label;
      bool search_abandoned = false;
      const bool found = uv_clipboard->find_isomorphism(
          dest_element_map, i, cd_loop_uv_offset, label, &search_abandoned);
      if (!found) {
        if (search_abandoned) {
          complicated_search++;
        }
        continue; /* No source UVs can be found that is isomorphic to this island. */
      }

      uv_clipboard->write_uvs(dest_element_map, i, cd_loop_uv_offset, label);
      changed = true; /* UVs were moved. */
    }

    BM_uv_element_map_free(dest_element_map);

    if (changed) {
      changed_multi = true;

      DEG_id_tag_update(static_cast<ID *>(ob->data), 0);
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);
    }
  }

  if (complicated_search) {
    BKE_reportf(op->reports,
                RPT_WARNING,
                "Skipped %d of %d island(s), geometry was too complicated to detect a match",
                complicated_search,
                total_search);
  }

  return changed_multi ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

void UV_OT_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy UVs";
  ot->description = "Copy selected UV vertices";
  ot->idname = "UV_OT_copy";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* API callbacks. */
  ot->exec = uv_copy_exec;
  ot->poll = ED_operator_uvedit;
}

void UV_OT_paste(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Paste UVs";
  ot->description = "Paste selected UV vertices";
  ot->idname = "UV_OT_paste";
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* API callbacks. */
  ot->exec = uv_paste_exec;
  ot->poll = ED_operator_uvedit;
}

void UV_clipboard_free()
{
  delete uv_clipboard;
  uv_clipboard = nullptr;
}
