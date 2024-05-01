/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "DNA_meshdata_types.h"

#include "BLI_listbase.h"
#include "BLI_set.hh"
#include "BLI_string.h"

#include "BKE_curves.hh"
#include "BKE_deform.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_grease_pencil_vertex_groups.hh"

namespace blender::bke::greasepencil {

/* ------------------------------------------------------------------- */
/** \name Vertex groups in drawings
 * \{ */

void validate_drawing_vertex_groups(GreasePencil &grease_pencil)
{
  Set<std::string> valid_names;
  LISTBASE_FOREACH (const bDeformGroup *, defgroup, &grease_pencil.vertex_group_names) {
    valid_names.add_new(defgroup->name);
  }

  for (GreasePencilDrawingBase *base : grease_pencil.drawings()) {
    if (base->type != GP_DRAWING) {
      continue;
    }
    Drawing &drawing = reinterpret_cast<GreasePencilDrawing *>(base)->wrap();

    /* Remove unknown vertex groups. */
    CurvesGeometry &curves = drawing.strokes_for_write();
    int defgroup_index = 0;
    LISTBASE_FOREACH_MUTABLE (bDeformGroup *, defgroup, &curves.vertex_group_names) {
      if (!valid_names.contains(defgroup->name)) {
        remove_defgroup_index(curves.deform_verts_for_write(), defgroup_index);

        BLI_remlink(&curves.vertex_group_names, defgroup);
        MEM_SAFE_FREE(defgroup);
      }

      ++defgroup_index;
    }
  }
}

int ensure_vertex_group(const StringRef name, ListBase &vertex_group_names)
{
  int def_nr = BLI_findstringindex(&vertex_group_names, name.data(), offsetof(bDeformGroup, name));
  if (def_nr < 0) {
    bDeformGroup *defgroup = MEM_cnew<bDeformGroup>(__func__);
    STRNCPY(defgroup->name, name.data());
    BLI_addtail(&vertex_group_names, defgroup);
    def_nr = BLI_listbase_count(&vertex_group_names) - 1;
    BLI_assert(def_nr >= 0);
  }
  return def_nr;
}

void assign_to_vertex_group(GreasePencil &grease_pencil, const StringRef name, const float weight)
{
  for (GreasePencilDrawingBase *base : grease_pencil.drawings()) {
    if (base->type != GP_DRAWING) {
      continue;
    }
    Drawing &drawing = reinterpret_cast<GreasePencilDrawing *>(base)->wrap();
    bke::CurvesGeometry &curves = drawing.strokes_for_write();
    ListBase &vertex_group_names = curves.vertex_group_names;

    const bke::AttributeAccessor attributes = curves.attributes();
    const VArray<bool> selection = *attributes.lookup_or_default<bool>(
        ".selection", bke::AttrDomain::Point, true);

    /* Look for existing group, otherwise lazy-initialize if any vertex is selected. */
    int def_nr = BLI_findstringindex(
        &vertex_group_names, name.data(), offsetof(bDeformGroup, name));

    const MutableSpan<MDeformVert> dverts = curves.deform_verts_for_write();
    for (const int i : dverts.index_range()) {
      if (selection[i]) {
        /* Lazily add the vertex group if any vertex is selected. */
        if (def_nr < 0) {
          bDeformGroup *defgroup = MEM_cnew<bDeformGroup>(__func__);
          STRNCPY(defgroup->name, name.data());

          BLI_addtail(&vertex_group_names, defgroup);
          def_nr = BLI_listbase_count(&vertex_group_names) - 1;
          BLI_assert(def_nr >= 0);
        }

        MDeformWeight *dw = BKE_defvert_ensure_index(&dverts[i], def_nr);
        if (dw) {
          dw->weight = weight;
        }
      }
    }
  }
}

/** Remove selected vertices from the vertex group. */
bool remove_from_vertex_group(GreasePencil &grease_pencil,
                              const StringRef name,
                              const bool use_selection)
{
  bool changed = false;
  for (GreasePencilDrawingBase *base : grease_pencil.drawings()) {
    if (base->type != GP_DRAWING) {
      continue;
    }
    Drawing &drawing = reinterpret_cast<GreasePencilDrawing *>(base)->wrap();
    bke::CurvesGeometry &curves = drawing.strokes_for_write();
    ListBase &vertex_group_names = curves.vertex_group_names;

    const int def_nr = BLI_findstringindex(
        &vertex_group_names, name.data(), offsetof(bDeformGroup, name));
    if (def_nr < 0) {
      /* No vertices assigned to the group in this drawing. */
      continue;
    }

    const MutableSpan<MDeformVert> dverts = curves.deform_verts_for_write();
    const bke::AttributeAccessor attributes = curves.attributes();
    const VArray<bool> selection = *attributes.lookup_or_default<bool>(
        ".selection", bke::AttrDomain::Point, true);
    for (const int i : dverts.index_range()) {
      if (!use_selection || selection[i]) {
        MDeformVert *dv = &dverts[i];
        MDeformWeight *dw = BKE_defvert_find_index(dv, def_nr);
        BKE_defvert_remove_group(dv, dw);

        /* Adjust remaining vertex group indices. */
        for (const int j : IndexRange(dv->totweight)) {
          if (dv->dw[j].def_nr > def_nr) {
            dv->dw[j].def_nr--;
          }
        }

        changed = true;
      }
    }
  }
  return changed;
}

void clear_vertex_groups(GreasePencil &grease_pencil)
{
  for (GreasePencilDrawingBase *base : grease_pencil.drawings()) {
    if (base->type != GP_DRAWING) {
      continue;
    }
    Drawing &drawing = reinterpret_cast<GreasePencilDrawing *>(base)->wrap();
    bke::CurvesGeometry &curves = drawing.strokes_for_write();

    for (MDeformVert &dvert : curves.deform_verts_for_write()) {
      BKE_defvert_clear(&dvert);
    }
  }
}

void select_from_group(GreasePencil &grease_pencil, const StringRef name, const bool select)
{
  for (GreasePencilDrawingBase *base : grease_pencil.drawings()) {
    if (base->type != GP_DRAWING) {
      continue;
    }
    Drawing &drawing = reinterpret_cast<GreasePencilDrawing *>(base)->wrap();
    bke::CurvesGeometry &curves = drawing.strokes_for_write();
    ListBase &vertex_group_names = curves.vertex_group_names;

    const int def_nr = BLI_findstringindex(
        &vertex_group_names, name.data(), offsetof(bDeformGroup, name));
    if (def_nr < 0) {
      /* No vertices assigned to the group in this drawing. */
      continue;
    }

    const Span<MDeformVert> dverts = curves.deform_verts_for_write();
    if (!dverts.is_empty()) {
      bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
      SpanAttributeWriter<bool> selection = attributes.lookup_or_add_for_write_span<bool>(
          ".selection",
          bke::AttrDomain::Point,
          AttributeInitVArray(VArray<bool>::ForSingle(true, curves.point_num)));

      for (const int i : selection.span.index_range()) {
        if (BKE_defvert_find_index(&dverts[i], def_nr)) {
          selection.span[i] = select;
        }
      }

      selection.finish();
    }
  }
}

/** \} */

}  // namespace blender::bke::greasepencil
