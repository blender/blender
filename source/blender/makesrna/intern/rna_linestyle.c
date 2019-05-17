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
 * \ingroup RNA
 */

#include <stdio.h>
#include <stdlib.h>

#include "BLI_utildefines.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "DNA_linestyle_types.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"

#include "WM_types.h"
#include "WM_api.h"

const EnumPropertyItem rna_enum_linestyle_color_modifier_type_items[] = {
    {LS_MODIFIER_ALONG_STROKE, "ALONG_STROKE", ICON_MODIFIER, "Along Stroke", ""},
    {LS_MODIFIER_CREASE_ANGLE, "CREASE_ANGLE", ICON_MODIFIER, "Crease Angle", ""},
    {LS_MODIFIER_CURVATURE_3D, "CURVATURE_3D", ICON_MODIFIER, "Curvature 3D", ""},
    {LS_MODIFIER_DISTANCE_FROM_CAMERA,
     "DISTANCE_FROM_CAMERA",
     ICON_MODIFIER,
     "Distance from Camera",
     ""},
    {LS_MODIFIER_DISTANCE_FROM_OBJECT,
     "DISTANCE_FROM_OBJECT",
     ICON_MODIFIER,
     "Distance from Object",
     ""},
    {LS_MODIFIER_MATERIAL, "MATERIAL", ICON_MODIFIER, "Material", ""},
    {LS_MODIFIER_NOISE, "NOISE", ICON_MODIFIER, "Noise", ""},
    {LS_MODIFIER_TANGENT, "TANGENT", ICON_MODIFIER, "Tangent", ""},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_linestyle_alpha_modifier_type_items[] = {
    {LS_MODIFIER_ALONG_STROKE, "ALONG_STROKE", ICON_MODIFIER, "Along Stroke", ""},
    {LS_MODIFIER_CREASE_ANGLE, "CREASE_ANGLE", ICON_MODIFIER, "Crease Angle", ""},
    {LS_MODIFIER_CURVATURE_3D, "CURVATURE_3D", ICON_MODIFIER, "Curvature 3D", ""},
    {LS_MODIFIER_DISTANCE_FROM_CAMERA,
     "DISTANCE_FROM_CAMERA",
     ICON_MODIFIER,
     "Distance from Camera",
     ""},
    {LS_MODIFIER_DISTANCE_FROM_OBJECT,
     "DISTANCE_FROM_OBJECT",
     ICON_MODIFIER,
     "Distance from Object",
     ""},
    {LS_MODIFIER_MATERIAL, "MATERIAL", ICON_MODIFIER, "Material", ""},
    {LS_MODIFIER_NOISE, "NOISE", ICON_MODIFIER, "Noise", ""},
    {LS_MODIFIER_TANGENT, "TANGENT", ICON_MODIFIER, "Tangent", ""},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_linestyle_thickness_modifier_type_items[] = {
    {LS_MODIFIER_ALONG_STROKE, "ALONG_STROKE", ICON_MODIFIER, "Along Stroke", ""},
    {LS_MODIFIER_CALLIGRAPHY, "CALLIGRAPHY", ICON_MODIFIER, "Calligraphy", ""},
    {LS_MODIFIER_CREASE_ANGLE, "CREASE_ANGLE", ICON_MODIFIER, "Crease Angle", ""},
    {LS_MODIFIER_CURVATURE_3D, "CURVATURE_3D", ICON_MODIFIER, "Curvature 3D", ""},
    {LS_MODIFIER_DISTANCE_FROM_CAMERA,
     "DISTANCE_FROM_CAMERA",
     ICON_MODIFIER,
     "Distance from Camera",
     ""},
    {LS_MODIFIER_DISTANCE_FROM_OBJECT,
     "DISTANCE_FROM_OBJECT",
     ICON_MODIFIER,
     "Distance from Object",
     ""},
    {LS_MODIFIER_MATERIAL, "MATERIAL", ICON_MODIFIER, "Material", ""},
    {LS_MODIFIER_NOISE, "NOISE", ICON_MODIFIER, "Noise", ""},
    {LS_MODIFIER_TANGENT, "TANGENT", ICON_MODIFIER, "Tangent", ""},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_linestyle_geometry_modifier_type_items[] = {
    {LS_MODIFIER_2D_OFFSET, "2D_OFFSET", ICON_MODIFIER, "2D Offset", ""},
    {LS_MODIFIER_2D_TRANSFORM, "2D_TRANSFORM", ICON_MODIFIER, "2D Transform", ""},
    {LS_MODIFIER_BACKBONE_STRETCHER,
     "BACKBONE_STRETCHER",
     ICON_MODIFIER,
     "Backbone Stretcher",
     ""},
    {LS_MODIFIER_BEZIER_CURVE, "BEZIER_CURVE", ICON_MODIFIER, "Bezier Curve", ""},
    {LS_MODIFIER_BLUEPRINT, "BLUEPRINT", ICON_MODIFIER, "Blueprint", ""},
    {LS_MODIFIER_GUIDING_LINES, "GUIDING_LINES", ICON_MODIFIER, "Guiding Lines", ""},
    {LS_MODIFIER_PERLIN_NOISE_1D, "PERLIN_NOISE_1D", ICON_MODIFIER, "Perlin Noise 1D", ""},
    {LS_MODIFIER_PERLIN_NOISE_2D, "PERLIN_NOISE_2D", ICON_MODIFIER, "Perlin Noise 2D", ""},
    {LS_MODIFIER_POLYGONIZATION, "POLYGONIZATION", ICON_MODIFIER, "Polygonization", ""},
    {LS_MODIFIER_SAMPLING, "SAMPLING", ICON_MODIFIER, "Sampling", ""},
    {LS_MODIFIER_SIMPLIFICATION, "SIMPLIFICATION", ICON_MODIFIER, "Simplification", ""},
    {LS_MODIFIER_SINUS_DISPLACEMENT,
     "SINUS_DISPLACEMENT",
     ICON_MODIFIER,
     "Sinus Displacement",
     ""},
    {LS_MODIFIER_SPATIAL_NOISE, "SPATIAL_NOISE", ICON_MODIFIER, "Spatial Noise", ""},
    {LS_MODIFIER_TIP_REMOVER, "TIP_REMOVER", ICON_MODIFIER, "Tip Remover", ""},
    {0, NULL, 0, NULL, NULL},
};

#ifdef RNA_RUNTIME

#  include "BLI_string_utils.h"

#  include "BKE_linestyle.h"
#  include "BKE_texture.h"

#  include "DEG_depsgraph.h"

#  include "ED_node.h"

#  include "RNA_access.h"

static StructRNA *rna_LineStyle_color_modifier_refine(struct PointerRNA *ptr)
{
  LineStyleModifier *m = (LineStyleModifier *)ptr->data;

  switch (m->type) {
    case LS_MODIFIER_ALONG_STROKE:
      return &RNA_LineStyleColorModifier_AlongStroke;
    case LS_MODIFIER_DISTANCE_FROM_CAMERA:
      return &RNA_LineStyleColorModifier_DistanceFromCamera;
    case LS_MODIFIER_DISTANCE_FROM_OBJECT:
      return &RNA_LineStyleColorModifier_DistanceFromObject;
    case LS_MODIFIER_MATERIAL:
      return &RNA_LineStyleColorModifier_Material;
    case LS_MODIFIER_TANGENT:
      return &RNA_LineStyleColorModifier_Tangent;
    case LS_MODIFIER_NOISE:
      return &RNA_LineStyleColorModifier_Noise;
    case LS_MODIFIER_CREASE_ANGLE:
      return &RNA_LineStyleColorModifier_CreaseAngle;
    case LS_MODIFIER_CURVATURE_3D:
      return &RNA_LineStyleColorModifier_Curvature_3D;
    default:
      return &RNA_LineStyleColorModifier;
  }
}

static StructRNA *rna_LineStyle_alpha_modifier_refine(struct PointerRNA *ptr)
{
  LineStyleModifier *m = (LineStyleModifier *)ptr->data;

  switch (m->type) {
    case LS_MODIFIER_ALONG_STROKE:
      return &RNA_LineStyleAlphaModifier_AlongStroke;
    case LS_MODIFIER_DISTANCE_FROM_CAMERA:
      return &RNA_LineStyleAlphaModifier_DistanceFromCamera;
    case LS_MODIFIER_DISTANCE_FROM_OBJECT:
      return &RNA_LineStyleAlphaModifier_DistanceFromObject;
    case LS_MODIFIER_MATERIAL:
      return &RNA_LineStyleAlphaModifier_Material;
    case LS_MODIFIER_TANGENT:
      return &RNA_LineStyleAlphaModifier_Tangent;
    case LS_MODIFIER_NOISE:
      return &RNA_LineStyleAlphaModifier_Noise;
    case LS_MODIFIER_CREASE_ANGLE:
      return &RNA_LineStyleAlphaModifier_CreaseAngle;
    case LS_MODIFIER_CURVATURE_3D:
      return &RNA_LineStyleAlphaModifier_Curvature_3D;
    default:
      return &RNA_LineStyleAlphaModifier;
  }
}

static StructRNA *rna_LineStyle_thickness_modifier_refine(struct PointerRNA *ptr)
{
  LineStyleModifier *m = (LineStyleModifier *)ptr->data;

  switch (m->type) {
    case LS_MODIFIER_ALONG_STROKE:
      return &RNA_LineStyleThicknessModifier_AlongStroke;
    case LS_MODIFIER_DISTANCE_FROM_CAMERA:
      return &RNA_LineStyleThicknessModifier_DistanceFromCamera;
    case LS_MODIFIER_DISTANCE_FROM_OBJECT:
      return &RNA_LineStyleThicknessModifier_DistanceFromObject;
    case LS_MODIFIER_MATERIAL:
      return &RNA_LineStyleThicknessModifier_Material;
    case LS_MODIFIER_CALLIGRAPHY:
      return &RNA_LineStyleThicknessModifier_Calligraphy;
    case LS_MODIFIER_TANGENT:
      return &RNA_LineStyleThicknessModifier_Tangent;
    case LS_MODIFIER_NOISE:
      return &RNA_LineStyleThicknessModifier_Noise;
    case LS_MODIFIER_CREASE_ANGLE:
      return &RNA_LineStyleThicknessModifier_CreaseAngle;
    case LS_MODIFIER_CURVATURE_3D:
      return &RNA_LineStyleThicknessModifier_Curvature_3D;
    default:
      return &RNA_LineStyleThicknessModifier;
  }
}

static StructRNA *rna_LineStyle_geometry_modifier_refine(struct PointerRNA *ptr)
{
  LineStyleModifier *m = (LineStyleModifier *)ptr->data;

  switch (m->type) {
    case LS_MODIFIER_SAMPLING:
      return &RNA_LineStyleGeometryModifier_Sampling;
    case LS_MODIFIER_BEZIER_CURVE:
      return &RNA_LineStyleGeometryModifier_BezierCurve;
    case LS_MODIFIER_SINUS_DISPLACEMENT:
      return &RNA_LineStyleGeometryModifier_SinusDisplacement;
    case LS_MODIFIER_SPATIAL_NOISE:
      return &RNA_LineStyleGeometryModifier_SpatialNoise;
    case LS_MODIFIER_PERLIN_NOISE_1D:
      return &RNA_LineStyleGeometryModifier_PerlinNoise1D;
    case LS_MODIFIER_PERLIN_NOISE_2D:
      return &RNA_LineStyleGeometryModifier_PerlinNoise2D;
    case LS_MODIFIER_BACKBONE_STRETCHER:
      return &RNA_LineStyleGeometryModifier_BackboneStretcher;
    case LS_MODIFIER_TIP_REMOVER:
      return &RNA_LineStyleGeometryModifier_TipRemover;
    case LS_MODIFIER_POLYGONIZATION:
      return &RNA_LineStyleGeometryModifier_Polygonalization;
    case LS_MODIFIER_GUIDING_LINES:
      return &RNA_LineStyleGeometryModifier_GuidingLines;
    case LS_MODIFIER_BLUEPRINT:
      return &RNA_LineStyleGeometryModifier_Blueprint;
    case LS_MODIFIER_2D_OFFSET:
      return &RNA_LineStyleGeometryModifier_2DOffset;
    case LS_MODIFIER_2D_TRANSFORM:
      return &RNA_LineStyleGeometryModifier_2DTransform;
    case LS_MODIFIER_SIMPLIFICATION:
      return &RNA_LineStyleGeometryModifier_Simplification;
    default:
      return &RNA_LineStyleGeometryModifier;
  }
}

static char *rna_LineStyle_color_modifier_path(PointerRNA *ptr)
{
  LineStyleModifier *m = (LineStyleModifier *)ptr->data;
  char name_esc[sizeof(m->name) * 2];
  BLI_strescape(name_esc, m->name, sizeof(name_esc));
  return BLI_sprintfN("color_modifiers[\"%s\"]", name_esc);
}

static char *rna_LineStyle_alpha_modifier_path(PointerRNA *ptr)
{
  LineStyleModifier *m = (LineStyleModifier *)ptr->data;
  char name_esc[sizeof(m->name) * 2];
  BLI_strescape(name_esc, m->name, sizeof(name_esc));
  return BLI_sprintfN("alpha_modifiers[\"%s\"]", name_esc);
}

static char *rna_LineStyle_thickness_modifier_path(PointerRNA *ptr)
{
  LineStyleModifier *m = (LineStyleModifier *)ptr->data;
  char name_esc[sizeof(m->name) * 2];
  BLI_strescape(name_esc, m->name, sizeof(name_esc));
  return BLI_sprintfN("thickness_modifiers[\"%s\"]", name_esc);
}

static char *rna_LineStyle_geometry_modifier_path(PointerRNA *ptr)
{
  LineStyleModifier *m = (LineStyleModifier *)ptr->data;
  char name_esc[sizeof(m->name) * 2];
  BLI_strescape(name_esc, m->name, sizeof(name_esc));
  return BLI_sprintfN("geometry_modifiers[\"%s\"]", name_esc);
}

static void rna_LineStyleColorModifier_name_set(PointerRNA *ptr, const char *value)
{
  FreestyleLineStyle *linestyle = (FreestyleLineStyle *)ptr->id.data;
  LineStyleModifier *m = (LineStyleModifier *)ptr->data;

  BLI_strncpy_utf8(m->name, value, sizeof(m->name));
  BLI_uniquename(&linestyle->color_modifiers,
                 m,
                 "ColorModifier",
                 '.',
                 offsetof(LineStyleModifier, name),
                 sizeof(m->name));
}

static void rna_LineStyleAlphaModifier_name_set(PointerRNA *ptr, const char *value)
{
  FreestyleLineStyle *linestyle = (FreestyleLineStyle *)ptr->id.data;
  LineStyleModifier *m = (LineStyleModifier *)ptr->data;

  BLI_strncpy_utf8(m->name, value, sizeof(m->name));
  BLI_uniquename(&linestyle->alpha_modifiers,
                 m,
                 "AlphaModifier",
                 '.',
                 offsetof(LineStyleModifier, name),
                 sizeof(m->name));
}

static void rna_LineStyleThicknessModifier_name_set(PointerRNA *ptr, const char *value)
{
  FreestyleLineStyle *linestyle = (FreestyleLineStyle *)ptr->id.data;
  LineStyleModifier *m = (LineStyleModifier *)ptr->data;

  BLI_strncpy_utf8(m->name, value, sizeof(m->name));
  BLI_uniquename(&linestyle->thickness_modifiers,
                 m,
                 "ThicknessModifier",
                 '.',
                 offsetof(LineStyleModifier, name),
                 sizeof(m->name));
}

static void rna_LineStyleGeometryModifier_name_set(PointerRNA *ptr, const char *value)
{
  FreestyleLineStyle *linestyle = (FreestyleLineStyle *)ptr->id.data;
  LineStyleModifier *m = (LineStyleModifier *)ptr->data;

  BLI_strncpy_utf8(m->name, value, sizeof(m->name));
  BLI_uniquename(&linestyle->geometry_modifiers,
                 m,
                 "GeometryModifier",
                 '.',
                 offsetof(LineStyleModifier, name),
                 sizeof(m->name));
}

static void rna_LineStyle_mtex_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  FreestyleLineStyle *linestyle = (FreestyleLineStyle *)ptr->id.data;
  rna_iterator_array_begin(iter, (void *)linestyle->mtex, sizeof(MTex *), MAX_MTEX, 0, NULL);
}

static PointerRNA rna_LineStyle_active_texture_get(PointerRNA *ptr)
{
  FreestyleLineStyle *linestyle = (FreestyleLineStyle *)ptr->id.data;
  Tex *tex;

  tex = give_current_linestyle_texture(linestyle);
  return rna_pointer_inherit_refine(ptr, &RNA_Texture, tex);
}

static void rna_LineStyle_active_texture_set(struct ReportList *UNUSED(reports),
                                             PointerRNA *ptr,
                                             PointerRNA value)
{
  FreestyleLineStyle *linestyle = (FreestyleLineStyle *)ptr->id.data;

  set_current_linestyle_texture(linestyle, value.data);
}

static void rna_LineStyle_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  FreestyleLineStyle *linestyle = ptr->id.data;

  DEG_id_tag_update(&linestyle->id, 0);
  WM_main_add_notifier(NC_LINESTYLE, linestyle);
}

static void rna_LineStyle_use_nodes_update(bContext *C, PointerRNA *ptr)
{
  FreestyleLineStyle *linestyle = (FreestyleLineStyle *)ptr->data;

  if (linestyle->use_nodes && linestyle->nodetree == NULL)
    BKE_linestyle_default_shader(C, linestyle);

  rna_LineStyle_update(CTX_data_main(C), CTX_data_scene(C), ptr);
}

static LineStyleModifier *rna_LineStyle_color_modifier_add(FreestyleLineStyle *linestyle,
                                                           ReportList *reports,
                                                           const char *name,
                                                           int type)
{
  LineStyleModifier *modifier = BKE_linestyle_color_modifier_add(linestyle, name, type);

  if (!modifier) {
    BKE_report(reports, RPT_ERROR, "Failed to add the color modifier");
    return NULL;
  }

  DEG_id_tag_update(&linestyle->id, 0);
  WM_main_add_notifier(NC_LINESTYLE, linestyle);

  return modifier;
}

static void rna_LineStyle_color_modifier_remove(FreestyleLineStyle *linestyle,
                                                ReportList *reports,
                                                PointerRNA *modifier_ptr)
{
  LineStyleModifier *modifier = modifier_ptr->data;

  if (BKE_linestyle_color_modifier_remove(linestyle, modifier) == -1) {
    BKE_reportf(reports, RPT_ERROR, "Color modifier '%s' could not be removed", modifier->name);
    return;
  }

  RNA_POINTER_INVALIDATE(modifier_ptr);

  DEG_id_tag_update(&linestyle->id, 0);
  WM_main_add_notifier(NC_LINESTYLE, linestyle);
}

static LineStyleModifier *rna_LineStyle_alpha_modifier_add(FreestyleLineStyle *linestyle,
                                                           ReportList *reports,
                                                           const char *name,
                                                           int type)
{
  LineStyleModifier *modifier = BKE_linestyle_alpha_modifier_add(linestyle, name, type);

  if (!modifier) {
    BKE_report(reports, RPT_ERROR, "Failed to add the alpha modifier");
    return NULL;
  }

  DEG_id_tag_update(&linestyle->id, 0);
  WM_main_add_notifier(NC_LINESTYLE, linestyle);

  return modifier;
}

static void rna_LineStyle_alpha_modifier_remove(FreestyleLineStyle *linestyle,
                                                ReportList *reports,
                                                PointerRNA *modifier_ptr)
{
  LineStyleModifier *modifier = modifier_ptr->data;

  if (BKE_linestyle_alpha_modifier_remove(linestyle, modifier) == -1) {
    BKE_reportf(reports, RPT_ERROR, "Alpha modifier '%s' could not be removed", modifier->name);
    return;
  }

  RNA_POINTER_INVALIDATE(modifier_ptr);

  DEG_id_tag_update(&linestyle->id, 0);
  WM_main_add_notifier(NC_LINESTYLE, linestyle);
}

static LineStyleModifier *rna_LineStyle_thickness_modifier_add(FreestyleLineStyle *linestyle,
                                                               ReportList *reports,
                                                               const char *name,
                                                               int type)
{
  LineStyleModifier *modifier = BKE_linestyle_thickness_modifier_add(linestyle, name, type);

  if (!modifier) {
    BKE_report(reports, RPT_ERROR, "Failed to add the thickness modifier");
    return NULL;
  }

  DEG_id_tag_update(&linestyle->id, 0);
  WM_main_add_notifier(NC_LINESTYLE, linestyle);

  return modifier;
}

static void rna_LineStyle_thickness_modifier_remove(FreestyleLineStyle *linestyle,
                                                    ReportList *reports,
                                                    PointerRNA *modifier_ptr)
{
  LineStyleModifier *modifier = modifier_ptr->data;

  if (BKE_linestyle_thickness_modifier_remove(linestyle, modifier) == -1) {
    BKE_reportf(
        reports, RPT_ERROR, "Thickness modifier '%s' could not be removed", modifier->name);
    return;
  }

  RNA_POINTER_INVALIDATE(modifier_ptr);

  DEG_id_tag_update(&linestyle->id, 0);
  WM_main_add_notifier(NC_LINESTYLE, linestyle);
}

static LineStyleModifier *rna_LineStyle_geometry_modifier_add(FreestyleLineStyle *linestyle,
                                                              ReportList *reports,
                                                              const char *name,
                                                              int type)
{
  LineStyleModifier *modifier = BKE_linestyle_geometry_modifier_add(linestyle, name, type);

  if (!modifier) {
    BKE_report(reports, RPT_ERROR, "Failed to add the geometry modifier");
    return NULL;
  }

  DEG_id_tag_update(&linestyle->id, 0);
  WM_main_add_notifier(NC_LINESTYLE, linestyle);

  return modifier;
}

static void rna_LineStyle_geometry_modifier_remove(FreestyleLineStyle *linestyle,
                                                   ReportList *reports,
                                                   PointerRNA *modifier_ptr)
{
  LineStyleModifier *modifier = modifier_ptr->data;

  if (BKE_linestyle_geometry_modifier_remove(linestyle, modifier) == -1) {
    BKE_reportf(reports, RPT_ERROR, "Geometry modifier '%s' could not be removed", modifier->name);
    return;
  }

  RNA_POINTER_INVALIDATE(modifier_ptr);

  DEG_id_tag_update(&linestyle->id, 0);
  WM_main_add_notifier(NC_LINESTYLE, linestyle);
}

#else

#  include "BLI_math.h"

static void rna_def_linestyle_mtex(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem texco_items[] = {
      {TEXCO_WINDOW, "WINDOW", 0, "Window", "Use screen coordinates as texture coordinates"},
      {TEXCO_GLOB, "GLOBAL", 0, "Global", "Use global coordinates for the texture coordinates"},
      {TEXCO_STROKE,
       "ALONG_STROKE",
       0,
       "Along stroke",
       "Use stroke length for texture coordinates"},
      {TEXCO_ORCO,
       "ORCO",
       0,
       "Generated",
       "Use the original undeformed coordinates of the object"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem prop_mapping_items[] = {
      {MTEX_FLAT, "FLAT", 0, "Flat", "Map X and Y coordinates directly"},
      {MTEX_CUBE, "CUBE", 0, "Cube", "Map using the normal vector"},
      {MTEX_TUBE, "TUBE", 0, "Tube", "Map with Z as central axis"},
      {MTEX_SPHERE, "SPHERE", 0, "Sphere", "Map with Z as central axis"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem prop_x_mapping_items[] = {
      {0, "NONE", 0, "None", ""},
      {1, "X", 0, "X", ""},
      {2, "Y", 0, "Y", ""},
      {3, "Z", 0, "Z", ""},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem prop_y_mapping_items[] = {
      {0, "NONE", 0, "None", ""},
      {1, "X", 0, "X", ""},
      {2, "Y", 0, "Y", ""},
      {3, "Z", 0, "Z", ""},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem prop_z_mapping_items[] = {
      {0, "NONE", 0, "None", ""},
      {1, "X", 0, "X", ""},
      {2, "Y", 0, "Y", ""},
      {3, "Z", 0, "Z", ""},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "LineStyleTextureSlot", "TextureSlot");
  RNA_def_struct_sdna(srna, "MTex");
  RNA_def_struct_ui_text(
      srna, "LineStyle Texture Slot", "Texture slot for textures in a LineStyle data-block");

  prop = RNA_def_property(srna, "mapping_x", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "projx");
  RNA_def_property_enum_items(prop, prop_x_mapping_items);
  RNA_def_property_ui_text(prop, "X Mapping", "");
  RNA_def_property_update(prop, 0, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "mapping_y", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "projy");
  RNA_def_property_enum_items(prop, prop_y_mapping_items);
  RNA_def_property_ui_text(prop, "Y Mapping", "");
  RNA_def_property_update(prop, 0, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "mapping_z", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "projz");
  RNA_def_property_enum_items(prop, prop_z_mapping_items);
  RNA_def_property_ui_text(prop, "Z Mapping", "");
  RNA_def_property_update(prop, 0, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "mapping", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_mapping_items);
  RNA_def_property_ui_text(prop, "Mapping", "");
  RNA_def_property_update(prop, 0, "rna_LineStyle_update");

  /* map to */
  prop = RNA_def_property(srna, "use_map_color_diffuse", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "mapto", MAP_COL);
  RNA_def_property_ui_text(prop, "Diffuse Color", "The texture affects basic color of the stroke");
  RNA_def_property_update(prop, 0, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "use_map_alpha", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "mapto", MAP_ALPHA);
  RNA_def_property_ui_text(prop, "Alpha", "The texture affects the alpha value");
  RNA_def_property_update(prop, 0, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "texture_coords", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "texco");
  RNA_def_property_enum_items(prop, texco_items);
  RNA_def_property_ui_text(prop,
                           "Texture Coordinates",
                           "Texture coordinates used to map the texture onto the background");
  RNA_def_property_update(prop, 0, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "alpha_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "alphafac");
  RNA_def_property_ui_range(prop, -1, 1, 10, 3);
  RNA_def_property_ui_text(prop, "Alpha Factor", "Amount texture affects alpha");
  RNA_def_property_update(prop, 0, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "diffuse_color_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "colfac");
  RNA_def_property_ui_range(prop, 0, 1, 10, 3);
  RNA_def_property_ui_text(prop, "Diffuse Color Factor", "Amount texture affects diffuse color");
  RNA_def_property_update(prop, 0, "rna_LineStyle_update");
}

static void rna_def_modifier_type_common(StructRNA *srna,
                                         const EnumPropertyItem *modifier_type_items,
                                         const char *set_name_func,
                                         const bool blend,
                                         const bool color)
{
  PropertyRNA *prop;

  /* TODO: Check this is not already defined somewhere else, e.g. in nodes... */
  static const EnumPropertyItem value_blend_items[] = {
      {LS_VALUE_BLEND, "MIX", 0, "Mix", ""},
      {LS_VALUE_ADD, "ADD", 0, "Add", ""},
      {LS_VALUE_SUB, "SUBTRACT", 0, "Subtract", ""},
      {LS_VALUE_MULT, "MULTIPLY", 0, "Multiply", ""},
      {LS_VALUE_DIV, "DIVIDE", 0, "Divide", ""},
      {LS_VALUE_DIFF, "DIFFERENCE", 0, "Difference", ""},
      {LS_VALUE_MIN, "MINIMUM", 0, "Minimum", ""},
      {LS_VALUE_MAX, "MAXIMUM", 0, "Maximum", ""},
      {0, NULL, 0, NULL, NULL},
  };

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "modifier.type");
  RNA_def_property_enum_items(prop, modifier_type_items);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Modifier Type", "Type of the modifier");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "modifier.name");
  RNA_def_property_string_funcs(prop, NULL, NULL, set_name_func);
  RNA_def_property_ui_text(prop, "Modifier Name", "Name of the modifier");
  RNA_def_property_update(prop, NC_LINESTYLE, NULL);
  RNA_def_struct_name_property(srna, prop);

  if (blend) {
    prop = RNA_def_property(srna, "blend", PROP_ENUM, PROP_NONE);
    RNA_def_property_enum_sdna(prop, NULL, "modifier.blend");
    RNA_def_property_enum_items(prop, (color) ? rna_enum_ramp_blend_items : value_blend_items);
    RNA_def_property_ui_text(
        prop, "Blend", "Specify how the modifier value is blended into the base value");
    RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

    prop = RNA_def_property(srna, "influence", PROP_FLOAT, PROP_FACTOR);
    RNA_def_property_float_sdna(prop, NULL, "modifier.influence");
    RNA_def_property_range(prop, 0.0f, 1.0f);
    RNA_def_property_ui_text(
        prop, "Influence", "Influence factor by which the modifier changes the property");
    RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");
  }

  prop = RNA_def_property(srna, "use", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "modifier.flags", LS_MODIFIER_ENABLED);
  RNA_def_property_ui_text(prop, "Use", "Enable or disable this modifier during stroke rendering");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "expanded", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "modifier.flags", LS_MODIFIER_EXPANDED);
  RNA_def_property_ui_text(prop, "Expanded", "True if the modifier tab is expanded");
}

static void rna_def_color_modifier(StructRNA *srna)
{
  rna_def_modifier_type_common(srna,
                               rna_enum_linestyle_color_modifier_type_items,
                               "rna_LineStyleColorModifier_name_set",
                               true,
                               true);
}

static void rna_def_alpha_modifier(StructRNA *srna)
{
  rna_def_modifier_type_common(srna,
                               rna_enum_linestyle_alpha_modifier_type_items,
                               "rna_LineStyleAlphaModifier_name_set",
                               true,
                               false);
}

static void rna_def_thickness_modifier(StructRNA *srna)
{
  rna_def_modifier_type_common(srna,
                               rna_enum_linestyle_thickness_modifier_type_items,
                               "rna_LineStyleThicknessModifier_name_set",
                               true,
                               false);
}

static void rna_def_geometry_modifier(StructRNA *srna)
{
  rna_def_modifier_type_common(srna,
                               rna_enum_linestyle_geometry_modifier_type_items,
                               "rna_LineStyleGeometryModifier_name_set",
                               false,
                               false);
}

static void rna_def_modifier_color_ramp_common(StructRNA *srna, int range)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "color_ramp", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "color_ramp");
  RNA_def_property_struct_type(prop, "ColorRamp");
  RNA_def_property_ui_text(prop, "Color Ramp", "Color ramp used to change line color");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  if (range) {
    prop = RNA_def_property(srna, "range_min", PROP_FLOAT, PROP_DISTANCE);
    RNA_def_property_float_sdna(prop, NULL, "range_min");
    RNA_def_property_ui_text(
        prop, "Range Min", "Lower bound of the input range the mapping is applied");
    RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

    prop = RNA_def_property(srna, "range_max", PROP_FLOAT, PROP_DISTANCE);
    RNA_def_property_float_sdna(prop, NULL, "range_max");
    RNA_def_property_ui_text(
        prop, "Range Max", "Upper bound of the input range the mapping is applied");
    RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");
  }
}

static void rna_def_modifier_curve_common(StructRNA *srna, bool range, bool value)
{
  PropertyRNA *prop;

  static const EnumPropertyItem mapping_items[] = {
      {0, "LINEAR", 0, "Linear", "Use linear mapping"},
      {LS_MODIFIER_USE_CURVE, "CURVE", 0, "Curve", "Use curve mapping"},
      {0, NULL, 0, NULL, NULL},
  };

  prop = RNA_def_property(srna, "mapping", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "flags");
  RNA_def_property_enum_items(prop, mapping_items);
  RNA_def_property_ui_text(prop, "Mapping", "Select the mapping type");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "invert", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", LS_MODIFIER_INVERT);
  RNA_def_property_ui_text(prop, "Invert", "Invert the fade-out direction of the linear mapping");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "curve");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Curve", "Curve used for the curve mapping");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  if (range) {
    prop = RNA_def_property(srna, "range_min", PROP_FLOAT, PROP_DISTANCE);
    RNA_def_property_float_sdna(prop, NULL, "range_min");
    RNA_def_property_ui_text(
        prop, "Range Min", "Lower bound of the input range the mapping is applied");
    RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

    prop = RNA_def_property(srna, "range_max", PROP_FLOAT, PROP_DISTANCE);
    RNA_def_property_float_sdna(prop, NULL, "range_max");
    RNA_def_property_ui_text(
        prop, "Range Max", "Upper bound of the input range the mapping is applied");
    RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");
  }

  if (value) {
    prop = RNA_def_property(srna, "value_min", PROP_FLOAT, PROP_NONE);
    RNA_def_property_float_sdna(prop, NULL, "value_min");
    RNA_def_property_ui_text(prop, "Value Min", "Minimum output value of the mapping");
    RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

    prop = RNA_def_property(srna, "value_max", PROP_FLOAT, PROP_NONE);
    RNA_def_property_float_sdna(prop, NULL, "value_max");
    RNA_def_property_ui_text(prop, "Value Max", "Maximum output value of the mapping");
    RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");
  }
}

static void rna_def_modifier_material_common(StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem mat_attr_items[] = {
      {LS_MODIFIER_MATERIAL_LINE, "LINE", 0, "Line Color", ""},
      {LS_MODIFIER_MATERIAL_LINE_R, "LINE_R", 0, "Line Color Red", ""},
      {LS_MODIFIER_MATERIAL_LINE_G, "LINE_G", 0, "Line Color Green", ""},
      {LS_MODIFIER_MATERIAL_LINE_B, "LINE_B", 0, "Line Color Blue", ""},
      {LS_MODIFIER_MATERIAL_LINE_A, "LINE_A", 0, "Line Color Alpha", ""},
      {LS_MODIFIER_MATERIAL_DIFF, "DIFF", 0, "Diffuse Color", ""},
      {LS_MODIFIER_MATERIAL_DIFF_R, "DIFF_R", 0, "Diffuse Color Red", ""},
      {LS_MODIFIER_MATERIAL_DIFF_G, "DIFF_G", 0, "Diffuse Color Green", ""},
      {LS_MODIFIER_MATERIAL_DIFF_B, "DIFF_B", 0, "Diffuse Color Blue", ""},
      {LS_MODIFIER_MATERIAL_SPEC, "SPEC", 0, "Specular Color", ""},
      {LS_MODIFIER_MATERIAL_SPEC_R, "SPEC_R", 0, "Specular Color Red", ""},
      {LS_MODIFIER_MATERIAL_SPEC_G, "SPEC_G", 0, "Specular Color Green", ""},
      {LS_MODIFIER_MATERIAL_SPEC_B, "SPEC_B", 0, "Specular Color Blue", ""},
      {LS_MODIFIER_MATERIAL_SPEC_HARD, "SPEC_HARD", 0, "Specular Hardness", ""},
      {LS_MODIFIER_MATERIAL_ALPHA, "ALPHA", 0, "Alpha Transparency", ""},
      {0, NULL, 0, NULL, NULL},
  };

  prop = RNA_def_property(srna, "material_attribute", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "mat_attr");
  RNA_def_property_enum_items(prop, mat_attr_items);
  RNA_def_property_ui_text(prop, "Material Attribute", "Specify which material attribute is used");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");
}

static void rna_def_linestyle_modifiers(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem blueprint_shape_items[] = {
      {LS_MODIFIER_BLUEPRINT_CIRCLES,
       "CIRCLES",
       0,
       "Circles",
       "Draw a blueprint using circular contour strokes"},
      {LS_MODIFIER_BLUEPRINT_ELLIPSES,
       "ELLIPSES",
       0,
       "Ellipses",
       "Draw a blueprint using elliptic contour strokes"},
      {LS_MODIFIER_BLUEPRINT_SQUARES,
       "SQUARES",
       0,
       "Squares",
       "Draw a blueprint using square contour strokes"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem transform_pivot_items[] = {
      {LS_MODIFIER_2D_TRANSFORM_PIVOT_CENTER, "CENTER", 0, "Stroke Center", ""},
      {LS_MODIFIER_2D_TRANSFORM_PIVOT_START, "START", 0, "Stroke Start", ""},
      {LS_MODIFIER_2D_TRANSFORM_PIVOT_END, "END", 0, "Stroke End", ""},
      {LS_MODIFIER_2D_TRANSFORM_PIVOT_PARAM, "PARAM", 0, "Stroke Point Parameter", ""},
      {LS_MODIFIER_2D_TRANSFORM_PIVOT_ABSOLUTE, "ABSOLUTE", 0, "Absolute 2D Point", ""},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "LineStyleModifier", NULL);
  RNA_def_struct_ui_text(srna, "Line Style Modifier", "Base type to define modifiers");

  /* line color modifiers */

  srna = RNA_def_struct(brna, "LineStyleColorModifier", "LineStyleModifier");
  RNA_def_struct_sdna(srna, "LineStyleModifier");
  RNA_def_struct_refine_func(srna, "rna_LineStyle_color_modifier_refine");
  RNA_def_struct_path_func(srna, "rna_LineStyle_color_modifier_path");
  RNA_def_struct_ui_text(
      srna, "Line Style Color Modifier", "Base type to define line color modifiers");

  srna = RNA_def_struct(brna, "LineStyleColorModifier_AlongStroke", "LineStyleColorModifier");
  RNA_def_struct_ui_text(srna, "Along Stroke", "Change line color along stroke");
  rna_def_color_modifier(srna);
  rna_def_modifier_color_ramp_common(srna, false);

  srna = RNA_def_struct(
      brna, "LineStyleColorModifier_DistanceFromCamera", "LineStyleColorModifier");
  RNA_def_struct_ui_text(
      srna, "Distance from Camera", "Change line color based on the distance from the camera");
  rna_def_color_modifier(srna);
  rna_def_modifier_color_ramp_common(srna, true);

  srna = RNA_def_struct(
      brna, "LineStyleColorModifier_DistanceFromObject", "LineStyleColorModifier");
  RNA_def_struct_ui_text(
      srna, "Distance from Object", "Change line color based on the distance from an object");
  rna_def_color_modifier(srna);
  rna_def_modifier_color_ramp_common(srna, true);

  prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "target");
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Target", "Target object from which the distance is measured");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  srna = RNA_def_struct(brna, "LineStyleColorModifier_Material", "LineStyleColorModifier");
  RNA_def_struct_ui_text(srna, "Material", "Change line color based on a material attribute");
  rna_def_color_modifier(srna);
  rna_def_modifier_material_common(srna);
  rna_def_modifier_color_ramp_common(srna, false);

  prop = RNA_def_property(srna, "use_ramp", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", LS_MODIFIER_USE_RAMP);
  RNA_def_property_ui_text(prop, "Ramp", "Use color ramp to map the BW average into an RGB color");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  srna = RNA_def_struct(brna, "LineStyleColorModifier_Tangent", "LineStyleColorModifier");
  RNA_def_struct_ui_text(srna, "Tangent", "Change line color based on the direction of a stroke");
  rna_def_color_modifier(srna);
  rna_def_modifier_color_ramp_common(srna, false);

  srna = RNA_def_struct(brna, "LineStyleColorModifier_Noise", "LineStyleColorModifier");
  RNA_def_struct_ui_text(srna, "Noise", "Change line color based on random noise");
  rna_def_color_modifier(srna);
  rna_def_modifier_color_ramp_common(srna, false);

  prop = RNA_def_property(srna, "amplitude", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "amplitude");
  RNA_def_property_ui_text(prop, "Amplitude", "Amplitude of the noise");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "period", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "period");
  RNA_def_property_ui_text(prop, "Period", "Period of the noise");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "seed", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "seed");
  RNA_def_property_ui_text(prop, "Seed", "Seed for the noise generation");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  srna = RNA_def_struct(brna, "LineStyleColorModifier_CreaseAngle", "LineStyleColorModifier");
  RNA_def_struct_ui_text(
      srna, "Crease Angle", "Change line color based on the underlying crease angle");
  rna_def_color_modifier(srna);
  rna_def_modifier_color_ramp_common(srna, false);

  prop = RNA_def_property(srna, "angle_min", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "min_angle");
  RNA_def_property_ui_text(prop, "Min Angle", "Minimum angle to modify thickness");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "angle_max", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "max_angle");
  RNA_def_property_ui_text(prop, "Max Angle", "Maximum angle to modify thickness");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  srna = RNA_def_struct(brna, "LineStyleColorModifier_Curvature_3D", "LineStyleColorModifier");
  RNA_def_struct_ui_text(
      srna, "Curvature 3D", "Change line color based on the radial curvature of 3D mesh surfaces");
  rna_def_color_modifier(srna);
  rna_def_modifier_color_ramp_common(srna, false);

  prop = RNA_def_property(srna, "curvature_min", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "min_curvature");
  RNA_def_property_ui_text(prop, "Min Curvature", "Minimum Curvature");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "curvature_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "max_curvature");
  RNA_def_property_ui_text(prop, "Max Curvature", "Maximum Curvature");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  /* alpha transparency modifiers */

  srna = RNA_def_struct(brna, "LineStyleAlphaModifier", "LineStyleModifier");
  RNA_def_struct_sdna(srna, "LineStyleModifier");
  RNA_def_struct_refine_func(srna, "rna_LineStyle_alpha_modifier_refine");
  RNA_def_struct_path_func(srna, "rna_LineStyle_alpha_modifier_path");
  RNA_def_struct_ui_text(
      srna, "Line Style Alpha Modifier", "Base type to define alpha transparency modifiers");

  srna = RNA_def_struct(brna, "LineStyleAlphaModifier_AlongStroke", "LineStyleAlphaModifier");
  RNA_def_struct_ui_text(srna, "Along Stroke", "Change alpha transparency along stroke");
  rna_def_alpha_modifier(srna);
  rna_def_modifier_curve_common(srna, false, false);

  srna = RNA_def_struct(
      brna, "LineStyleAlphaModifier_DistanceFromCamera", "LineStyleAlphaModifier");
  RNA_def_struct_ui_text(srna,
                         "Distance from Camera",
                         "Change alpha transparency based on the distance from the camera");
  rna_def_alpha_modifier(srna);
  rna_def_modifier_curve_common(srna, true, false);

  srna = RNA_def_struct(
      brna, "LineStyleAlphaModifier_DistanceFromObject", "LineStyleAlphaModifier");
  RNA_def_struct_ui_text(srna,
                         "Distance from Object",
                         "Change alpha transparency based on the distance from an object");
  rna_def_alpha_modifier(srna);
  rna_def_modifier_curve_common(srna, true, false);

  prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "target");
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Target", "Target object from which the distance is measured");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  srna = RNA_def_struct(brna, "LineStyleAlphaModifier_Material", "LineStyleAlphaModifier");
  RNA_def_struct_ui_text(
      srna, "Material", "Change alpha transparency based on a material attribute");
  rna_def_alpha_modifier(srna);
  rna_def_modifier_material_common(srna);
  rna_def_modifier_curve_common(srna, false, false);

  srna = RNA_def_struct(brna, "LineStyleAlphaModifier_Tangent", "LineStyleAlphaModifier");
  RNA_def_struct_ui_text(
      srna, "Tangent", "Alpha transparency based on the direction of the stroke");
  rna_def_alpha_modifier(srna);
  rna_def_modifier_curve_common(srna, false, false);

  srna = RNA_def_struct(brna, "LineStyleAlphaModifier_Noise", "LineStyleAlphaModifier");
  RNA_def_struct_ui_text(srna, "Noise", "Alpha transparency based on random noise");
  rna_def_alpha_modifier(srna);
  rna_def_modifier_curve_common(srna, false, false);

  prop = RNA_def_property(srna, "amplitude", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "amplitude");
  RNA_def_property_ui_text(prop, "Amplitude", "Amplitude of the noise");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "period", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "period");
  RNA_def_property_ui_text(prop, "Period", "Period of the noise");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "seed", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "seed");
  RNA_def_property_ui_text(prop, "Seed", "Seed for the noise generation");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  srna = RNA_def_struct(brna, "LineStyleAlphaModifier_CreaseAngle", "LineStyleAlphaModifier");
  RNA_def_struct_ui_text(
      srna, "Crease Angle", "Alpha transparency based on the angle between two adjacent faces");
  rna_def_alpha_modifier(srna);
  rna_def_modifier_curve_common(srna, false, false);

  prop = RNA_def_property(srna, "angle_min", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "min_angle");
  RNA_def_property_ui_text(prop, "Min Angle", "Minimum angle to modify thickness");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "angle_max", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "max_angle");
  RNA_def_property_ui_text(prop, "Max Angle", "Maximum angle to modify thickness");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  srna = RNA_def_struct(brna, "LineStyleAlphaModifier_Curvature_3D", "LineStyleAlphaModifier");
  RNA_def_struct_ui_text(srna,
                         "Curvature 3D",
                         "Alpha transparency based on the radial curvature of 3D mesh surfaces");
  rna_def_alpha_modifier(srna);
  rna_def_modifier_curve_common(srna, false, false);

  prop = RNA_def_property(srna, "curvature_min", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "min_curvature");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_ui_text(prop, "Min Curvature", "Minimum Curvature");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "curvature_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "max_curvature");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_ui_text(prop, "Max Curvature", "Maximum Curvature");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  /* line thickness modifiers */
  srna = RNA_def_struct(brna, "LineStyleThicknessModifier", "LineStyleModifier");
  RNA_def_struct_sdna(srna, "LineStyleModifier");
  RNA_def_struct_refine_func(srna, "rna_LineStyle_thickness_modifier_refine");
  RNA_def_struct_path_func(srna, "rna_LineStyle_thickness_modifier_path");
  RNA_def_struct_ui_text(
      srna, "Line Style Thickness Modifier", "Base type to define line thickness modifiers");

  srna = RNA_def_struct(brna, "LineStyleThicknessModifier_Tangent", "LineStyleThicknessModifier");
  RNA_def_struct_ui_text(srna, "Tangent", "Thickness based on the direction of the stroke");
  rna_def_thickness_modifier(srna);
  rna_def_modifier_curve_common(srna, false, false);

  prop = RNA_def_property(srna, "thickness_min", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "min_thickness");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_ui_text(prop, "Min Thickness", "Minimum thickness");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "thickness_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "max_thickness");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_ui_text(prop, "Max Thickness", "Maximum thickness");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  srna = RNA_def_struct(
      brna, "LineStyleThicknessModifier_AlongStroke", "LineStyleThicknessModifier");
  RNA_def_struct_ui_text(srna, "Along Stroke", "Change line thickness along stroke");
  rna_def_thickness_modifier(srna);
  rna_def_modifier_curve_common(srna, false, true);

  srna = RNA_def_struct(
      brna, "LineStyleThicknessModifier_DistanceFromCamera", "LineStyleThicknessModifier");
  RNA_def_struct_ui_text(
      srna, "Distance from Camera", "Change line thickness based on the distance from the camera");
  rna_def_thickness_modifier(srna);
  rna_def_modifier_curve_common(srna, true, true);

  srna = RNA_def_struct(
      brna, "LineStyleThicknessModifier_DistanceFromObject", "LineStyleThicknessModifier");
  RNA_def_struct_ui_text(
      srna, "Distance from Object", "Change line thickness based on the distance from an object");
  rna_def_thickness_modifier(srna);
  rna_def_modifier_curve_common(srna, true, true);

  prop = RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "target");
  RNA_def_property_struct_type(prop, "Object");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Target", "Target object from which the distance is measured");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  srna = RNA_def_struct(brna, "LineStyleThicknessModifier_Material", "LineStyleThicknessModifier");
  RNA_def_struct_ui_text(srna, "Material", "Change line thickness based on a material attribute");
  rna_def_thickness_modifier(srna);
  rna_def_modifier_material_common(srna);
  rna_def_modifier_curve_common(srna, false, true);

  srna = RNA_def_struct(
      brna, "LineStyleThicknessModifier_Calligraphy", "LineStyleThicknessModifier");
  RNA_def_struct_ui_text(
      srna,
      "Calligraphy",
      "Change line thickness so that stroke looks like made with a calligraphic pen");
  rna_def_thickness_modifier(srna);

  prop = RNA_def_property(srna, "orientation", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "orientation");
  RNA_def_property_ui_text(prop, "Orientation", "Angle of the main direction");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "thickness_min", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "min_thickness");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_ui_text(
      prop,
      "Min Thickness",
      "Minimum thickness in the direction perpendicular to the main direction");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "thickness_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "max_thickness");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_ui_text(prop, "Max Thickness", "Maximum thickness in the main direction");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  srna = RNA_def_struct(brna, "LineStyleThicknessModifier_Noise", "LineStyleThicknessModifier");
  RNA_def_struct_ui_text(srna, "Noise", "Line thickness based on random noise");
  rna_def_thickness_modifier(srna);

  prop = RNA_def_property(srna, "amplitude", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "amplitude");
  RNA_def_property_ui_text(prop, "Amplitude", "Amplitude of the noise");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "period", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "period");
  RNA_def_property_ui_text(prop, "Period", "Period of the noise");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "seed", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "seed");
  RNA_def_property_ui_text(prop, "Seed", "Seed for the noise generation");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "use_asymmetric", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", LS_THICKNESS_ASYMMETRIC);
  RNA_def_property_ui_text(prop, "Asymmetric", "Allow thickness to be assigned asymmetrically");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  srna = RNA_def_struct(
      brna, "LineStyleThicknessModifier_Curvature_3D", "LineStyleThicknessModifier");
  RNA_def_struct_ui_text(
      srna, "Curvature 3D", "Line thickness based on the radial curvature of 3D mesh surfaces");
  rna_def_thickness_modifier(srna);
  rna_def_modifier_curve_common(srna, false, false);

  prop = RNA_def_property(srna, "thickness_min", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "min_thickness");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_ui_text(prop, "Min Thickness", "Minimum thickness");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "thickness_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "max_thickness");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_ui_text(prop, "Max Thickness", "Maximum thickness");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "curvature_min", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "min_curvature");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_ui_text(prop, "Min Curvature", "Minimum Curvature");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "curvature_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "max_curvature");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_ui_text(prop, "Max Curvature", "Maximum Curvature");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  srna = RNA_def_struct(
      brna, "LineStyleThicknessModifier_CreaseAngle", "LineStyleThicknessModifier");
  RNA_def_struct_ui_text(
      srna, "Crease Angle", "Line thickness based on the angle between two adjacent faces");
  rna_def_thickness_modifier(srna);
  rna_def_modifier_curve_common(srna, false, false);

  prop = RNA_def_property(srna, "angle_min", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "min_angle");
  RNA_def_property_ui_text(prop, "Min Angle", "Minimum angle to modify thickness");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "angle_max", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "max_angle");
  RNA_def_property_ui_text(prop, "Max Angle", "Maximum angle to modify thickness");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "thickness_min", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "min_thickness");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_ui_text(prop, "Min Thickness", "Minimum thickness");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "thickness_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "max_thickness");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_ui_text(prop, "Max Thickness", "Maximum thickness");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  /* geometry modifiers */

  srna = RNA_def_struct(brna, "LineStyleGeometryModifier", "LineStyleModifier");
  RNA_def_struct_sdna(srna, "LineStyleModifier");
  RNA_def_struct_refine_func(srna, "rna_LineStyle_geometry_modifier_refine");
  RNA_def_struct_path_func(srna, "rna_LineStyle_geometry_modifier_path");
  RNA_def_struct_ui_text(
      srna, "Line Style Geometry Modifier", "Base type to define stroke geometry modifiers");

  srna = RNA_def_struct(brna, "LineStyleGeometryModifier_Sampling", "LineStyleGeometryModifier");
  RNA_def_struct_ui_text(
      srna,
      "Sampling",
      "Specify a new sampling value that determines the resolution of stroke polylines");
  rna_def_geometry_modifier(srna);

  prop = RNA_def_property(srna, "sampling", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "sampling");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_ui_text(
      prop, "Sampling", "New sampling value to be used for subsequent modifiers");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  srna = RNA_def_struct(
      brna, "LineStyleGeometryModifier_BezierCurve", "LineStyleGeometryModifier");
  RNA_def_struct_ui_text(srna,
                         "Bezier Curve",
                         "Replace stroke backbone geometry by a Bezier curve approximation of the "
                         "original backbone geometry");
  rna_def_geometry_modifier(srna);

  prop = RNA_def_property(srna, "error", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "error");
  RNA_def_property_ui_text(prop,
                           "Error",
                           "Maximum distance allowed between the new Bezier curve and the "
                           "original backbone geometry");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  srna = RNA_def_struct(
      brna, "LineStyleGeometryModifier_SinusDisplacement", "LineStyleGeometryModifier");
  RNA_def_struct_ui_text(
      srna, "Sinus Displacement", "Add sinus displacement to stroke backbone geometry");
  rna_def_geometry_modifier(srna);

  prop = RNA_def_property(srna, "wavelength", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "wavelength");
  RNA_def_property_ui_text(prop, "Wavelength", "Wavelength of the sinus displacement");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "amplitude", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "amplitude");
  RNA_def_property_ui_text(prop, "Amplitude", "Amplitude of the sinus displacement");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "phase", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "phase");
  RNA_def_property_ui_text(prop, "Phase", "Phase of the sinus displacement");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  srna = RNA_def_struct(
      brna, "LineStyleGeometryModifier_SpatialNoise", "LineStyleGeometryModifier");
  RNA_def_struct_ui_text(srna, "Spatial Noise", "Add spatial noise to stroke backbone geometry");
  rna_def_geometry_modifier(srna);

  prop = RNA_def_property(srna, "amplitude", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "amplitude");
  RNA_def_property_ui_text(prop, "Amplitude", "Amplitude of the spatial noise");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "scale");
  RNA_def_property_ui_text(prop, "Scale", "Scale of the spatial noise");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "octaves", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "octaves");
  RNA_def_property_ui_text(
      prop, "Octaves", "Number of octaves (i.e., the amount of detail of the spatial noise)");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "smooth", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", LS_MODIFIER_SPATIAL_NOISE_SMOOTH);
  RNA_def_property_ui_text(prop, "Smooth", "If true, the spatial noise is smooth");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "use_pure_random", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", LS_MODIFIER_SPATIAL_NOISE_PURERANDOM);
  RNA_def_property_ui_text(
      prop, "Pure Random", "If true, the spatial noise does not show any coherence");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  srna = RNA_def_struct(
      brna, "LineStyleGeometryModifier_PerlinNoise1D", "LineStyleGeometryModifier");
  RNA_def_struct_ui_text(
      srna, "Perlin Noise 1D", "Add one-dimensional Perlin noise to stroke backbone geometry");
  rna_def_geometry_modifier(srna);

  prop = RNA_def_property(srna, "frequency", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "frequency");
  RNA_def_property_ui_text(prop, "Frequency", "Frequency of the Perlin noise");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "amplitude", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "amplitude");
  RNA_def_property_ui_text(prop, "Amplitude", "Amplitude of the Perlin noise");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "octaves", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "octaves");
  RNA_def_property_ui_text(
      prop, "Octaves", "Number of octaves (i.e., the amount of detail of the Perlin noise)");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "angle");
  RNA_def_property_ui_text(prop, "Angle", "Displacement direction");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "seed", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "seed");
  RNA_def_property_ui_text(
      prop,
      "Seed",
      "Seed for random number generation (if negative, time is used as a seed instead)");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  srna = RNA_def_struct(
      brna, "LineStyleGeometryModifier_PerlinNoise2D", "LineStyleGeometryModifier");
  RNA_def_struct_ui_text(
      srna, "Perlin Noise 2D", "Add two-dimensional Perlin noise to stroke backbone geometry");
  rna_def_geometry_modifier(srna);

  prop = RNA_def_property(srna, "frequency", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "frequency");
  RNA_def_property_ui_text(prop, "Frequency", "Frequency of the Perlin noise");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "amplitude", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "amplitude");
  RNA_def_property_ui_text(prop, "Amplitude", "Amplitude of the Perlin noise");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "octaves", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "octaves");
  RNA_def_property_ui_text(
      prop, "Octaves", "Number of octaves (i.e., the amount of detail of the Perlin noise)");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "angle");
  RNA_def_property_ui_text(prop, "Angle", "Displacement direction");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "seed", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "seed");
  RNA_def_property_ui_text(
      prop,
      "Seed",
      "Seed for random number generation (if negative, time is used as a seed instead)");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  srna = RNA_def_struct(
      brna, "LineStyleGeometryModifier_BackboneStretcher", "LineStyleGeometryModifier");
  RNA_def_struct_ui_text(
      srna, "Backbone Stretcher", "Stretch the beginning and the end of stroke backbone");
  rna_def_geometry_modifier(srna);

  prop = RNA_def_property(srna, "backbone_length", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "backbone_length");
  RNA_def_property_ui_text(prop, "Backbone Length", "Amount of backbone stretching");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  srna = RNA_def_struct(brna, "LineStyleGeometryModifier_TipRemover", "LineStyleGeometryModifier");
  RNA_def_struct_ui_text(
      srna,
      "Tip Remover",
      "Remove a piece of stroke at the beginning and the end of stroke backbone");
  rna_def_geometry_modifier(srna);

  prop = RNA_def_property(srna, "tip_length", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "tip_length");
  RNA_def_property_ui_text(prop, "Tip Length", "Length of tips to be removed");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  srna = RNA_def_struct(
      brna, "LineStyleGeometryModifier_Polygonalization", "LineStyleGeometryModifier");
  RNA_def_struct_ui_text(
      srna, "Polygonalization", "Modify the stroke geometry so that it looks more 'polygonal'");
  rna_def_geometry_modifier(srna);

  prop = RNA_def_property(srna, "error", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "error");
  RNA_def_property_ui_text(
      prop,
      "Error",
      "Maximum distance between the original stroke and its polygonal approximation");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  srna = RNA_def_struct(
      brna, "LineStyleGeometryModifier_GuidingLines", "LineStyleGeometryModifier");
  RNA_def_struct_ui_text(
      srna,
      "Guiding Lines",
      "Modify the stroke geometry so that it corresponds to its main direction line");
  rna_def_geometry_modifier(srna);

  prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "offset");
  RNA_def_property_ui_text(
      prop, "Offset", "Displacement that is applied to the main direction line along its normal");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  srna = RNA_def_struct(brna, "LineStyleGeometryModifier_Blueprint", "LineStyleGeometryModifier");
  RNA_def_struct_ui_text(
      srna,
      "Blueprint",
      "Produce a blueprint using circular, elliptic, and square contour strokes");
  rna_def_geometry_modifier(srna);

  prop = RNA_def_property(srna, "shape", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "flags");
  RNA_def_property_enum_items(prop, blueprint_shape_items);
  RNA_def_property_ui_text(prop, "Shape", "Select the shape of blueprint contour strokes");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "rounds", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "rounds");
  RNA_def_property_range(prop, 1, 1000);
  RNA_def_property_ui_text(prop, "Rounds", "Number of rounds in contour strokes");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "backbone_length", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "backbone_length");
  RNA_def_property_ui_text(prop, "Backbone Length", "Amount of backbone stretching");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "random_radius", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "random_radius");
  RNA_def_property_ui_text(prop, "Random Radius", "Randomness of the radius");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "random_center", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "random_center");
  RNA_def_property_ui_text(prop, "Random Center", "Randomness of the center");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "random_backbone", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "random_backbone");
  RNA_def_property_ui_text(prop, "Random Backbone", "Randomness of the backbone stretching");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  srna = RNA_def_struct(brna, "LineStyleGeometryModifier_2DOffset", "LineStyleGeometryModifier");
  RNA_def_struct_ui_text(
      srna, "2D Offset", "Add two-dimensional offsets to stroke backbone geometry");
  rna_def_geometry_modifier(srna);

  prop = RNA_def_property(srna, "start", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "start");
  RNA_def_property_ui_text(
      prop, "Start", "Displacement that is applied from the beginning of the stroke");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "end", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "end");
  RNA_def_property_ui_text(prop, "End", "Displacement that is applied from the end of the stroke");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "x");
  RNA_def_property_ui_text(
      prop, "X", "Displacement that is applied to the X coordinates of stroke vertices");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "y");
  RNA_def_property_ui_text(
      prop, "Y", "Displacement that is applied to the Y coordinates of stroke vertices");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  srna = RNA_def_struct(
      brna, "LineStyleGeometryModifier_2DTransform", "LineStyleGeometryModifier");
  RNA_def_struct_ui_text(srna,
                         "2D Transform",
                         "Apply two-dimensional scaling and rotation to stroke backbone geometry");
  rna_def_geometry_modifier(srna);

  prop = RNA_def_property(srna, "pivot", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "pivot");
  RNA_def_property_enum_items(prop, transform_pivot_items);
  RNA_def_property_ui_text(prop, "Pivot", "Pivot of scaling and rotation operations");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "scale_x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "scale_x");
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_ui_text(prop, "Scale X", "Scaling factor that is applied along the X axis");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "scale_y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "scale_y");
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_ui_text(prop, "Scale Y", "Scaling factor that is applied along the Y axis");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "angle");
  RNA_def_property_ui_text(prop, "Rotation Angle", "Rotation angle");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "pivot_u", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "pivot_u");
  RNA_def_property_range(prop, 0.f, 1.f);
  RNA_def_property_ui_text(prop,
                           "Stroke Point Parameter",
                           "Pivot in terms of the stroke point parameter u (0 <= u <= 1)");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "pivot_x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "pivot_x");
  RNA_def_property_ui_text(prop, "Pivot X", "2D X coordinate of the absolute pivot");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "pivot_y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "pivot_y");
  RNA_def_property_ui_text(prop, "Pivot Y", "2D Y coordinate of the absolute pivot");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  srna = RNA_def_struct(
      brna, "LineStyleGeometryModifier_Simplification", "LineStyleGeometryModifier");
  RNA_def_struct_ui_text(srna, "Simplification", "Simplify the stroke set");
  rna_def_geometry_modifier(srna);

  prop = RNA_def_property(srna, "tolerance", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "tolerance");
  RNA_def_property_ui_text(prop, "Tolerance", "Distance below which segments will be merged");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");
}

static void rna_def_freestyle_color_modifiers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "LineStyleColorModifiers");
  srna = RNA_def_struct(brna, "LineStyleColorModifiers", NULL);
  RNA_def_struct_sdna(srna, "FreestyleLineStyle");
  RNA_def_struct_ui_text(srna, "Color Modifiers", "Color modifiers for changing line colors");

  func = RNA_def_function(srna, "new", "rna_LineStyle_color_modifier_add");
  RNA_def_function_ui_description(func, "Add a color modifier to line style");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_string(
      func, "name", "ColorModifier", 0, "", "New name for the color modifier (not unique)");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_enum(func,
                      "type",
                      rna_enum_linestyle_color_modifier_type_items,
                      0,
                      "",
                      "Color modifier type to add");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_pointer(
      func, "modifier", "LineStyleColorModifier", "", "Newly added color modifier");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_LineStyle_color_modifier_remove");
  RNA_def_function_ui_description(func, "Remove a color modifier from line style");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(
      func, "modifier", "LineStyleColorModifier", "", "Color modifier to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);
}

static void rna_def_freestyle_alpha_modifiers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "LineStyleAlphaModifiers");
  srna = RNA_def_struct(brna, "LineStyleAlphaModifiers", NULL);
  RNA_def_struct_sdna(srna, "FreestyleLineStyle");
  RNA_def_struct_ui_text(srna, "Alpha Modifiers", "Alpha modifiers for changing line alphas");

  func = RNA_def_function(srna, "new", "rna_LineStyle_alpha_modifier_add");
  RNA_def_function_ui_description(func, "Add a alpha modifier to line style");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_string(
      func, "name", "AlphaModifier", 0, "", "New name for the alpha modifier (not unique)");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_enum(func,
                      "type",
                      rna_enum_linestyle_alpha_modifier_type_items,
                      0,
                      "",
                      "Alpha modifier type to add");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_pointer(
      func, "modifier", "LineStyleAlphaModifier", "", "Newly added alpha modifier");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_LineStyle_alpha_modifier_remove");
  RNA_def_function_ui_description(func, "Remove a alpha modifier from line style");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(
      func, "modifier", "LineStyleAlphaModifier", "", "Alpha modifier to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);
}

static void rna_def_freestyle_thickness_modifiers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "LineStyleThicknessModifiers");
  srna = RNA_def_struct(brna, "LineStyleThicknessModifiers", NULL);
  RNA_def_struct_sdna(srna, "FreestyleLineStyle");
  RNA_def_struct_ui_text(
      srna, "Thickness Modifiers", "Thickness modifiers for changing line thickness");

  func = RNA_def_function(srna, "new", "rna_LineStyle_thickness_modifier_add");
  RNA_def_function_ui_description(func, "Add a thickness modifier to line style");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_string(func,
                        "name",
                        "ThicknessModifier",
                        0,
                        "",
                        "New name for the thickness modifier (not unique)");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_enum(func,
                      "type",
                      rna_enum_linestyle_thickness_modifier_type_items,
                      0,
                      "",
                      "Thickness modifier type to add");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_pointer(
      func, "modifier", "LineStyleThicknessModifier", "", "Newly added thickness modifier");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_LineStyle_thickness_modifier_remove");
  RNA_def_function_ui_description(func, "Remove a thickness modifier from line style");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(
      func, "modifier", "LineStyleThicknessModifier", "", "Thickness modifier to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);
}

static void rna_def_freestyle_geometry_modifiers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "LineStyleGeometryModifiers");
  srna = RNA_def_struct(brna, "LineStyleGeometryModifiers", NULL);
  RNA_def_struct_sdna(srna, "FreestyleLineStyle");
  RNA_def_struct_ui_text(
      srna, "Geometry Modifiers", "Geometry modifiers for changing line geometries");

  func = RNA_def_function(srna, "new", "rna_LineStyle_geometry_modifier_add");
  RNA_def_function_ui_description(func, "Add a geometry modifier to line style");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_string(
      func, "name", "GeometryModifier", 0, "", "New name for the geometry modifier (not unique)");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_enum(func,
                      "type",
                      rna_enum_linestyle_geometry_modifier_type_items,
                      0,
                      "",
                      "Geometry modifier type to add");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_pointer(
      func, "modifier", "LineStyleGeometryModifier", "", "Newly added geometry modifier");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_LineStyle_geometry_modifier_remove");
  RNA_def_function_ui_description(func, "Remove a geometry modifier from line style");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(
      func, "modifier", "LineStyleGeometryModifier", "", "Geometry modifier to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);
}

static void rna_def_linestyle(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem panel_items[] = {
    {LS_PANEL_STROKES, "STROKES", 0, "Strokes", "Show the panel for stroke construction"},
    {LS_PANEL_COLOR, "COLOR", 0, "Color", "Show the panel for line color options"},
    {LS_PANEL_ALPHA, "ALPHA", 0, "Alpha", "Show the panel for alpha transparency options"},
    {LS_PANEL_THICKNESS, "THICKNESS", 0, "Thickness", "Show the panel for line thickness options"},
    {LS_PANEL_GEOMETRY, "GEOMETRY", 0, "Geometry", "Show the panel for stroke geometry options"},
    {LS_PANEL_TEXTURE, "TEXTURE", 0, "Texture", "Show the panel for stroke texture options"},
#  if 0 /* hidden for now */
    {LS_PANEL_MISC, "MISC", 0, "Misc", "Show the panel for miscellaneous options"},
#  endif
    {0, NULL, 0, NULL, NULL},
  };
  static const EnumPropertyItem chaining_items[] = {
      {LS_CHAINING_PLAIN, "PLAIN", 0, "Plain", "Plain chaining"},
      {LS_CHAINING_SKETCHY, "SKETCHY", 0, "Sketchy", "Sketchy chaining with a multiple touch"},
      {0, NULL, 0, NULL, NULL},
  };
  static const EnumPropertyItem cap_items[] = {
      {LS_CAPS_BUTT, "BUTT", 0, "Butt", "Butt cap (flat)"},
      {LS_CAPS_ROUND, "ROUND", 0, "Round", "Round cap (half-circle)"},
      {LS_CAPS_SQUARE, "SQUARE", 0, "Square", "Square cap (flat and extended)"},
      {0, NULL, 0, NULL, NULL},
  };
  static const EnumPropertyItem thickness_position_items[] = {
      {LS_THICKNESS_CENTER,
       "CENTER",
       0,
       "Center",
       "Silhouettes and border edges are centered along stroke geometry"},
      {LS_THICKNESS_INSIDE,
       "INSIDE",
       0,
       "Inside",
       "Silhouettes and border edges are drawn inside of stroke geometry"},
      {LS_THICKNESS_OUTSIDE,
       "OUTSIDE",
       0,
       "Outside",
       "Silhouettes and border edges are drawn outside of stroke geometry"},
      {LS_THICKNESS_RELATIVE,
       "RELATIVE",
       0,
       "Relative",
       "Silhouettes and border edges are shifted by a user-defined ratio"},
      {0, NULL, 0, NULL, NULL},
  };
  static const EnumPropertyItem sort_key_items[] = {
      {LS_SORT_KEY_DISTANCE_FROM_CAMERA,
       "DISTANCE_FROM_CAMERA",
       0,
       "Distance from Camera",
       "Sort by distance from camera (closer lines lie on top of further lines)"},
      {LS_SORT_KEY_2D_LENGTH,
       "2D_LENGTH",
       0,
       "2D Length",
       "Sort by curvilinear 2D length (longer lines lie on top of shorter lines)"},
      {LS_SORT_KEY_PROJECTED_X,
       "PROJECTED_X",
       0,
       "Projected X",
       "Sort by the projected X value in the image coordinate system"},
      {LS_SORT_KEY_PROJECTED_Y,
       "PROJECTED_Y",
       0,
       "Projected Y",
       "Sort by the projected Y value in the image coordinate system"},
      {0, NULL, 0, NULL, NULL},
  };
  static const EnumPropertyItem sort_order_items[] = {
      {0, "DEFAULT", 0, "Default", "Default order of the sort key"},
      {LS_REVERSE_ORDER, "REVERSE", 0, "Reverse", "Reverse order"},
      {0, NULL, 0, NULL, NULL},
  };
  static const EnumPropertyItem integration_type_items[] = {
      {LS_INTEGRATION_MEAN,
       "MEAN",
       0,
       "Mean",
       "The value computed for the chain is the mean of the values obtained for chain vertices"},
      {LS_INTEGRATION_MIN,
       "MIN",
       0,
       "Min",
       "The value computed for the chain is the minimum of the values obtained for chain "
       "vertices"},
      {LS_INTEGRATION_MAX,
       "MAX",
       0,
       "Max",
       "The value computed for the chain is the maximum of the values obtained for chain "
       "vertices"},
      {LS_INTEGRATION_FIRST,
       "FIRST",
       0,
       "First",
       "The value computed for the chain is the value obtained for the first chain vertex"},
      {LS_INTEGRATION_LAST,
       "LAST",
       0,
       "Last",
       "The value computed for the chain is the value obtained for the last chain vertex"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "FreestyleLineStyle", "ID");
  RNA_def_struct_ui_text(
      srna, "Freestyle Line Style", "Freestyle line style, reusable by multiple line sets");
  RNA_def_struct_ui_icon(srna, ICON_LINE_DATA);

  rna_def_mtex_common(brna,
                      srna,
                      "rna_LineStyle_mtex_begin",
                      "rna_LineStyle_active_texture_get",
                      "rna_LineStyle_active_texture_set",
                      NULL,
                      "LineStyleTextureSlot",
                      "LineStyleTextureSlots",
                      "rna_LineStyle_update",
                      "rna_LineStyle_update");

  prop = RNA_def_property(srna, "panel", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "panel");
  RNA_def_property_enum_items(prop, panel_items);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Panel", "Select the property panel to be shown");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, NULL, "r");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(
      prop, "Color", "Base line color, possibly modified by line color modifiers");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "alpha", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "alpha");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop,
      "Alpha Transparency",
      "Base alpha transparency, possibly modified by alpha transparency modifiers");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "thickness", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "thickness");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_ui_text(
      prop, "Thickness", "Base line thickness, possibly modified by line thickness modifiers");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "thickness_position", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "thickness_position");
  RNA_def_property_enum_items(prop, thickness_position_items);
  RNA_def_property_ui_text(prop,
                           "Thickness Position",
                           "Thickness position of silhouettes and border edges (applicable when "
                           "plain chaining is used with the Same Object option)");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "thickness_ratio", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "thickness_ratio");
  RNA_def_property_range(prop, 0.f, 1.f);
  RNA_def_property_ui_text(
      prop,
      "Thickness Ratio",
      "A number between 0 (inside) and 1 (outside) specifying the relative position of "
      "stroke thickness");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "color_modifiers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "color_modifiers", NULL);
  RNA_def_property_struct_type(prop, "LineStyleColorModifier");
  RNA_def_property_ui_text(prop, "Color Modifiers", "List of line color modifiers");
  rna_def_freestyle_color_modifiers(brna, prop);

  prop = RNA_def_property(srna, "alpha_modifiers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "alpha_modifiers", NULL);
  RNA_def_property_struct_type(prop, "LineStyleAlphaModifier");
  RNA_def_property_ui_text(prop, "Alpha Modifiers", "List of alpha transparency modifiers");
  rna_def_freestyle_alpha_modifiers(brna, prop);

  prop = RNA_def_property(srna, "thickness_modifiers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "thickness_modifiers", NULL);
  RNA_def_property_struct_type(prop, "LineStyleThicknessModifier");
  RNA_def_property_ui_text(prop, "Thickness Modifiers", "List of line thickness modifiers");
  rna_def_freestyle_thickness_modifiers(brna, prop);

  prop = RNA_def_property(srna, "geometry_modifiers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "geometry_modifiers", NULL);
  RNA_def_property_struct_type(prop, "LineStyleGeometryModifier");
  RNA_def_property_ui_text(prop, "Geometry Modifiers", "List of stroke geometry modifiers");
  rna_def_freestyle_geometry_modifiers(brna, prop);

  prop = RNA_def_property(srna, "use_chaining", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", LS_NO_CHAINING);
  RNA_def_property_ui_text(prop, "Chaining", "Enable chaining of feature edges");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "chaining", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "chaining");
  RNA_def_property_enum_items(prop, chaining_items);
  RNA_def_property_ui_text(
      prop, "Chaining Method", "Select the way how feature edges are jointed to form chains");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "rounds", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "rounds");
  RNA_def_property_range(prop, 1, 1000);
  RNA_def_property_ui_text(prop, "Rounds", "Number of rounds in a sketchy multiple touch");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "use_same_object", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", LS_SAME_OBJECT);
  RNA_def_property_ui_text(
      prop, "Same Object", "If true, only feature edges of the same object are joined");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "use_split_length", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", LS_SPLIT_LENGTH);
  RNA_def_property_ui_text(
      prop, "Use Split Length", "Enable chain splitting by curvilinear 2D length");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "split_length", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "split_length");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_ui_text(prop, "Split Length", "Curvilinear 2D length for chain splitting");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "use_angle_min", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", LS_MIN_2D_ANGLE);
  RNA_def_property_ui_text(prop,
                           "Use Min 2D Angle",
                           "Split chains at points with angles smaller than the minimum 2D angle");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "angle_min", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "min_angle");
  RNA_def_property_range(prop, 0.0f, DEG2RADF(180.0f));
  RNA_def_property_ui_text(prop, "Min 2D Angle", "Minimum 2D angle for splitting chains");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "use_angle_max", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", LS_MAX_2D_ANGLE);
  RNA_def_property_ui_text(prop,
                           "Use Max 2D Angle",
                           "Split chains at points with angles larger than the maximum 2D angle");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "angle_max", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "max_angle");
  RNA_def_property_range(prop, 0.0f, DEG2RADF(180.0f));
  RNA_def_property_ui_text(prop, "Max 2D Angle", "Maximum 2D angle for splitting chains");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "use_length_min", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", LS_MIN_2D_LENGTH);
  RNA_def_property_ui_text(
      prop, "Use Min 2D Length", "Enable the selection of chains by a minimum 2D length");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "length_min", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "min_length");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_ui_text(
      prop, "Min 2D Length", "Minimum curvilinear 2D length for the selection of chains");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "use_length_max", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", LS_MAX_2D_LENGTH);
  RNA_def_property_ui_text(
      prop, "Use Max 2D Length", "Enable the selection of chains by a maximum 2D length");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "length_max", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "max_length");
  RNA_def_property_range(prop, 0.0f, 10000.0f);
  RNA_def_property_ui_text(
      prop, "Max 2D Length", "Maximum curvilinear 2D length for the selection of chains");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "use_chain_count", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", LS_CHAIN_COUNT);
  RNA_def_property_ui_text(prop, "Use Chain Count", "Enable the selection of first N chains");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "chain_count", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "chain_count");
  RNA_def_property_ui_text(prop, "Chain Count", "Chain count for the selection of first N chains");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "use_split_pattern", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", LS_SPLIT_PATTERN);
  RNA_def_property_ui_text(
      prop, "Use Split Pattern", "Enable chain splitting by dashed line patterns");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "split_dash1", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "split_dash1");
  RNA_def_property_range(prop, 0, USHRT_MAX);
  RNA_def_property_ui_text(prop, "Split Dash 1", "Length of the 1st dash for splitting");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "split_gap1", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "split_gap1");
  RNA_def_property_range(prop, 0, USHRT_MAX);
  RNA_def_property_ui_text(prop, "Split Gap 1", "Length of the 1st gap for splitting");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "split_dash2", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "split_dash2");
  RNA_def_property_range(prop, 0, USHRT_MAX);
  RNA_def_property_ui_text(prop, "Split Dash 2", "Length of the 2nd dash for splitting");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "split_gap2", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "split_gap2");
  RNA_def_property_range(prop, 0, USHRT_MAX);
  RNA_def_property_ui_text(prop, "Split Gap 2", "Length of the 2nd gap for splitting");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "split_dash3", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "split_dash3");
  RNA_def_property_range(prop, 0, USHRT_MAX);
  RNA_def_property_ui_text(prop, "Split Dash 3", "Length of the 3rd dash for splitting");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "split_gap3", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "split_gap3");
  RNA_def_property_range(prop, 0, USHRT_MAX);
  RNA_def_property_ui_text(prop, "Split Gap 3", "Length of the 3rd gap for splitting");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "material_boundary", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", LS_MATERIAL_BOUNDARY);
  RNA_def_property_ui_text(prop,
                           "Material Boundary",
                           "If true, chains of feature edges are split at material boundaries");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "use_sorting", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", LS_NO_SORTING);
  RNA_def_property_ui_text(prop, "Sorting", "Arrange the stacking order of strokes");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "sort_key", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "sort_key");
  RNA_def_property_enum_items(prop, sort_key_items);
  RNA_def_property_ui_text(
      prop, "Sort Key", "Select the sort key to determine the stacking order of chains");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "sort_order", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
  RNA_def_property_enum_items(prop, sort_order_items);
  RNA_def_property_ui_text(prop, "Sort Order", "Select the sort order");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "integration_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "integration_type");
  RNA_def_property_enum_items(prop, integration_type_items);
  RNA_def_property_ui_text(
      prop, "Integration Type", "Select the way how the sort key is computed for each chain");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "use_dashed_line", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", LS_DASHED_LINE);
  RNA_def_property_ui_text(prop, "Dashed Line", "Enable or disable dashed line");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "caps", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "caps");
  RNA_def_property_enum_items(prop, cap_items);
  RNA_def_property_ui_text(prop, "Caps", "Select the shape of both ends of strokes");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "dash1", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "dash1");
  RNA_def_property_range(prop, 0, USHRT_MAX);
  RNA_def_property_ui_text(prop, "Dash 1", "Length of the 1st dash for dashed lines");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "gap1", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "gap1");
  RNA_def_property_range(prop, 0, USHRT_MAX);
  RNA_def_property_ui_text(prop, "Gap 1", "Length of the 1st gap for dashed lines");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "dash2", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "dash2");
  RNA_def_property_range(prop, 0, USHRT_MAX);
  RNA_def_property_ui_text(prop, "Dash 2", "Length of the 2nd dash for dashed lines");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "gap2", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "gap2");
  RNA_def_property_range(prop, 0, USHRT_MAX);
  RNA_def_property_ui_text(prop, "Gap 2", "Length of the 2nd gap for dashed lines");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "dash3", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "dash3");
  RNA_def_property_range(prop, 0, USHRT_MAX);
  RNA_def_property_ui_text(prop, "Dash 3", "Length of the 3rd dash for dashed lines");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "gap3", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "gap3");
  RNA_def_property_range(prop, 0, USHRT_MAX);
  RNA_def_property_ui_text(prop, "Gap 3", "Length of the 3rd gap for dashed lines");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "use_texture", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", LS_TEXTURE);
  RNA_def_property_ui_text(prop, "Use Textures", "Enable or disable textured strokes");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  prop = RNA_def_property(srna, "texture_spacing", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "texstep");
  RNA_def_property_range(prop, 0.01f, 100.0f);
  RNA_def_property_ui_text(prop, "Texture spacing", "Spacing for textures along stroke length");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_update");

  /* anim */
  rna_def_animdata_common(srna);

  /* nodes */
  prop = RNA_def_property(srna, "node_tree", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "nodetree");
  RNA_def_property_ui_text(prop, "Node Tree", "Node tree for node-based shaders");

  prop = RNA_def_property(srna, "use_nodes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "use_nodes", 1);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_ui_text(prop, "Use Nodes", "Use shader nodes for the line style");
  RNA_def_property_update(prop, NC_LINESTYLE, "rna_LineStyle_use_nodes_update");
}

void RNA_def_linestyle(BlenderRNA *brna)
{
  rna_def_linestyle_modifiers(brna);
  rna_def_linestyle(brna);
  rna_def_linestyle_mtex(brna);
}

#endif
