/* SPDX-FileCopyrightText: 2010 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>

#include <fmt/format.h>

#include "MEM_guardedalloc.h"

#include "DNA_defaults.h"
#include "DNA_material_types.h" /* for ramp blend */
#include "DNA_object_types.h"
#include "DNA_sdna_type_ids.hh"
#include "DNA_texture_types.h"

#include "BLI_listbase.h"
#include "BLI_math_rotation.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_colorband.hh"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_freestyle.h"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_linestyle.h"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_texture.h"

#include "BLO_read_write.hh"

using blender::dna::sdna_struct_id_get;

static void linestyle_init_data(ID *id)
{
  FreestyleLineStyle *linestyle = (FreestyleLineStyle *)id;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(linestyle, id));

  MEMCPY_STRUCT_AFTER(linestyle, DNA_struct_default_get(FreestyleLineStyle), id);

  BKE_linestyle_geometry_modifier_add(linestyle, nullptr, LS_MODIFIER_SAMPLING);
}

static void linestyle_copy_data(Main *bmain,
                                std::optional<Library *> owner_library,
                                ID *id_dst,
                                const ID *id_src,
                                const int flag)
{
  FreestyleLineStyle *linestyle_dst = (FreestyleLineStyle *)id_dst;
  const FreestyleLineStyle *linestyle_src = (const FreestyleLineStyle *)id_src;

  /* Never handle user-count here for own sub-data. */
  const int flag_subdata = flag | LIB_ID_CREATE_NO_USER_REFCOUNT;
  /* Always need allocation of the embedded ID data. */
  const int flag_embedded_id_data = flag_subdata & ~LIB_ID_CREATE_NO_ALLOCATE;

  for (int a = 0; a < MAX_MTEX; a++) {
    if (linestyle_src->mtex[a]) {
      linestyle_dst->mtex[a] = MEM_callocN<MTex>(__func__);
      *linestyle_dst->mtex[a] = blender::dna::shallow_copy(*linestyle_src->mtex[a]);
    }
  }

  if (linestyle_src->nodetree) {
    BKE_id_copy_in_lib(bmain,
                       owner_library,
                       &linestyle_src->nodetree->id,
                       &linestyle_dst->id,
                       reinterpret_cast<ID **>(&linestyle_dst->nodetree),
                       flag_embedded_id_data);
  }

  BLI_listbase_clear(&linestyle_dst->color_modifiers);
  LISTBASE_FOREACH (LineStyleModifier *, linestyle_modifier, &linestyle_src->color_modifiers) {
    BKE_linestyle_color_modifier_copy(linestyle_dst, linestyle_modifier, flag_subdata);
  }

  BLI_listbase_clear(&linestyle_dst->alpha_modifiers);
  LISTBASE_FOREACH (LineStyleModifier *, linestyle_modifier, &linestyle_src->alpha_modifiers) {
    BKE_linestyle_alpha_modifier_copy(linestyle_dst, linestyle_modifier, flag_subdata);
  }

  BLI_listbase_clear(&linestyle_dst->thickness_modifiers);
  LISTBASE_FOREACH (LineStyleModifier *, linestyle_modifier, &linestyle_src->thickness_modifiers) {
    BKE_linestyle_thickness_modifier_copy(linestyle_dst, linestyle_modifier, flag_subdata);
  }

  BLI_listbase_clear(&linestyle_dst->geometry_modifiers);
  LISTBASE_FOREACH (LineStyleModifier *, linestyle_modifier, &linestyle_src->geometry_modifiers) {
    BKE_linestyle_geometry_modifier_copy(linestyle_dst, linestyle_modifier, flag_subdata);
  }
}

static void linestyle_free_data(ID *id)
{
  FreestyleLineStyle *linestyle = (FreestyleLineStyle *)id;
  LineStyleModifier *linestyle_modifier;

  for (int material_slot_index = 0; material_slot_index < MAX_MTEX; material_slot_index++) {
    MEM_SAFE_FREE(linestyle->mtex[material_slot_index]);
  }

  /* is no lib link block, but linestyle extension */
  if (linestyle->nodetree) {
    blender::bke::node_tree_free_embedded_tree(linestyle->nodetree);
    MEM_freeN(linestyle->nodetree);
    linestyle->nodetree = nullptr;
  }

  while ((linestyle_modifier = (LineStyleModifier *)linestyle->color_modifiers.first)) {
    BKE_linestyle_color_modifier_remove(linestyle, linestyle_modifier);
  }
  while ((linestyle_modifier = (LineStyleModifier *)linestyle->alpha_modifiers.first)) {
    BKE_linestyle_alpha_modifier_remove(linestyle, linestyle_modifier);
  }
  while ((linestyle_modifier = (LineStyleModifier *)linestyle->thickness_modifiers.first)) {
    BKE_linestyle_thickness_modifier_remove(linestyle, linestyle_modifier);
  }
  while ((linestyle_modifier = (LineStyleModifier *)linestyle->geometry_modifiers.first)) {
    BKE_linestyle_geometry_modifier_remove(linestyle, linestyle_modifier);
  }
}

static void linestyle_foreach_id(ID *id, LibraryForeachIDData *data)
{
  FreestyleLineStyle *linestyle = (FreestyleLineStyle *)id;

  for (int i = 0; i < MAX_MTEX; i++) {
    if (linestyle->mtex[i]) {
      BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(
          data, BKE_texture_mtex_foreach_id(data, linestyle->mtex[i]));
    }
  }
  if (linestyle->nodetree) {
    /* nodetree **are owned by IDs**, treat them as mere sub-data and not real ID! */
    BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(
        data, BKE_library_foreach_ID_embedded(data, (ID **)&linestyle->nodetree));
  }

  LISTBASE_FOREACH (LineStyleModifier *, lsm, &linestyle->color_modifiers) {
    if (lsm->type == LS_MODIFIER_DISTANCE_FROM_OBJECT) {
      LineStyleColorModifier_DistanceFromObject *p = (LineStyleColorModifier_DistanceFromObject *)
          lsm;
      BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, p->target, IDWALK_CB_NOP);
    }
  }
  LISTBASE_FOREACH (LineStyleModifier *, lsm, &linestyle->alpha_modifiers) {
    if (lsm->type == LS_MODIFIER_DISTANCE_FROM_OBJECT) {
      LineStyleAlphaModifier_DistanceFromObject *p = (LineStyleAlphaModifier_DistanceFromObject *)
          lsm;
      BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, p->target, IDWALK_CB_NOP);
    }
  }
  LISTBASE_FOREACH (LineStyleModifier *, lsm, &linestyle->thickness_modifiers) {
    if (lsm->type == LS_MODIFIER_DISTANCE_FROM_OBJECT) {
      LineStyleThicknessModifier_DistanceFromObject *p =
          (LineStyleThicknessModifier_DistanceFromObject *)lsm;
      BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, p->target, IDWALK_CB_NOP);
    }
  }
}

static void linestyle_foreach_working_space_color(ID *id,
                                                  const IDTypeForeachColorFunctionCallback &fn)
{
  FreestyleLineStyle *linestyle = (FreestyleLineStyle *)id;

  fn.single(&linestyle->r);
}

static void write_linestyle_color_modifiers(BlendWriter *writer, ListBase *modifiers)
{
  LISTBASE_FOREACH (LineStyleModifier *, m, modifiers) {
    int struct_nr;
    switch (m->type) {
      case LS_MODIFIER_ALONG_STROKE:
        struct_nr = sdna_struct_id_get<LineStyleColorModifier_AlongStroke>();
        break;
      case LS_MODIFIER_DISTANCE_FROM_CAMERA:
        struct_nr = sdna_struct_id_get<LineStyleColorModifier_DistanceFromCamera>();
        break;
      case LS_MODIFIER_DISTANCE_FROM_OBJECT:
        struct_nr = sdna_struct_id_get<LineStyleColorModifier_DistanceFromObject>();
        break;
      case LS_MODIFIER_MATERIAL:
        struct_nr = sdna_struct_id_get<LineStyleColorModifier_Material>();
        break;
      case LS_MODIFIER_TANGENT:
        struct_nr = sdna_struct_id_get<LineStyleColorModifier_Tangent>();
        break;
      case LS_MODIFIER_NOISE:
        struct_nr = sdna_struct_id_get<LineStyleColorModifier_Noise>();
        break;
      case LS_MODIFIER_CREASE_ANGLE:
        struct_nr = sdna_struct_id_get<LineStyleColorModifier_CreaseAngle>();
        break;
      case LS_MODIFIER_CURVATURE_3D:
        struct_nr = sdna_struct_id_get<LineStyleColorModifier_Curvature_3D>();
        break;
      default:
        struct_nr = sdna_struct_id_get<LineStyleModifier>(); /* this should not happen */
    }
    BLO_write_struct_by_id(writer, struct_nr, m);
  }
  LISTBASE_FOREACH (LineStyleModifier *, m, modifiers) {
    switch (m->type) {
      case LS_MODIFIER_ALONG_STROKE:
        BLO_write_struct(writer, ColorBand, ((LineStyleColorModifier_AlongStroke *)m)->color_ramp);
        break;
      case LS_MODIFIER_DISTANCE_FROM_CAMERA:
        BLO_write_struct(
            writer, ColorBand, ((LineStyleColorModifier_DistanceFromCamera *)m)->color_ramp);
        break;
      case LS_MODIFIER_DISTANCE_FROM_OBJECT:
        BLO_write_struct(
            writer, ColorBand, ((LineStyleColorModifier_DistanceFromObject *)m)->color_ramp);
        break;
      case LS_MODIFIER_MATERIAL:
        BLO_write_struct(writer, ColorBand, ((LineStyleColorModifier_Material *)m)->color_ramp);
        break;
      case LS_MODIFIER_TANGENT:
        BLO_write_struct(writer, ColorBand, ((LineStyleColorModifier_Tangent *)m)->color_ramp);
        break;
      case LS_MODIFIER_NOISE:
        BLO_write_struct(writer, ColorBand, ((LineStyleColorModifier_Noise *)m)->color_ramp);
        break;
      case LS_MODIFIER_CREASE_ANGLE:
        BLO_write_struct(writer, ColorBand, ((LineStyleColorModifier_CreaseAngle *)m)->color_ramp);
        break;
      case LS_MODIFIER_CURVATURE_3D:
        BLO_write_struct(
            writer, ColorBand, ((LineStyleColorModifier_Curvature_3D *)m)->color_ramp);
        break;
    }
  }
}

static void write_linestyle_alpha_modifiers(BlendWriter *writer, ListBase *modifiers)
{
  LISTBASE_FOREACH (LineStyleModifier *, m, modifiers) {
    int struct_nr;
    switch (m->type) {
      case LS_MODIFIER_ALONG_STROKE:
        struct_nr = sdna_struct_id_get<LineStyleAlphaModifier_AlongStroke>();
        break;
      case LS_MODIFIER_DISTANCE_FROM_CAMERA:
        struct_nr = sdna_struct_id_get<LineStyleAlphaModifier_DistanceFromCamera>();
        break;
      case LS_MODIFIER_DISTANCE_FROM_OBJECT:
        struct_nr = sdna_struct_id_get<LineStyleAlphaModifier_DistanceFromObject>();
        break;
      case LS_MODIFIER_MATERIAL:
        struct_nr = sdna_struct_id_get<LineStyleAlphaModifier_Material>();
        break;
      case LS_MODIFIER_TANGENT:
        struct_nr = sdna_struct_id_get<LineStyleAlphaModifier_Tangent>();
        break;
      case LS_MODIFIER_NOISE:
        struct_nr = sdna_struct_id_get<LineStyleAlphaModifier_Noise>();
        break;
      case LS_MODIFIER_CREASE_ANGLE:
        struct_nr = sdna_struct_id_get<LineStyleAlphaModifier_CreaseAngle>();
        break;
      case LS_MODIFIER_CURVATURE_3D:
        struct_nr = sdna_struct_id_get<LineStyleAlphaModifier_Curvature_3D>();
        break;
      default:
        struct_nr = sdna_struct_id_get<LineStyleModifier>(); /* this should not happen */
    }
    BLO_write_struct_by_id(writer, struct_nr, m);
  }
  LISTBASE_FOREACH (LineStyleModifier *, m, modifiers) {
    switch (m->type) {
      case LS_MODIFIER_ALONG_STROKE:
        BKE_curvemapping_blend_write(writer, ((LineStyleAlphaModifier_AlongStroke *)m)->curve);
        break;
      case LS_MODIFIER_DISTANCE_FROM_CAMERA:
        BKE_curvemapping_blend_write(writer,
                                     ((LineStyleAlphaModifier_DistanceFromCamera *)m)->curve);
        break;
      case LS_MODIFIER_DISTANCE_FROM_OBJECT:
        BKE_curvemapping_blend_write(writer,
                                     ((LineStyleAlphaModifier_DistanceFromObject *)m)->curve);
        break;
      case LS_MODIFIER_MATERIAL:
        BKE_curvemapping_blend_write(writer, ((LineStyleAlphaModifier_Material *)m)->curve);
        break;
      case LS_MODIFIER_TANGENT:
        BKE_curvemapping_blend_write(writer, ((LineStyleAlphaModifier_Tangent *)m)->curve);
        break;
      case LS_MODIFIER_NOISE:
        BKE_curvemapping_blend_write(writer, ((LineStyleAlphaModifier_Noise *)m)->curve);
        break;
      case LS_MODIFIER_CREASE_ANGLE:
        BKE_curvemapping_blend_write(writer, ((LineStyleAlphaModifier_CreaseAngle *)m)->curve);
        break;
      case LS_MODIFIER_CURVATURE_3D:
        BKE_curvemapping_blend_write(writer, ((LineStyleAlphaModifier_Curvature_3D *)m)->curve);
        break;
    }
  }
}

static void write_linestyle_thickness_modifiers(BlendWriter *writer, ListBase *modifiers)
{
  LISTBASE_FOREACH (LineStyleModifier *, m, modifiers) {
    int struct_nr;
    switch (m->type) {
      case LS_MODIFIER_ALONG_STROKE:
        struct_nr = sdna_struct_id_get<LineStyleThicknessModifier_AlongStroke>();
        break;
      case LS_MODIFIER_DISTANCE_FROM_CAMERA:
        struct_nr = sdna_struct_id_get<LineStyleThicknessModifier_DistanceFromCamera>();
        break;
      case LS_MODIFIER_DISTANCE_FROM_OBJECT:
        struct_nr = sdna_struct_id_get<LineStyleThicknessModifier_DistanceFromObject>();
        break;
      case LS_MODIFIER_MATERIAL:
        struct_nr = sdna_struct_id_get<LineStyleThicknessModifier_Material>();
        break;
      case LS_MODIFIER_CALLIGRAPHY:
        struct_nr = sdna_struct_id_get<LineStyleThicknessModifier_Calligraphy>();
        break;
      case LS_MODIFIER_TANGENT:
        struct_nr = sdna_struct_id_get<LineStyleThicknessModifier_Tangent>();
        break;
      case LS_MODIFIER_NOISE:
        struct_nr = sdna_struct_id_get<LineStyleThicknessModifier_Noise>();
        break;
      case LS_MODIFIER_CREASE_ANGLE:
        struct_nr = sdna_struct_id_get<LineStyleThicknessModifier_CreaseAngle>();
        break;
      case LS_MODIFIER_CURVATURE_3D:
        struct_nr = sdna_struct_id_get<LineStyleThicknessModifier_Curvature_3D>();
        break;
      default:
        struct_nr = sdna_struct_id_get<LineStyleModifier>(); /* this should not happen */
    }
    BLO_write_struct_by_id(writer, struct_nr, m);
  }
  LISTBASE_FOREACH (LineStyleModifier *, m, modifiers) {
    switch (m->type) {
      case LS_MODIFIER_ALONG_STROKE:
        BKE_curvemapping_blend_write(writer, ((LineStyleThicknessModifier_AlongStroke *)m)->curve);
        break;
      case LS_MODIFIER_DISTANCE_FROM_CAMERA:
        BKE_curvemapping_blend_write(writer,
                                     ((LineStyleThicknessModifier_DistanceFromCamera *)m)->curve);
        break;
      case LS_MODIFIER_DISTANCE_FROM_OBJECT:
        BKE_curvemapping_blend_write(writer,
                                     ((LineStyleThicknessModifier_DistanceFromObject *)m)->curve);
        break;
      case LS_MODIFIER_MATERIAL:
        BKE_curvemapping_blend_write(writer, ((LineStyleThicknessModifier_Material *)m)->curve);
        break;
      case LS_MODIFIER_TANGENT:
        BKE_curvemapping_blend_write(writer, ((LineStyleThicknessModifier_Tangent *)m)->curve);
        break;
      case LS_MODIFIER_CREASE_ANGLE:
        BKE_curvemapping_blend_write(writer, ((LineStyleThicknessModifier_CreaseAngle *)m)->curve);
        break;
      case LS_MODIFIER_CURVATURE_3D:
        BKE_curvemapping_blend_write(writer,
                                     ((LineStyleThicknessModifier_Curvature_3D *)m)->curve);
        break;
    }
  }
}

static void write_linestyle_geometry_modifiers(BlendWriter *writer, ListBase *modifiers)
{
  LISTBASE_FOREACH (LineStyleModifier *, m, modifiers) {
    int struct_nr;
    switch (m->type) {
      case LS_MODIFIER_SAMPLING:
        struct_nr = sdna_struct_id_get<LineStyleGeometryModifier_Sampling>();
        break;
      case LS_MODIFIER_BEZIER_CURVE:
        struct_nr = sdna_struct_id_get<LineStyleGeometryModifier_BezierCurve>();
        break;
      case LS_MODIFIER_SINUS_DISPLACEMENT:
        struct_nr = sdna_struct_id_get<LineStyleGeometryModifier_SinusDisplacement>();
        break;
      case LS_MODIFIER_SPATIAL_NOISE:
        struct_nr = sdna_struct_id_get<LineStyleGeometryModifier_SpatialNoise>();
        break;
      case LS_MODIFIER_PERLIN_NOISE_1D:
        struct_nr = sdna_struct_id_get<LineStyleGeometryModifier_PerlinNoise1D>();
        break;
      case LS_MODIFIER_PERLIN_NOISE_2D:
        struct_nr = sdna_struct_id_get<LineStyleGeometryModifier_PerlinNoise2D>();
        break;
      case LS_MODIFIER_BACKBONE_STRETCHER:
        struct_nr = sdna_struct_id_get<LineStyleGeometryModifier_BackboneStretcher>();
        break;
      case LS_MODIFIER_TIP_REMOVER:
        struct_nr = sdna_struct_id_get<LineStyleGeometryModifier_TipRemover>();
        break;
      case LS_MODIFIER_POLYGONIZATION:
        struct_nr = sdna_struct_id_get<LineStyleGeometryModifier_Polygonalization>();
        break;
      case LS_MODIFIER_GUIDING_LINES:
        struct_nr = sdna_struct_id_get<LineStyleGeometryModifier_GuidingLines>();
        break;
      case LS_MODIFIER_BLUEPRINT:
        struct_nr = sdna_struct_id_get<LineStyleGeometryModifier_Blueprint>();
        break;
      case LS_MODIFIER_2D_OFFSET:
        struct_nr = sdna_struct_id_get<LineStyleGeometryModifier_2DOffset>();
        break;
      case LS_MODIFIER_2D_TRANSFORM:
        struct_nr = sdna_struct_id_get<LineStyleGeometryModifier_2DTransform>();
        break;
      case LS_MODIFIER_SIMPLIFICATION:
        struct_nr = sdna_struct_id_get<LineStyleGeometryModifier_Simplification>();
        break;
      default:
        struct_nr = sdna_struct_id_get<LineStyleModifier>(); /* this should not happen */
    }
    BLO_write_struct_by_id(writer, struct_nr, m);
  }
}

static void linestyle_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  FreestyleLineStyle *linestyle = (FreestyleLineStyle *)id;

  BLO_write_id_struct(writer, FreestyleLineStyle, id_address, &linestyle->id);
  BKE_id_blend_write(writer, &linestyle->id);

  write_linestyle_color_modifiers(writer, &linestyle->color_modifiers);
  write_linestyle_alpha_modifiers(writer, &linestyle->alpha_modifiers);
  write_linestyle_thickness_modifiers(writer, &linestyle->thickness_modifiers);
  write_linestyle_geometry_modifiers(writer, &linestyle->geometry_modifiers);
  for (int a = 0; a < MAX_MTEX; a++) {
    if (linestyle->mtex[a]) {
      BLO_write_struct(writer, MTex, linestyle->mtex[a]);
    }
  }
  if (linestyle->nodetree) {
    BLO_Write_IDBuffer temp_embedded_id_buffer{linestyle->nodetree->id, writer};
    BLO_write_struct_at_address(
        writer, bNodeTree, linestyle->nodetree, temp_embedded_id_buffer.get());
    blender::bke::node_tree_blend_write(
        writer, reinterpret_cast<bNodeTree *>(temp_embedded_id_buffer.get()));
  }
}

static void direct_link_linestyle_color_modifier(BlendDataReader *reader,
                                                 LineStyleModifier *modifier)
{
  switch (modifier->type) {
    case LS_MODIFIER_ALONG_STROKE: {
      LineStyleColorModifier_AlongStroke *m = (LineStyleColorModifier_AlongStroke *)modifier;
      BLO_read_struct(reader, ColorBand, &m->color_ramp);
      break;
    }
    case LS_MODIFIER_DISTANCE_FROM_CAMERA: {
      LineStyleColorModifier_DistanceFromCamera *m = (LineStyleColorModifier_DistanceFromCamera *)
          modifier;
      BLO_read_struct(reader, ColorBand, &m->color_ramp);
      break;
    }
    case LS_MODIFIER_DISTANCE_FROM_OBJECT: {
      LineStyleColorModifier_DistanceFromObject *m = (LineStyleColorModifier_DistanceFromObject *)
          modifier;
      BLO_read_struct(reader, ColorBand, &m->color_ramp);
      break;
    }
    case LS_MODIFIER_MATERIAL: {
      LineStyleColorModifier_Material *m = (LineStyleColorModifier_Material *)modifier;
      BLO_read_struct(reader, ColorBand, &m->color_ramp);
      break;
    }
    case LS_MODIFIER_TANGENT: {
      LineStyleColorModifier_Tangent *m = (LineStyleColorModifier_Tangent *)modifier;
      BLO_read_struct(reader, ColorBand, &m->color_ramp);
      break;
    }
    case LS_MODIFIER_NOISE: {
      LineStyleColorModifier_Noise *m = (LineStyleColorModifier_Noise *)modifier;
      BLO_read_struct(reader, ColorBand, &m->color_ramp);
      break;
    }
    case LS_MODIFIER_CREASE_ANGLE: {
      LineStyleColorModifier_CreaseAngle *m = (LineStyleColorModifier_CreaseAngle *)modifier;
      BLO_read_struct(reader, ColorBand, &m->color_ramp);
      break;
    }
    case LS_MODIFIER_CURVATURE_3D: {
      LineStyleColorModifier_Curvature_3D *m = (LineStyleColorModifier_Curvature_3D *)modifier;
      BLO_read_struct(reader, ColorBand, &m->color_ramp);
      break;
    }
  }
}

static void direct_link_linestyle_alpha_modifier(BlendDataReader *reader,
                                                 LineStyleModifier *modifier)
{
  switch (modifier->type) {
    case LS_MODIFIER_ALONG_STROKE: {
      LineStyleAlphaModifier_AlongStroke *m = (LineStyleAlphaModifier_AlongStroke *)modifier;
      BLO_read_struct(reader, CurveMapping, &m->curve);
      BKE_curvemapping_blend_read(reader, m->curve);
      break;
    }
    case LS_MODIFIER_DISTANCE_FROM_CAMERA: {
      LineStyleAlphaModifier_DistanceFromCamera *m = (LineStyleAlphaModifier_DistanceFromCamera *)
          modifier;
      BLO_read_struct(reader, CurveMapping, &m->curve);
      BKE_curvemapping_blend_read(reader, m->curve);
      break;
    }
    case LS_MODIFIER_DISTANCE_FROM_OBJECT: {
      LineStyleAlphaModifier_DistanceFromObject *m = (LineStyleAlphaModifier_DistanceFromObject *)
          modifier;
      BLO_read_struct(reader, CurveMapping, &m->curve);
      BKE_curvemapping_blend_read(reader, m->curve);
      break;
    }
    case LS_MODIFIER_MATERIAL: {
      LineStyleAlphaModifier_Material *m = (LineStyleAlphaModifier_Material *)modifier;
      BLO_read_struct(reader, CurveMapping, &m->curve);
      BKE_curvemapping_blend_read(reader, m->curve);
      break;
    }
    case LS_MODIFIER_TANGENT: {
      LineStyleAlphaModifier_Tangent *m = (LineStyleAlphaModifier_Tangent *)modifier;
      BLO_read_struct(reader, CurveMapping, &m->curve);
      BKE_curvemapping_blend_read(reader, m->curve);
      break;
    }
    case LS_MODIFIER_NOISE: {
      LineStyleAlphaModifier_Noise *m = (LineStyleAlphaModifier_Noise *)modifier;
      BLO_read_struct(reader, CurveMapping, &m->curve);
      BKE_curvemapping_blend_read(reader, m->curve);
      break;
    }
    case LS_MODIFIER_CREASE_ANGLE: {
      LineStyleAlphaModifier_CreaseAngle *m = (LineStyleAlphaModifier_CreaseAngle *)modifier;
      BLO_read_struct(reader, CurveMapping, &m->curve);
      BKE_curvemapping_blend_read(reader, m->curve);
      break;
    }
    case LS_MODIFIER_CURVATURE_3D: {
      LineStyleAlphaModifier_Curvature_3D *m = (LineStyleAlphaModifier_Curvature_3D *)modifier;
      BLO_read_struct(reader, CurveMapping, &m->curve);
      BKE_curvemapping_blend_read(reader, m->curve);
      break;
    }
  }
}

static void direct_link_linestyle_thickness_modifier(BlendDataReader *reader,
                                                     LineStyleModifier *modifier)
{
  switch (modifier->type) {
    case LS_MODIFIER_ALONG_STROKE: {
      LineStyleThicknessModifier_AlongStroke *m = (LineStyleThicknessModifier_AlongStroke *)
          modifier;
      BLO_read_struct(reader, CurveMapping, &m->curve);
      BKE_curvemapping_blend_read(reader, m->curve);
      break;
    }
    case LS_MODIFIER_DISTANCE_FROM_CAMERA: {
      LineStyleThicknessModifier_DistanceFromCamera *m =
          (LineStyleThicknessModifier_DistanceFromCamera *)modifier;
      BLO_read_struct(reader, CurveMapping, &m->curve);
      BKE_curvemapping_blend_read(reader, m->curve);
      break;
    }
    case LS_MODIFIER_DISTANCE_FROM_OBJECT: {
      LineStyleThicknessModifier_DistanceFromObject *m =
          (LineStyleThicknessModifier_DistanceFromObject *)modifier;
      BLO_read_struct(reader, CurveMapping, &m->curve);
      BKE_curvemapping_blend_read(reader, m->curve);
      break;
    }
    case LS_MODIFIER_MATERIAL: {
      LineStyleThicknessModifier_Material *m = (LineStyleThicknessModifier_Material *)modifier;
      BLO_read_struct(reader, CurveMapping, &m->curve);
      BKE_curvemapping_blend_read(reader, m->curve);
      break;
    }
    case LS_MODIFIER_TANGENT: {
      LineStyleThicknessModifier_Tangent *m = (LineStyleThicknessModifier_Tangent *)modifier;
      BLO_read_struct(reader, CurveMapping, &m->curve);
      BKE_curvemapping_blend_read(reader, m->curve);
      break;
    }
    case LS_MODIFIER_CREASE_ANGLE: {
      LineStyleThicknessModifier_CreaseAngle *m = (LineStyleThicknessModifier_CreaseAngle *)
          modifier;
      BLO_read_struct(reader, CurveMapping, &m->curve);
      BKE_curvemapping_blend_read(reader, m->curve);
      break;
    }
    case LS_MODIFIER_CURVATURE_3D: {
      LineStyleThicknessModifier_Curvature_3D *m = (LineStyleThicknessModifier_Curvature_3D *)
          modifier;
      BLO_read_struct(reader, CurveMapping, &m->curve);
      BKE_curvemapping_blend_read(reader, m->curve);
      break;
    }
  }
}

static void direct_link_linestyle_geometry_modifier(BlendDataReader * /*reader*/,
                                                    LineStyleModifier * /*modifier*/)
{
}

static void linestyle_blend_read_data(BlendDataReader *reader, ID *id)
{
  FreestyleLineStyle *linestyle = (FreestyleLineStyle *)id;

  BLO_read_struct_list(reader, LineStyleModifier, &linestyle->color_modifiers);
  LISTBASE_FOREACH (LineStyleModifier *, modifier, &linestyle->color_modifiers) {
    direct_link_linestyle_color_modifier(reader, modifier);
  }
  BLO_read_struct_list(reader, LineStyleModifier, &linestyle->alpha_modifiers);
  LISTBASE_FOREACH (LineStyleModifier *, modifier, &linestyle->alpha_modifiers) {
    direct_link_linestyle_alpha_modifier(reader, modifier);
  }
  BLO_read_struct_list(reader, LineStyleModifier, &linestyle->thickness_modifiers);
  LISTBASE_FOREACH (LineStyleModifier *, modifier, &linestyle->thickness_modifiers) {
    direct_link_linestyle_thickness_modifier(reader, modifier);
  }
  BLO_read_struct_list(reader, LineStyleModifier, &linestyle->geometry_modifiers);
  LISTBASE_FOREACH (LineStyleModifier *, modifier, &linestyle->geometry_modifiers) {
    direct_link_linestyle_geometry_modifier(reader, modifier);
  }
  for (int a = 0; a < MAX_MTEX; a++) {
    BLO_read_struct(reader, MTex, &linestyle->mtex[a]);
  }
}

IDTypeInfo IDType_ID_LS = {
    /*id_code*/ FreestyleLineStyle::id_type,
    /*id_filter*/ FILTER_ID_LS,
    /*dependencies_id_types*/ FILTER_ID_TE | FILTER_ID_OB,
    /*main_listbase_index*/ INDEX_ID_LS,
    /*struct_size*/ sizeof(FreestyleLineStyle),
    /*name*/ "FreestyleLineStyle",
    /*name_plural*/ N_("linestyles"),
    /*translation_context*/ BLT_I18NCONTEXT_ID_FREESTYLELINESTYLE,
    /*flags*/ IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    /*asset_type_info*/ nullptr,

    /*init_data*/ linestyle_init_data,
    /*copy_data*/ linestyle_copy_data,
    /*free_data*/ linestyle_free_data,
    /*make_local*/ nullptr,
    /*foreach_id*/ linestyle_foreach_id,
    /*foreach_cache*/ nullptr,
    /*foreach_path*/ nullptr,
    /*foreach_working_space_color*/ linestyle_foreach_working_space_color,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ linestyle_blend_write,
    /*blend_read_data*/ linestyle_blend_read_data,
    /*blend_read_after_liblink*/ nullptr,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

static const char *modifier_name[LS_MODIFIER_NUM] = {
    nullptr,         "Along Stroke",    "Distance from Camera", "Distance from Object",
    "Material",      "Sampling",        "Bézier Curve",         "Sinus Displacement",
    "Spatial Noise", "Perlin Noise 1D", "Perlin Noise 2D",      "Backbone Stretcher",
    "Tip Remover",   "Calligraphy",     "Polygonalization",     "Guiding Lines",
    "Blueprint",     "2D Offset",       "2D Transform",         "Tangent",
    "Noise",         "Crease Angle",    "Simplification",       "Curvature 3D",
};

void BKE_linestyle_init(FreestyleLineStyle *linestyle)
{
  linestyle_init_data(&linestyle->id);
}

FreestyleLineStyle *BKE_linestyle_new(Main *bmain, const char *name)
{
  FreestyleLineStyle *linestyle;

  linestyle = (FreestyleLineStyle *)BKE_libblock_alloc(bmain, ID_LS, name, 0);

  BKE_linestyle_init(linestyle);

  return linestyle;
}

FreestyleLineStyle *BKE_linestyle_active_from_view_layer(ViewLayer *view_layer)
{
  FreestyleConfig *config = &view_layer->freestyle_config;
  FreestyleLineSet *lineset = BKE_freestyle_lineset_get_active(config);
  return (lineset) ? lineset->linestyle : nullptr;
}

static LineStyleModifier *new_modifier(const char *name, int type, size_t size)
{
  LineStyleModifier *m;

  if (!name) {
    name = modifier_name[type];
  }
  m = (LineStyleModifier *)MEM_callocN(size, "line style modifier");
  m->type = type;
  STRNCPY_UTF8(m->name, DATA_(name));
  m->influence = 1.0f;
  m->flags = LS_MODIFIER_ENABLED | LS_MODIFIER_EXPANDED;

  return m;
}

static void add_to_modifier_list(ListBase *lb, LineStyleModifier *m)
{
  BLI_addtail(lb, (void *)m);
  BLI_uniquename(
      lb, m, modifier_name[m->type], '.', offsetof(LineStyleModifier, name), sizeof(m->name));
}

static LineStyleModifier *alloc_color_modifier(const char *name, int type)
{
  size_t size;

  switch (type) {
    case LS_MODIFIER_ALONG_STROKE:
      size = sizeof(LineStyleColorModifier_AlongStroke);
      break;
    case LS_MODIFIER_DISTANCE_FROM_CAMERA:
      size = sizeof(LineStyleColorModifier_DistanceFromCamera);
      break;
    case LS_MODIFIER_DISTANCE_FROM_OBJECT:
      size = sizeof(LineStyleColorModifier_DistanceFromObject);
      break;
    case LS_MODIFIER_MATERIAL:
      size = sizeof(LineStyleColorModifier_Material);
      break;
    case LS_MODIFIER_TANGENT:
      size = sizeof(LineStyleColorModifier_Tangent);
      break;
    case LS_MODIFIER_NOISE:
      size = sizeof(LineStyleColorModifier_Noise);
      break;
    case LS_MODIFIER_CREASE_ANGLE:
      size = sizeof(LineStyleColorModifier_CreaseAngle);
      break;
    case LS_MODIFIER_CURVATURE_3D:
      size = sizeof(LineStyleColorModifier_Curvature_3D);
      break;
    default:
      return nullptr; /* unknown modifier type */
  }

  return new_modifier(name, type, size);
}

LineStyleModifier *BKE_linestyle_color_modifier_add(FreestyleLineStyle *linestyle,
                                                    const char *name,
                                                    int type)
{
  LineStyleModifier *m;

  m = alloc_color_modifier(name, type);
  if (UNLIKELY(m == nullptr)) {
    return nullptr;
  }
  m->blend = MA_RAMP_BLEND;

  switch (type) {
    case LS_MODIFIER_ALONG_STROKE:
      ((LineStyleColorModifier_AlongStroke *)m)->color_ramp = BKE_colorband_add(true);
      break;
    case LS_MODIFIER_DISTANCE_FROM_CAMERA:
      ((LineStyleColorModifier_DistanceFromCamera *)m)->color_ramp = BKE_colorband_add(true);
      ((LineStyleColorModifier_DistanceFromCamera *)m)->range_min = 0.0f;
      ((LineStyleColorModifier_DistanceFromCamera *)m)->range_max = 10000.0f;
      break;
    case LS_MODIFIER_DISTANCE_FROM_OBJECT:
      ((LineStyleColorModifier_DistanceFromObject *)m)->target = nullptr;
      ((LineStyleColorModifier_DistanceFromObject *)m)->color_ramp = BKE_colorband_add(true);
      ((LineStyleColorModifier_DistanceFromObject *)m)->range_min = 0.0f;
      ((LineStyleColorModifier_DistanceFromObject *)m)->range_max = 10000.0f;
      break;
    case LS_MODIFIER_MATERIAL:
      ((LineStyleColorModifier_Material *)m)->color_ramp = BKE_colorband_add(true);
      ((LineStyleColorModifier_Material *)m)->mat_attr = LS_MODIFIER_MATERIAL_LINE;
      break;
    case LS_MODIFIER_TANGENT:
      ((LineStyleColorModifier_Tangent *)m)->color_ramp = BKE_colorband_add(true);
      break;
    case LS_MODIFIER_NOISE:
      ((LineStyleColorModifier_Noise *)m)->color_ramp = BKE_colorband_add(true);
      ((LineStyleColorModifier_Noise *)m)->amplitude = 10.0f;
      ((LineStyleColorModifier_Noise *)m)->period = 10.0f;
      ((LineStyleColorModifier_Noise *)m)->seed = 512;
      break;
    case LS_MODIFIER_CREASE_ANGLE:
      ((LineStyleColorModifier_CreaseAngle *)m)->color_ramp = BKE_colorband_add(true);
      ((LineStyleColorModifier_CreaseAngle *)m)->min_angle = 0.0f;
      ((LineStyleColorModifier_CreaseAngle *)m)->max_angle = DEG2RADF(180.0f);
      break;
    case LS_MODIFIER_CURVATURE_3D:
      ((LineStyleColorModifier_Curvature_3D *)m)->color_ramp = BKE_colorband_add(true);
      ((LineStyleColorModifier_Curvature_3D *)m)->min_curvature = 0.0f;
      ((LineStyleColorModifier_Curvature_3D *)m)->max_curvature = 0.5f;
      break;
    default:
      return nullptr; /* unknown modifier type */
  }
  add_to_modifier_list(&linestyle->color_modifiers, m);

  return m;
}

LineStyleModifier *BKE_linestyle_color_modifier_copy(FreestyleLineStyle *linestyle,
                                                     const LineStyleModifier *m,
                                                     const int flag)
{
  LineStyleModifier *new_m;

  new_m = alloc_color_modifier(m->name, m->type);
  if (UNLIKELY(new_m == nullptr)) {
    return nullptr;
  }
  new_m->influence = m->influence;
  new_m->flags = m->flags;
  new_m->blend = m->blend;

  switch (m->type) {
    case LS_MODIFIER_ALONG_STROKE: {
      LineStyleColorModifier_AlongStroke *p = (LineStyleColorModifier_AlongStroke *)m;
      LineStyleColorModifier_AlongStroke *q = (LineStyleColorModifier_AlongStroke *)new_m;
      q->color_ramp = static_cast<ColorBand *>(MEM_dupallocN(p->color_ramp));
      break;
    }
    case LS_MODIFIER_DISTANCE_FROM_CAMERA: {
      LineStyleColorModifier_DistanceFromCamera *p = (LineStyleColorModifier_DistanceFromCamera *)
          m;
      LineStyleColorModifier_DistanceFromCamera *q = (LineStyleColorModifier_DistanceFromCamera *)
          new_m;
      q->color_ramp = static_cast<ColorBand *>(MEM_dupallocN(p->color_ramp));
      q->range_min = p->range_min;
      q->range_max = p->range_max;
      break;
    }
    case LS_MODIFIER_DISTANCE_FROM_OBJECT: {
      LineStyleColorModifier_DistanceFromObject *p = (LineStyleColorModifier_DistanceFromObject *)
          m;
      LineStyleColorModifier_DistanceFromObject *q = (LineStyleColorModifier_DistanceFromObject *)
          new_m;
      q->target = p->target;
      if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
        id_us_plus((ID *)q->target);
      }
      q->color_ramp = static_cast<ColorBand *>(MEM_dupallocN(p->color_ramp));
      q->range_min = p->range_min;
      q->range_max = p->range_max;
      break;
    }
    case LS_MODIFIER_MATERIAL: {
      LineStyleColorModifier_Material *p = (LineStyleColorModifier_Material *)m;
      LineStyleColorModifier_Material *q = (LineStyleColorModifier_Material *)new_m;
      q->color_ramp = static_cast<ColorBand *>(MEM_dupallocN(p->color_ramp));
      q->flags = p->flags;
      q->mat_attr = p->mat_attr;
      break;
    }
    case LS_MODIFIER_TANGENT: {
      LineStyleColorModifier_Tangent *p = (LineStyleColorModifier_Tangent *)m;
      LineStyleColorModifier_Tangent *q = (LineStyleColorModifier_Tangent *)new_m;
      q->color_ramp = static_cast<ColorBand *>(MEM_dupallocN(p->color_ramp));
      break;
    }
    case LS_MODIFIER_NOISE: {
      LineStyleColorModifier_Noise *p = (LineStyleColorModifier_Noise *)m;
      LineStyleColorModifier_Noise *q = (LineStyleColorModifier_Noise *)new_m;
      q->color_ramp = static_cast<ColorBand *>(MEM_dupallocN(p->color_ramp));
      q->amplitude = p->amplitude;
      q->period = p->period;
      q->seed = p->seed;
      break;
    }
    case LS_MODIFIER_CREASE_ANGLE: {
      LineStyleColorModifier_CreaseAngle *p = (LineStyleColorModifier_CreaseAngle *)m;
      LineStyleColorModifier_CreaseAngle *q = (LineStyleColorModifier_CreaseAngle *)new_m;
      q->color_ramp = static_cast<ColorBand *>(MEM_dupallocN(p->color_ramp));
      q->min_angle = p->min_angle;
      q->max_angle = p->max_angle;
      break;
    }
    case LS_MODIFIER_CURVATURE_3D: {
      LineStyleColorModifier_Curvature_3D *p = (LineStyleColorModifier_Curvature_3D *)m;
      LineStyleColorModifier_Curvature_3D *q = (LineStyleColorModifier_Curvature_3D *)new_m;
      q->color_ramp = static_cast<ColorBand *>(MEM_dupallocN(p->color_ramp));
      q->min_curvature = p->min_curvature;
      q->max_curvature = p->max_curvature;
      break;
    }
    default:
      return nullptr; /* unknown modifier type */
  }
  add_to_modifier_list(&linestyle->color_modifiers, new_m);

  return new_m;
}

int BKE_linestyle_color_modifier_remove(FreestyleLineStyle *linestyle, LineStyleModifier *m)
{
  if (BLI_findindex(&linestyle->color_modifiers, m) == -1) {
    return -1;
  }
  switch (m->type) {
    case LS_MODIFIER_ALONG_STROKE:
      MEM_freeN(((LineStyleColorModifier_AlongStroke *)m)->color_ramp);
      break;
    case LS_MODIFIER_DISTANCE_FROM_CAMERA:
      MEM_freeN(((LineStyleColorModifier_DistanceFromCamera *)m)->color_ramp);
      break;
    case LS_MODIFIER_DISTANCE_FROM_OBJECT:
      MEM_freeN(((LineStyleColorModifier_DistanceFromObject *)m)->color_ramp);
      break;
    case LS_MODIFIER_MATERIAL:
      MEM_freeN(((LineStyleColorModifier_Material *)m)->color_ramp);
      break;
    case LS_MODIFIER_TANGENT:
      MEM_freeN(((LineStyleColorModifier_Tangent *)m)->color_ramp);
      break;
    case LS_MODIFIER_NOISE:
      MEM_freeN(((LineStyleColorModifier_Noise *)m)->color_ramp);
      break;
    case LS_MODIFIER_CREASE_ANGLE:
      MEM_freeN(((LineStyleColorModifier_CreaseAngle *)m)->color_ramp);
      break;
    case LS_MODIFIER_CURVATURE_3D:
      MEM_freeN(((LineStyleColorModifier_Curvature_3D *)m)->color_ramp);
      break;
  }
  BLI_freelinkN(&linestyle->color_modifiers, m);
  return 0;
}

static LineStyleModifier *alloc_alpha_modifier(const char *name, int type)
{
  size_t size;

  switch (type) {
    case LS_MODIFIER_ALONG_STROKE:
      size = sizeof(LineStyleAlphaModifier_AlongStroke);
      break;
    case LS_MODIFIER_DISTANCE_FROM_CAMERA:
      size = sizeof(LineStyleAlphaModifier_DistanceFromCamera);
      break;
    case LS_MODIFIER_DISTANCE_FROM_OBJECT:
      size = sizeof(LineStyleAlphaModifier_DistanceFromObject);
      break;
    case LS_MODIFIER_MATERIAL:
      size = sizeof(LineStyleAlphaModifier_Material);
      break;
    case LS_MODIFIER_TANGENT:
      size = sizeof(LineStyleAlphaModifier_Tangent);
      break;
    case LS_MODIFIER_NOISE:
      size = sizeof(LineStyleAlphaModifier_Noise);
      break;
    case LS_MODIFIER_CREASE_ANGLE:
      size = sizeof(LineStyleAlphaModifier_CreaseAngle);
      break;
    case LS_MODIFIER_CURVATURE_3D:
      size = sizeof(LineStyleAlphaModifier_Curvature_3D);
      break;
    default:
      return nullptr; /* unknown modifier type */
  }
  return new_modifier(name, type, size);
}

LineStyleModifier *BKE_linestyle_alpha_modifier_add(FreestyleLineStyle *linestyle,
                                                    const char *name,
                                                    int type)
{
  LineStyleModifier *m;

  m = alloc_alpha_modifier(name, type);
  m->blend = LS_VALUE_BLEND;

  switch (type) {
    case LS_MODIFIER_ALONG_STROKE: {
      LineStyleAlphaModifier_AlongStroke *p = (LineStyleAlphaModifier_AlongStroke *)m;
      p->curve = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
      break;
    }
    case LS_MODIFIER_DISTANCE_FROM_CAMERA: {
      LineStyleAlphaModifier_DistanceFromCamera *p = (LineStyleAlphaModifier_DistanceFromCamera *)
          m;
      p->curve = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
      p->range_min = 0.0f;
      p->range_max = 10000.0f;
      break;
    }
    case LS_MODIFIER_DISTANCE_FROM_OBJECT: {
      LineStyleAlphaModifier_DistanceFromObject *p = (LineStyleAlphaModifier_DistanceFromObject *)
          m;
      p->target = nullptr;
      p->curve = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
      p->range_min = 0.0f;
      p->range_max = 10000.0f;
      break;
    }
    case LS_MODIFIER_MATERIAL: {
      LineStyleAlphaModifier_Material *p = (LineStyleAlphaModifier_Material *)m;
      p->curve = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
      p->mat_attr = LS_MODIFIER_MATERIAL_LINE_A;
      break;
    }
    case LS_MODIFIER_TANGENT: {
      LineStyleAlphaModifier_Tangent *p = (LineStyleAlphaModifier_Tangent *)m;
      p->curve = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
      break;
    }
    case LS_MODIFIER_NOISE: {
      LineStyleAlphaModifier_Noise *p = (LineStyleAlphaModifier_Noise *)m;
      p->curve = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
      ((LineStyleAlphaModifier_Noise *)m)->amplitude = 10.0f;
      ((LineStyleAlphaModifier_Noise *)m)->period = 10.0f;
      ((LineStyleAlphaModifier_Noise *)m)->seed = 512;
      break;
    }
    case LS_MODIFIER_CREASE_ANGLE: {
      LineStyleAlphaModifier_CreaseAngle *p = (LineStyleAlphaModifier_CreaseAngle *)m;
      p->curve = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
      ((LineStyleAlphaModifier_CreaseAngle *)m)->min_angle = 0.0f;
      ((LineStyleAlphaModifier_CreaseAngle *)m)->max_angle = DEG2RADF(180.0f);
      break;
    }
    case LS_MODIFIER_CURVATURE_3D: {
      LineStyleAlphaModifier_Curvature_3D *p = (LineStyleAlphaModifier_Curvature_3D *)m;
      p->curve = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
      ((LineStyleAlphaModifier_Curvature_3D *)m)->min_curvature = 0.0f;
      ((LineStyleAlphaModifier_Curvature_3D *)m)->max_curvature = 0.5f;
      break;
    }
    default:
      return nullptr; /* unknown modifier type */
  }
  add_to_modifier_list(&linestyle->alpha_modifiers, m);

  return m;
}

LineStyleModifier *BKE_linestyle_alpha_modifier_copy(FreestyleLineStyle *linestyle,
                                                     const LineStyleModifier *m,
                                                     const int /*flag*/)
{
  LineStyleModifier *new_m;

  new_m = alloc_alpha_modifier(m->name, m->type);
  new_m->influence = m->influence;
  new_m->flags = m->flags;
  new_m->blend = m->blend;

  switch (m->type) {
    case LS_MODIFIER_ALONG_STROKE: {
      LineStyleAlphaModifier_AlongStroke *p = (LineStyleAlphaModifier_AlongStroke *)m;
      LineStyleAlphaModifier_AlongStroke *q = (LineStyleAlphaModifier_AlongStroke *)new_m;
      q->curve = BKE_curvemapping_copy(p->curve);
      q->flags = p->flags;
      break;
    }
    case LS_MODIFIER_DISTANCE_FROM_CAMERA: {
      LineStyleAlphaModifier_DistanceFromCamera *p = (LineStyleAlphaModifier_DistanceFromCamera *)
          m;
      LineStyleAlphaModifier_DistanceFromCamera *q = (LineStyleAlphaModifier_DistanceFromCamera *)
          new_m;
      q->curve = BKE_curvemapping_copy(p->curve);
      q->flags = p->flags;
      q->range_min = p->range_min;
      q->range_max = p->range_max;
      break;
    }
    case LS_MODIFIER_DISTANCE_FROM_OBJECT: {
      LineStyleAlphaModifier_DistanceFromObject *p = (LineStyleAlphaModifier_DistanceFromObject *)
          m;
      LineStyleAlphaModifier_DistanceFromObject *q = (LineStyleAlphaModifier_DistanceFromObject *)
          new_m;
      if (p->target) {
        id_us_plus(&p->target->id);
      }
      q->target = p->target;
      q->curve = BKE_curvemapping_copy(p->curve);
      q->flags = p->flags;
      q->range_min = p->range_min;
      q->range_max = p->range_max;
      break;
    }
    case LS_MODIFIER_MATERIAL: {
      LineStyleAlphaModifier_Material *p = (LineStyleAlphaModifier_Material *)m;
      LineStyleAlphaModifier_Material *q = (LineStyleAlphaModifier_Material *)new_m;
      q->curve = BKE_curvemapping_copy(p->curve);
      q->flags = p->flags;
      q->mat_attr = p->mat_attr;
      break;
    }
    case LS_MODIFIER_TANGENT: {
      LineStyleAlphaModifier_Tangent *p = (LineStyleAlphaModifier_Tangent *)m;
      LineStyleAlphaModifier_Tangent *q = (LineStyleAlphaModifier_Tangent *)new_m;
      q->curve = BKE_curvemapping_copy(p->curve);
      q->flags = p->flags;
      break;
    }
    case LS_MODIFIER_NOISE: {
      LineStyleAlphaModifier_Noise *p = (LineStyleAlphaModifier_Noise *)m;
      LineStyleAlphaModifier_Noise *q = (LineStyleAlphaModifier_Noise *)new_m;
      q->curve = BKE_curvemapping_copy(p->curve);
      q->flags = p->flags;
      q->amplitude = p->amplitude;
      q->period = p->period;
      q->seed = p->seed;
      break;
    }
    case LS_MODIFIER_CREASE_ANGLE: {
      LineStyleAlphaModifier_CreaseAngle *p = (LineStyleAlphaModifier_CreaseAngle *)m;
      LineStyleAlphaModifier_CreaseAngle *q = (LineStyleAlphaModifier_CreaseAngle *)new_m;
      q->curve = BKE_curvemapping_copy(p->curve);
      q->flags = p->flags;
      q->min_angle = p->min_angle;
      q->max_angle = p->max_angle;
      break;
    }
    case LS_MODIFIER_CURVATURE_3D: {
      LineStyleAlphaModifier_Curvature_3D *p = (LineStyleAlphaModifier_Curvature_3D *)m;
      LineStyleAlphaModifier_Curvature_3D *q = (LineStyleAlphaModifier_Curvature_3D *)new_m;
      q->curve = BKE_curvemapping_copy(p->curve);
      q->flags = p->flags;
      q->min_curvature = p->min_curvature;
      q->max_curvature = p->max_curvature;
      break;
    }
    default:
      return nullptr; /* unknown modifier type */
  }
  add_to_modifier_list(&linestyle->alpha_modifiers, new_m);

  return new_m;
}

int BKE_linestyle_alpha_modifier_remove(FreestyleLineStyle *linestyle, LineStyleModifier *m)
{
  if (BLI_findindex(&linestyle->alpha_modifiers, m) == -1) {
    return -1;
  }
  switch (m->type) {
    case LS_MODIFIER_ALONG_STROKE:
      BKE_curvemapping_free(((LineStyleAlphaModifier_AlongStroke *)m)->curve);
      break;
    case LS_MODIFIER_DISTANCE_FROM_CAMERA:
      BKE_curvemapping_free(((LineStyleAlphaModifier_DistanceFromCamera *)m)->curve);
      break;
    case LS_MODIFIER_DISTANCE_FROM_OBJECT:
      BKE_curvemapping_free(((LineStyleAlphaModifier_DistanceFromObject *)m)->curve);
      break;
    case LS_MODIFIER_MATERIAL:
      BKE_curvemapping_free(((LineStyleAlphaModifier_Material *)m)->curve);
      break;
    case LS_MODIFIER_TANGENT:
      BKE_curvemapping_free(((LineStyleAlphaModifier_Tangent *)m)->curve);
      break;
    case LS_MODIFIER_NOISE:
      BKE_curvemapping_free(((LineStyleAlphaModifier_Noise *)m)->curve);
      break;
    case LS_MODIFIER_CREASE_ANGLE:
      BKE_curvemapping_free(((LineStyleAlphaModifier_CreaseAngle *)m)->curve);
      break;
    case LS_MODIFIER_CURVATURE_3D:
      BKE_curvemapping_free(((LineStyleAlphaModifier_Curvature_3D *)m)->curve);
      break;
  }
  BLI_freelinkN(&linestyle->alpha_modifiers, m);
  return 0;
}

static LineStyleModifier *alloc_thickness_modifier(const char *name, int type)
{
  size_t size;

  switch (type) {
    case LS_MODIFIER_ALONG_STROKE:
      size = sizeof(LineStyleThicknessModifier_AlongStroke);
      break;
    case LS_MODIFIER_DISTANCE_FROM_CAMERA:
      size = sizeof(LineStyleThicknessModifier_DistanceFromCamera);
      break;
    case LS_MODIFIER_DISTANCE_FROM_OBJECT:
      size = sizeof(LineStyleThicknessModifier_DistanceFromObject);
      break;
    case LS_MODIFIER_MATERIAL:
      size = sizeof(LineStyleThicknessModifier_Material);
      break;
    case LS_MODIFIER_CALLIGRAPHY:
      size = sizeof(LineStyleThicknessModifier_Calligraphy);
      break;
    case LS_MODIFIER_TANGENT:
      size = sizeof(LineStyleThicknessModifier_Tangent);
      break;
    case LS_MODIFIER_NOISE:
      size = sizeof(LineStyleThicknessModifier_Noise);
      break;
    case LS_MODIFIER_CREASE_ANGLE:
      size = sizeof(LineStyleThicknessModifier_CreaseAngle);
      break;
    case LS_MODIFIER_CURVATURE_3D:
      size = sizeof(LineStyleThicknessModifier_Curvature_3D);
      break;
    default:
      return nullptr; /* unknown modifier type */
  }

  return new_modifier(name, type, size);
}

LineStyleModifier *BKE_linestyle_thickness_modifier_add(FreestyleLineStyle *linestyle,
                                                        const char *name,
                                                        int type)
{
  LineStyleModifier *m;

  m = alloc_thickness_modifier(name, type);
  m->blend = LS_VALUE_BLEND;

  switch (type) {
    case LS_MODIFIER_ALONG_STROKE: {
      LineStyleThicknessModifier_AlongStroke *p = (LineStyleThicknessModifier_AlongStroke *)m;
      p->curve = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
      p->value_min = 0.0f;
      p->value_max = 1.0f;
      break;
    }
    case LS_MODIFIER_DISTANCE_FROM_CAMERA: {
      LineStyleThicknessModifier_DistanceFromCamera *p =
          (LineStyleThicknessModifier_DistanceFromCamera *)m;
      p->curve = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
      p->range_min = 0.0f;
      p->range_max = 1000.0f;
      p->value_min = 0.0f;
      p->value_max = 1.0f;
      break;
    }
    case LS_MODIFIER_DISTANCE_FROM_OBJECT: {
      LineStyleThicknessModifier_DistanceFromObject *p =
          (LineStyleThicknessModifier_DistanceFromObject *)m;
      p->target = nullptr;
      p->curve = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
      p->range_min = 0.0f;
      p->range_max = 1000.0f;
      p->value_min = 0.0f;
      p->value_max = 1.0f;
      break;
    }
    case LS_MODIFIER_MATERIAL: {
      LineStyleThicknessModifier_Material *p = (LineStyleThicknessModifier_Material *)m;
      p->curve = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
      p->mat_attr = LS_MODIFIER_MATERIAL_LINE;
      p->value_min = 0.0f;
      p->value_max = 1.0f;
      break;
    }
    case LS_MODIFIER_CALLIGRAPHY: {
      LineStyleThicknessModifier_Calligraphy *p = (LineStyleThicknessModifier_Calligraphy *)m;
      p->min_thickness = 1.0f;
      p->max_thickness = 10.0f;
      p->orientation = DEG2RADF(60.0f);
      break;
    }
    case LS_MODIFIER_TANGENT: {
      LineStyleThicknessModifier_Tangent *p = (LineStyleThicknessModifier_Tangent *)m;
      p->curve = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
      p->min_thickness = 1.0f;
      p->max_thickness = 10.0f;
      break;
    }
    case LS_MODIFIER_NOISE: {
      LineStyleThicknessModifier_Noise *p = (LineStyleThicknessModifier_Noise *)m;
      p->period = 10.0f;
      p->amplitude = 10.0f;
      p->seed = 512;
      p->flags = LS_THICKNESS_ASYMMETRIC;
      break;
    }
    case LS_MODIFIER_CREASE_ANGLE: {
      LineStyleThicknessModifier_CreaseAngle *p = (LineStyleThicknessModifier_CreaseAngle *)m;
      p->curve = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
      p->min_angle = 0.0f;
      p->max_angle = DEG2RADF(180.0f);
      p->min_thickness = 1.0f;
      p->max_thickness = 10.0f;
      break;
    }
    case LS_MODIFIER_CURVATURE_3D: {
      LineStyleThicknessModifier_Curvature_3D *p = (LineStyleThicknessModifier_Curvature_3D *)m;
      p->curve = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
      p->min_curvature = 0.0f;
      p->max_curvature = 0.5f;
      p->min_thickness = 1.0f;
      p->max_thickness = 10.0f;
      break;
    }
    default:
      return nullptr; /* unknown modifier type */
  }
  add_to_modifier_list(&linestyle->thickness_modifiers, m);

  return m;
}

LineStyleModifier *BKE_linestyle_thickness_modifier_copy(FreestyleLineStyle *linestyle,
                                                         const LineStyleModifier *m,
                                                         const int flag)
{
  LineStyleModifier *new_m;

  new_m = alloc_thickness_modifier(m->name, m->type);
  if (!new_m) {
    return nullptr;
  }
  new_m->influence = m->influence;
  new_m->flags = m->flags;
  new_m->blend = m->blend;

  switch (m->type) {
    case LS_MODIFIER_ALONG_STROKE: {
      LineStyleThicknessModifier_AlongStroke *p = (LineStyleThicknessModifier_AlongStroke *)m;
      LineStyleThicknessModifier_AlongStroke *q = (LineStyleThicknessModifier_AlongStroke *)new_m;
      q->curve = BKE_curvemapping_copy(p->curve);
      q->flags = p->flags;
      q->value_min = p->value_min;
      q->value_max = p->value_max;
      break;
    }
    case LS_MODIFIER_DISTANCE_FROM_CAMERA: {
      LineStyleThicknessModifier_DistanceFromCamera *p =
          (LineStyleThicknessModifier_DistanceFromCamera *)m;
      LineStyleThicknessModifier_DistanceFromCamera *q =
          (LineStyleThicknessModifier_DistanceFromCamera *)new_m;
      q->curve = BKE_curvemapping_copy(p->curve);
      q->flags = p->flags;
      q->range_min = p->range_min;
      q->range_max = p->range_max;
      q->value_min = p->value_min;
      q->value_max = p->value_max;
      break;
    }
    case LS_MODIFIER_DISTANCE_FROM_OBJECT: {
      LineStyleThicknessModifier_DistanceFromObject *p =
          (LineStyleThicknessModifier_DistanceFromObject *)m;
      LineStyleThicknessModifier_DistanceFromObject *q =
          (LineStyleThicknessModifier_DistanceFromObject *)new_m;
      q->target = p->target;
      if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
        id_us_plus((ID *)q->target);
      }
      q->curve = BKE_curvemapping_copy(p->curve);
      q->flags = p->flags;
      q->range_min = p->range_min;
      q->range_max = p->range_max;
      q->value_min = p->value_min;
      q->value_max = p->value_max;
      break;
    }
    case LS_MODIFIER_MATERIAL: {
      LineStyleThicknessModifier_Material *p = (LineStyleThicknessModifier_Material *)m;
      LineStyleThicknessModifier_Material *q = (LineStyleThicknessModifier_Material *)new_m;
      q->curve = BKE_curvemapping_copy(p->curve);
      q->flags = p->flags;
      q->mat_attr = p->mat_attr;
      q->value_min = p->value_min;
      q->value_max = p->value_max;
      break;
    }
    case LS_MODIFIER_CALLIGRAPHY: {
      LineStyleThicknessModifier_Calligraphy *p = (LineStyleThicknessModifier_Calligraphy *)m;
      LineStyleThicknessModifier_Calligraphy *q = (LineStyleThicknessModifier_Calligraphy *)new_m;
      q->min_thickness = p->min_thickness;
      q->max_thickness = p->max_thickness;
      q->orientation = p->orientation;
      break;
    }
    case LS_MODIFIER_TANGENT: {
      LineStyleThicknessModifier_Tangent *p = (LineStyleThicknessModifier_Tangent *)m;
      LineStyleThicknessModifier_Tangent *q = (LineStyleThicknessModifier_Tangent *)new_m;
      q->curve = BKE_curvemapping_copy(p->curve);
      q->flags = p->flags;
      q->min_thickness = p->min_thickness;
      q->max_thickness = p->max_thickness;
      break;
    }
    case LS_MODIFIER_NOISE: {
      LineStyleThicknessModifier_Noise *p = (LineStyleThicknessModifier_Noise *)m;
      LineStyleThicknessModifier_Noise *q = (LineStyleThicknessModifier_Noise *)new_m;
      q->amplitude = p->amplitude;
      q->period = p->period;
      q->seed = p->seed;
      q->flags = p->flags;
      break;
    }
    case LS_MODIFIER_CURVATURE_3D: {
      LineStyleThicknessModifier_Curvature_3D *p = (LineStyleThicknessModifier_Curvature_3D *)m;
      LineStyleThicknessModifier_Curvature_3D *q = (LineStyleThicknessModifier_Curvature_3D *)
          new_m;
      q->curve = BKE_curvemapping_copy(p->curve);
      q->flags = p->flags;
      q->min_curvature = p->min_curvature;
      q->max_curvature = p->max_curvature;
      q->min_thickness = p->min_thickness;
      q->max_thickness = p->max_thickness;
      break;
    }
    case LS_MODIFIER_CREASE_ANGLE: {
      LineStyleThicknessModifier_CreaseAngle *p = (LineStyleThicknessModifier_CreaseAngle *)m;
      LineStyleThicknessModifier_CreaseAngle *q = (LineStyleThicknessModifier_CreaseAngle *)new_m;
      q->curve = BKE_curvemapping_copy(p->curve);
      q->flags = p->flags;
      q->min_angle = p->min_angle;
      q->max_angle = p->max_angle;
      q->min_thickness = p->min_thickness;
      q->max_thickness = p->max_thickness;
      break;
    }
    default:
      return nullptr; /* unknown modifier type */
  }
  add_to_modifier_list(&linestyle->thickness_modifiers, new_m);

  return new_m;
}

int BKE_linestyle_thickness_modifier_remove(FreestyleLineStyle *linestyle, LineStyleModifier *m)
{
  if (BLI_findindex(&linestyle->thickness_modifiers, m) == -1) {
    return -1;
  }
  switch (m->type) {
    case LS_MODIFIER_ALONG_STROKE:
      BKE_curvemapping_free(((LineStyleThicknessModifier_AlongStroke *)m)->curve);
      break;
    case LS_MODIFIER_DISTANCE_FROM_CAMERA:
      BKE_curvemapping_free(((LineStyleThicknessModifier_DistanceFromCamera *)m)->curve);
      break;
    case LS_MODIFIER_DISTANCE_FROM_OBJECT:
      BKE_curvemapping_free(((LineStyleThicknessModifier_DistanceFromObject *)m)->curve);
      break;
    case LS_MODIFIER_MATERIAL:
      BKE_curvemapping_free(((LineStyleThicknessModifier_Material *)m)->curve);
      break;
    case LS_MODIFIER_CALLIGRAPHY:
      break;
    case LS_MODIFIER_TANGENT:
      BKE_curvemapping_free(((LineStyleThicknessModifier_Tangent *)m)->curve);
      break;
    case LS_MODIFIER_NOISE:
      break;
    case LS_MODIFIER_CREASE_ANGLE:
      break;
    case LS_MODIFIER_CURVATURE_3D:
      break;
  }
  BLI_freelinkN(&linestyle->thickness_modifiers, m);
  return 0;
}

static LineStyleModifier *alloc_geometry_modifier(const char *name, int type)
{
  size_t size;

  switch (type) {
    case LS_MODIFIER_SAMPLING:
      size = sizeof(LineStyleGeometryModifier_Sampling);
      break;
    case LS_MODIFIER_BEZIER_CURVE:
      size = sizeof(LineStyleGeometryModifier_BezierCurve);
      break;
    case LS_MODIFIER_SINUS_DISPLACEMENT:
      size = sizeof(LineStyleGeometryModifier_SinusDisplacement);
      break;
    case LS_MODIFIER_SPATIAL_NOISE:
      size = sizeof(LineStyleGeometryModifier_SpatialNoise);
      break;
    case LS_MODIFIER_PERLIN_NOISE_1D:
      size = sizeof(LineStyleGeometryModifier_PerlinNoise1D);
      break;
    case LS_MODIFIER_PERLIN_NOISE_2D:
      size = sizeof(LineStyleGeometryModifier_PerlinNoise2D);
      break;
    case LS_MODIFIER_BACKBONE_STRETCHER:
      size = sizeof(LineStyleGeometryModifier_BackboneStretcher);
      break;
    case LS_MODIFIER_TIP_REMOVER:
      size = sizeof(LineStyleGeometryModifier_TipRemover);
      break;
    case LS_MODIFIER_POLYGONIZATION:
      size = sizeof(LineStyleGeometryModifier_Polygonalization);
      break;
    case LS_MODIFIER_GUIDING_LINES:
      size = sizeof(LineStyleGeometryModifier_GuidingLines);
      break;
    case LS_MODIFIER_BLUEPRINT:
      size = sizeof(LineStyleGeometryModifier_Blueprint);
      break;
    case LS_MODIFIER_2D_OFFSET:
      size = sizeof(LineStyleGeometryModifier_2DOffset);
      break;
    case LS_MODIFIER_2D_TRANSFORM:
      size = sizeof(LineStyleGeometryModifier_2DTransform);
      break;
    case LS_MODIFIER_SIMPLIFICATION:
      size = sizeof(LineStyleGeometryModifier_Simplification);
      break;
    default:
      return nullptr; /* unknown modifier type */
  }

  return new_modifier(name, type, size);
}

LineStyleModifier *BKE_linestyle_geometry_modifier_add(FreestyleLineStyle *linestyle,
                                                       const char *name,
                                                       int type)
{
  LineStyleModifier *m;

  m = alloc_geometry_modifier(name, type);

  switch (type) {
    case LS_MODIFIER_SAMPLING: {
      LineStyleGeometryModifier_Sampling *p = (LineStyleGeometryModifier_Sampling *)m;
      p->sampling = 10.0f;
      break;
    }
    case LS_MODIFIER_BEZIER_CURVE: {
      LineStyleGeometryModifier_BezierCurve *p = (LineStyleGeometryModifier_BezierCurve *)m;
      p->error = 10.0f;
      break;
    }
    case LS_MODIFIER_SINUS_DISPLACEMENT: {
      LineStyleGeometryModifier_SinusDisplacement *p =
          (LineStyleGeometryModifier_SinusDisplacement *)m;
      p->wavelength = 20.0f;
      p->amplitude = 5.0f;
      p->phase = 0.0f;
      break;
    }
    case LS_MODIFIER_SPATIAL_NOISE: {
      LineStyleGeometryModifier_SpatialNoise *p = (LineStyleGeometryModifier_SpatialNoise *)m;
      p->amplitude = 5.0f;
      p->scale = 20.0f;
      p->octaves = 4;
      p->flags = LS_MODIFIER_SPATIAL_NOISE_SMOOTH | LS_MODIFIER_SPATIAL_NOISE_PURERANDOM;
      break;
    }
    case LS_MODIFIER_PERLIN_NOISE_1D: {
      LineStyleGeometryModifier_PerlinNoise1D *p = (LineStyleGeometryModifier_PerlinNoise1D *)m;
      p->frequency = 10.0f;
      p->amplitude = 10.0f;
      p->octaves = 4;
      p->angle = DEG2RADF(45.0f);
      break;
    }
    case LS_MODIFIER_PERLIN_NOISE_2D: {
      LineStyleGeometryModifier_PerlinNoise2D *p = (LineStyleGeometryModifier_PerlinNoise2D *)m;
      p->frequency = 10.0f;
      p->amplitude = 10.0f;
      p->octaves = 4;
      p->angle = DEG2RADF(45.0f);
      break;
    }
    case LS_MODIFIER_BACKBONE_STRETCHER: {
      LineStyleGeometryModifier_BackboneStretcher *p =
          (LineStyleGeometryModifier_BackboneStretcher *)m;
      p->backbone_length = 10.0f;
      break;
    }
    case LS_MODIFIER_TIP_REMOVER: {
      LineStyleGeometryModifier_TipRemover *p = (LineStyleGeometryModifier_TipRemover *)m;
      p->tip_length = 10.0f;
      break;
    }
    case LS_MODIFIER_POLYGONIZATION: {
      LineStyleGeometryModifier_Polygonalization *p =
          (LineStyleGeometryModifier_Polygonalization *)m;
      p->error = 10.0f;
      break;
    }
    case LS_MODIFIER_GUIDING_LINES: {
      LineStyleGeometryModifier_GuidingLines *p = (LineStyleGeometryModifier_GuidingLines *)m;
      p->offset = 0.0f;
      break;
    }
    case LS_MODIFIER_BLUEPRINT: {
      LineStyleGeometryModifier_Blueprint *p = (LineStyleGeometryModifier_Blueprint *)m;
      p->flags = LS_MODIFIER_BLUEPRINT_CIRCLES;
      p->rounds = 1;
      p->backbone_length = 10.0f;
      p->random_radius = 3;
      p->random_center = 5;
      p->random_backbone = 5;
      break;
    }
    case LS_MODIFIER_2D_OFFSET: {
      LineStyleGeometryModifier_2DOffset *p = (LineStyleGeometryModifier_2DOffset *)m;
      p->start = 0.0f;
      p->end = 0.0f;
      p->x = 0.0f;
      p->y = 0.0f;
      break;
    }
    case LS_MODIFIER_2D_TRANSFORM: {
      LineStyleGeometryModifier_2DTransform *p = (LineStyleGeometryModifier_2DTransform *)m;
      p->pivot = LS_MODIFIER_2D_TRANSFORM_PIVOT_CENTER;
      p->scale_x = 1.0f;
      p->scale_y = 1.0f;
      p->angle = DEG2RADF(0.0f);
      p->pivot_u = 0.5f;
      p->pivot_x = 0.0f;
      p->pivot_y = 0.0f;
      break;
    }
    case LS_MODIFIER_SIMPLIFICATION: {
      LineStyleGeometryModifier_Simplification *p = (LineStyleGeometryModifier_Simplification *)m;
      p->tolerance = 0.1f;
      break;
    }
    default:
      return nullptr; /* unknown modifier type */
  }
  add_to_modifier_list(&linestyle->geometry_modifiers, m);

  return m;
}

LineStyleModifier *BKE_linestyle_geometry_modifier_copy(FreestyleLineStyle *linestyle,
                                                        const LineStyleModifier *m,
                                                        const int /*flag*/)
{
  LineStyleModifier *new_m;

  new_m = alloc_geometry_modifier(m->name, m->type);
  new_m->flags = m->flags;

  switch (m->type) {
    case LS_MODIFIER_SAMPLING: {
      LineStyleGeometryModifier_Sampling *p = (LineStyleGeometryModifier_Sampling *)m;
      LineStyleGeometryModifier_Sampling *q = (LineStyleGeometryModifier_Sampling *)new_m;
      q->sampling = p->sampling;
      break;
    }
    case LS_MODIFIER_BEZIER_CURVE: {
      LineStyleGeometryModifier_BezierCurve *p = (LineStyleGeometryModifier_BezierCurve *)m;
      LineStyleGeometryModifier_BezierCurve *q = (LineStyleGeometryModifier_BezierCurve *)new_m;
      q->error = p->error;
      break;
    }
    case LS_MODIFIER_SINUS_DISPLACEMENT: {
      LineStyleGeometryModifier_SinusDisplacement *p =
          (LineStyleGeometryModifier_SinusDisplacement *)m;
      LineStyleGeometryModifier_SinusDisplacement *q =
          (LineStyleGeometryModifier_SinusDisplacement *)new_m;
      q->wavelength = p->wavelength;
      q->amplitude = p->amplitude;
      q->phase = p->phase;
      break;
    }
    case LS_MODIFIER_SPATIAL_NOISE: {
      LineStyleGeometryModifier_SpatialNoise *p = (LineStyleGeometryModifier_SpatialNoise *)m;
      LineStyleGeometryModifier_SpatialNoise *q = (LineStyleGeometryModifier_SpatialNoise *)new_m;
      q->amplitude = p->amplitude;
      q->scale = p->scale;
      q->octaves = p->octaves;
      q->flags = p->flags;
      break;
    }
    case LS_MODIFIER_PERLIN_NOISE_1D: {
      LineStyleGeometryModifier_PerlinNoise1D *p = (LineStyleGeometryModifier_PerlinNoise1D *)m;
      LineStyleGeometryModifier_PerlinNoise1D *q = (LineStyleGeometryModifier_PerlinNoise1D *)
          new_m;
      q->frequency = p->frequency;
      q->amplitude = p->amplitude;
      q->angle = p->angle;
      q->octaves = p->octaves;
      q->seed = p->seed;
      break;
    }
    case LS_MODIFIER_PERLIN_NOISE_2D: {
      LineStyleGeometryModifier_PerlinNoise2D *p = (LineStyleGeometryModifier_PerlinNoise2D *)m;
      LineStyleGeometryModifier_PerlinNoise2D *q = (LineStyleGeometryModifier_PerlinNoise2D *)
          new_m;
      q->frequency = p->frequency;
      q->amplitude = p->amplitude;
      q->angle = p->angle;
      q->octaves = p->octaves;
      q->seed = p->seed;
      break;
    }
    case LS_MODIFIER_BACKBONE_STRETCHER: {
      LineStyleGeometryModifier_BackboneStretcher *p =
          (LineStyleGeometryModifier_BackboneStretcher *)m;
      LineStyleGeometryModifier_BackboneStretcher *q =
          (LineStyleGeometryModifier_BackboneStretcher *)new_m;
      q->backbone_length = p->backbone_length;
      break;
    }
    case LS_MODIFIER_TIP_REMOVER: {
      LineStyleGeometryModifier_TipRemover *p = (LineStyleGeometryModifier_TipRemover *)m;
      LineStyleGeometryModifier_TipRemover *q = (LineStyleGeometryModifier_TipRemover *)new_m;
      q->tip_length = p->tip_length;
      break;
    }
    case LS_MODIFIER_POLYGONIZATION: {
      LineStyleGeometryModifier_Polygonalization *p =
          (LineStyleGeometryModifier_Polygonalization *)m;
      LineStyleGeometryModifier_Polygonalization *q =
          (LineStyleGeometryModifier_Polygonalization *)new_m;
      q->error = p->error;
      break;
    }
    case LS_MODIFIER_GUIDING_LINES: {
      LineStyleGeometryModifier_GuidingLines *p = (LineStyleGeometryModifier_GuidingLines *)m;
      LineStyleGeometryModifier_GuidingLines *q = (LineStyleGeometryModifier_GuidingLines *)new_m;
      q->offset = p->offset;
      break;
    }
    case LS_MODIFIER_BLUEPRINT: {
      LineStyleGeometryModifier_Blueprint *p = (LineStyleGeometryModifier_Blueprint *)m;
      LineStyleGeometryModifier_Blueprint *q = (LineStyleGeometryModifier_Blueprint *)new_m;
      q->flags = p->flags;
      q->rounds = p->rounds;
      q->backbone_length = p->backbone_length;
      q->random_radius = p->random_radius;
      q->random_center = p->random_center;
      q->random_backbone = p->random_backbone;
      break;
    }
    case LS_MODIFIER_2D_OFFSET: {
      LineStyleGeometryModifier_2DOffset *p = (LineStyleGeometryModifier_2DOffset *)m;
      LineStyleGeometryModifier_2DOffset *q = (LineStyleGeometryModifier_2DOffset *)new_m;
      q->start = p->start;
      q->end = p->end;
      q->x = p->x;
      q->y = p->y;
      break;
    }
    case LS_MODIFIER_2D_TRANSFORM: {
      LineStyleGeometryModifier_2DTransform *p = (LineStyleGeometryModifier_2DTransform *)m;
      LineStyleGeometryModifier_2DTransform *q = (LineStyleGeometryModifier_2DTransform *)new_m;
      q->pivot = p->pivot;
      q->scale_x = p->scale_x;
      q->scale_y = p->scale_y;
      q->angle = p->angle;
      q->pivot_u = p->pivot_u;
      q->pivot_x = p->pivot_x;
      q->pivot_y = p->pivot_y;
      break;
    }
    case LS_MODIFIER_SIMPLIFICATION: {
      LineStyleGeometryModifier_Simplification *p = (LineStyleGeometryModifier_Simplification *)m;
      LineStyleGeometryModifier_Simplification *q = (LineStyleGeometryModifier_Simplification *)
          new_m;
      q->tolerance = p->tolerance;
      break;
    }
    default:
      return nullptr; /* unknown modifier type */
  }
  add_to_modifier_list(&linestyle->geometry_modifiers, new_m);

  return new_m;
}

int BKE_linestyle_geometry_modifier_remove(FreestyleLineStyle *linestyle, LineStyleModifier *m)
{
  if (BLI_findindex(&linestyle->geometry_modifiers, m) == -1) {
    return -1;
  }
  BLI_freelinkN(&linestyle->geometry_modifiers, m);
  return 0;
}

bool BKE_linestyle_color_modifier_move(FreestyleLineStyle *linestyle,
                                       LineStyleModifier *modifier,
                                       int direction)
{
  return BLI_listbase_link_move(&linestyle->color_modifiers, modifier, direction);
}
bool BKE_linestyle_alpha_modifier_move(FreestyleLineStyle *linestyle,
                                       LineStyleModifier *modifier,
                                       int direction)
{
  return BLI_listbase_link_move(&linestyle->alpha_modifiers, modifier, direction);
}
bool BKE_linestyle_thickness_modifier_move(FreestyleLineStyle *linestyle,
                                           LineStyleModifier *modifier,
                                           int direction)
{
  return BLI_listbase_link_move(&linestyle->thickness_modifiers, modifier, direction);
}
bool BKE_linestyle_geometry_modifier_move(FreestyleLineStyle *linestyle,
                                          LineStyleModifier *modifier,
                                          int direction)
{
  return BLI_listbase_link_move(&linestyle->geometry_modifiers, modifier, direction);
}

void BKE_linestyle_modifier_list_color_ramps(FreestyleLineStyle *linestyle, ListBase *listbase)
{
  ColorBand *color_ramp;
  LinkData *link;

  BLI_listbase_clear(listbase);

  LISTBASE_FOREACH (LineStyleModifier *, m, &linestyle->color_modifiers) {
    switch (m->type) {
      case LS_MODIFIER_ALONG_STROKE:
        color_ramp = ((LineStyleColorModifier_AlongStroke *)m)->color_ramp;
        break;
      case LS_MODIFIER_DISTANCE_FROM_CAMERA:
        color_ramp = ((LineStyleColorModifier_DistanceFromCamera *)m)->color_ramp;
        break;
      case LS_MODIFIER_DISTANCE_FROM_OBJECT:
        color_ramp = ((LineStyleColorModifier_DistanceFromObject *)m)->color_ramp;
        break;
      case LS_MODIFIER_MATERIAL:
        color_ramp = ((LineStyleColorModifier_Material *)m)->color_ramp;
        break;
      default:
        continue;
    }
    link = MEM_callocN<LinkData>("link to color ramp");
    link->data = color_ramp;
    BLI_addtail(listbase, link);
  }
}

std::optional<std::string> BKE_linestyle_path_to_color_ramp(FreestyleLineStyle *linestyle,
                                                            const ColorBand *color_ramp)
{
  bool found = false;

  LISTBASE_FOREACH (LineStyleModifier *, m, &linestyle->color_modifiers) {
    switch (m->type) {
      case LS_MODIFIER_ALONG_STROKE:
        if (color_ramp == ((LineStyleColorModifier_AlongStroke *)m)->color_ramp) {
          found = true;
        }
        break;
      case LS_MODIFIER_DISTANCE_FROM_CAMERA:
        if (color_ramp == ((LineStyleColorModifier_DistanceFromCamera *)m)->color_ramp) {
          found = true;
        }
        break;
      case LS_MODIFIER_DISTANCE_FROM_OBJECT:
        if (color_ramp == ((LineStyleColorModifier_DistanceFromObject *)m)->color_ramp) {
          found = true;
        }
        break;
      case LS_MODIFIER_MATERIAL:
        if (color_ramp == ((LineStyleColorModifier_Material *)m)->color_ramp) {
          found = true;
        }
        break;
      case LS_MODIFIER_TANGENT:
        if (color_ramp == ((LineStyleColorModifier_Tangent *)m)->color_ramp) {
          found = true;
        }
        break;
      case LS_MODIFIER_NOISE:
        if (color_ramp == ((LineStyleColorModifier_Noise *)m)->color_ramp) {
          found = true;
        }
        break;
      case LS_MODIFIER_CREASE_ANGLE:
        if (color_ramp == ((LineStyleColorModifier_CreaseAngle *)m)->color_ramp) {
          found = true;
        }
        break;
      case LS_MODIFIER_CURVATURE_3D:
        if (color_ramp == ((LineStyleColorModifier_Curvature_3D *)m)->color_ramp) {
          found = true;
        }
        break;
    }

    if (found) {
      char name_esc[sizeof(m->name) * 2];
      BLI_str_escape(name_esc, m->name, sizeof(name_esc));
      return fmt::format("color_modifiers[\"{}\"].color_ramp", name_esc);
    }
  }
  printf("BKE_linestyle_path_to_color_ramp: No color ramps correspond to the given pointer.\n");
  return std::nullopt;
}

bool BKE_linestyle_use_textures(FreestyleLineStyle *linestyle, const bool use_shading_nodes)
{
  if (use_shading_nodes) {
    if (linestyle && linestyle->use_nodes && linestyle->nodetree) {
      LISTBASE_FOREACH (bNode *, node, &linestyle->nodetree->nodes) {
        if (node->typeinfo->nclass == NODE_CLASS_TEXTURE) {
          return true;
        }
      }
    }
  }
  else {
    if (linestyle && (linestyle->flag & LS_TEXTURE)) {
      return (linestyle->mtex[0] != nullptr);
    }
  }
  return false;
}

void BKE_linestyle_default_shader(const bContext *C, FreestyleLineStyle *linestyle)
{
  bNode *uv_along_stroke, *input_texture, *output_linestyle;
  bNodeSocket *fromsock, *tosock;
  bNodeTree *ntree;

  BLI_assert(linestyle->nodetree == nullptr);

  ntree = blender::bke::node_tree_add_tree_embedded(
      nullptr, &linestyle->id, "stroke_shader", "ShaderNodeTree");

  uv_along_stroke = blender::bke::node_add_static_node(C, *ntree, SH_NODE_UVALONGSTROKE);
  uv_along_stroke->location[0] = -200.0f;
  uv_along_stroke->location[1] = 100.0f;
  uv_along_stroke->custom1 = 0; /* use_tips */

  input_texture = blender::bke::node_add_static_node(C, *ntree, SH_NODE_TEX_IMAGE);
  input_texture->location[0] = 0.0f;
  input_texture->location[1] = 100.0f;

  output_linestyle = blender::bke::node_add_static_node(C, *ntree, SH_NODE_OUTPUT_LINESTYLE);
  output_linestyle->location[0] = 300.0f;
  output_linestyle->location[1] = 100.0f;
  output_linestyle->custom1 = MA_RAMP_BLEND;
  output_linestyle->custom2 = 0; /* use_clamp */

  blender::bke::node_set_active(*ntree, *input_texture);

  fromsock = static_cast<bNodeSocket *>(BLI_findlink(&uv_along_stroke->outputs, 0)); /* UV */
  tosock = static_cast<bNodeSocket *>(BLI_findlink(&input_texture->inputs, 0));      /* UV */
  blender::bke::node_add_link(*ntree, *uv_along_stroke, *fromsock, *input_texture, *tosock);

  fromsock = static_cast<bNodeSocket *>(BLI_findlink(&input_texture->outputs, 0)); /* Color */
  tosock = static_cast<bNodeSocket *>(BLI_findlink(&output_linestyle->inputs, 0)); /* Color */
  blender::bke::node_add_link(*ntree, *input_texture, *fromsock, *output_linestyle, *tosock);

  BKE_ntree_update_after_single_tree_change(*CTX_data_main(C), *ntree);
}
