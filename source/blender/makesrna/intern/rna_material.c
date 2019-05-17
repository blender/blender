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

#include <float.h>
#include <stdlib.h>

#include "DNA_material_types.h"
#include "DNA_texture_types.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "WM_api.h"
#include "WM_types.h"

const EnumPropertyItem rna_enum_ramp_blend_items[] = {
    {MA_RAMP_BLEND, "MIX", 0, "Mix", ""},
    {0, "", ICON_NONE, NULL, NULL},
    {MA_RAMP_DARK, "DARKEN", 0, "Darken", ""},
    {MA_RAMP_MULT, "MULTIPLY", 0, "Multiply", ""},
    {MA_RAMP_BURN, "BURN", 0, "Burn", ""},
    {0, "", ICON_NONE, NULL, NULL},
    {MA_RAMP_LIGHT, "LIGHTEN", 0, "Lighten", ""},
    {MA_RAMP_SCREEN, "SCREEN", 0, "Screen", ""},
    {MA_RAMP_DODGE, "DODGE", 0, "Dodge", ""},
    {MA_RAMP_ADD, "ADD", 0, "Add", ""},
    {0, "", ICON_NONE, NULL, NULL},
    {MA_RAMP_OVERLAY, "OVERLAY", 0, "Overlay", ""},
    {MA_RAMP_SOFT, "SOFT_LIGHT", 0, "Soft Light", ""},
    {MA_RAMP_LINEAR, "LINEAR_LIGHT", 0, "Linear Light", ""},
    {0, "", ICON_NONE, NULL, NULL},
    {MA_RAMP_DIFF, "DIFFERENCE", 0, "Difference", ""},
    {MA_RAMP_SUB, "SUBTRACT", 0, "Subtract", ""},
    {MA_RAMP_DIV, "DIVIDE", 0, "Divide", ""},
    {0, "", ICON_NONE, NULL, NULL},
    {MA_RAMP_HUE, "HUE", 0, "Hue", ""},
    {MA_RAMP_SAT, "SATURATION", 0, "Saturation", ""},
    {MA_RAMP_COLOR, "COLOR", 0, "Color", ""},
    {MA_RAMP_VAL, "VALUE", 0, "Value", ""},
    {0, NULL, 0, NULL, NULL},
};

#ifdef RNA_RUNTIME

#  include "MEM_guardedalloc.h"

#  include "DNA_node_types.h"
#  include "DNA_object_types.h"
#  include "DNA_screen_types.h"
#  include "DNA_space_types.h"

#  include "BKE_colorband.h"
#  include "BKE_context.h"
#  include "BKE_main.h"
#  include "BKE_gpencil.h"
#  include "BKE_material.h"
#  include "BKE_texture.h"
#  include "BKE_node.h"
#  include "BKE_paint.h"
#  include "BKE_scene.h"
#  include "BKE_workspace.h"

#  include "DEG_depsgraph.h"
#  include "DEG_depsgraph_build.h"

#  include "ED_node.h"
#  include "ED_image.h"
#  include "ED_screen.h"
#  include "ED_gpencil.h"

static void rna_Material_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  Material *ma = ptr->id.data;

  DEG_id_tag_update(&ma->id, ID_RECALC_SHADING);
  WM_main_add_notifier(NC_MATERIAL | ND_SHADING, ma);
}

static void rna_Material_update_previews(Main *UNUSED(bmain),
                                         Scene *UNUSED(scene),
                                         PointerRNA *ptr)
{
  Material *ma = ptr->id.data;

  if (ma->nodetree)
    BKE_node_preview_clear_tree(ma->nodetree);

  WM_main_add_notifier(NC_MATERIAL | ND_SHADING_PREVIEW, ma);
}

static void rna_MaterialGpencil_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  Material *ma = ptr->id.data;

  rna_Material_update(bmain, scene, ptr);
  WM_main_add_notifier(NC_GPENCIL | ND_DATA, ma);
}

static void rna_MaterialGpencil_nopreview_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  Material *ma = ptr->id.data;

  rna_Material_update(bmain, scene, ptr);
  WM_main_add_notifier(NC_GPENCIL | ND_DATA, ma);
}

static void rna_Material_draw_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  Material *ma = ptr->id.data;

  DEG_id_tag_update(&ma->id, ID_RECALC_SHADING);
  WM_main_add_notifier(NC_MATERIAL | ND_SHADING_DRAW, ma);
}

static void rna_Material_texpaint_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Material *ma = (Material *)ptr->data;
  rna_iterator_array_begin(
      iter, (void *)ma->texpaintslot, sizeof(TexPaintSlot), ma->tot_slots, 0, NULL);
}

static void rna_Material_active_paint_texture_index_update(Main *bmain,
                                                           Scene *UNUSED(scene),
                                                           PointerRNA *ptr)
{
  bScreen *sc;
  Material *ma = ptr->id.data;

  if (ma->use_nodes && ma->nodetree) {
    struct bNode *node;
    int index = 0;
    for (node = ma->nodetree->nodes.first; node; node = node->next) {
      if (node->typeinfo->nclass == NODE_CLASS_TEXTURE &&
          node->typeinfo->type == SH_NODE_TEX_IMAGE && node->id) {
        if (index++ == ma->paint_active_slot) {
          break;
        }
      }
    }
    if (node)
      nodeSetActive(ma->nodetree, node);
  }

  if (ma->texpaintslot) {
    Image *image = ma->texpaintslot[ma->paint_active_slot].ima;
    for (sc = bmain->screens.first; sc; sc = sc->id.next) {
      wmWindow *win = ED_screen_window_find(sc, bmain->wm.first);
      if (win == NULL) {
        continue;
      }

      Object *obedit = NULL;
      {
        ViewLayer *view_layer = WM_window_get_active_view_layer(win);
        obedit = OBEDIT_FROM_VIEW_LAYER(view_layer);
      }

      ScrArea *sa;
      for (sa = sc->areabase.first; sa; sa = sa->next) {
        SpaceLink *sl;
        for (sl = sa->spacedata.first; sl; sl = sl->next) {
          if (sl->spacetype == SPACE_IMAGE) {
            SpaceImage *sima = (SpaceImage *)sl;
            if (!sima->pin) {
              ED_space_image_set(bmain, sima, obedit, image, true);
            }
          }
        }
      }
    }
  }

  DEG_id_tag_update(&ma->id, 0);
  WM_main_add_notifier(NC_MATERIAL | ND_SHADING, ma);
}

static void rna_Material_use_nodes_update(bContext *C, PointerRNA *ptr)
{
  Material *ma = (Material *)ptr->data;
  Main *bmain = CTX_data_main(C);

  if (ma->use_nodes && ma->nodetree == NULL)
    ED_node_shader_default(C, &ma->id);

  DEG_id_tag_update(&ma->id, ID_RECALC_COPY_ON_WRITE);
  DEG_relations_tag_update(bmain);
  rna_Material_draw_update(bmain, CTX_data_scene(C), ptr);
}

MTex *rna_mtex_texture_slots_add(ID *self_id, struct bContext *C, ReportList *reports)
{
  MTex *mtex = BKE_texture_mtex_add_id(self_id, -1);
  if (mtex == NULL) {
    BKE_reportf(reports, RPT_ERROR, "Maximum number of textures added %d", MAX_MTEX);
    return NULL;
  }

  /* for redraw only */
  WM_event_add_notifier(C, NC_TEXTURE, CTX_data_scene(C));

  return mtex;
}

MTex *rna_mtex_texture_slots_create(ID *self_id,
                                    struct bContext *C,
                                    ReportList *reports,
                                    int index)
{
  MTex *mtex;

  if (index < 0 || index >= MAX_MTEX) {
    BKE_reportf(reports, RPT_ERROR, "Index %d is invalid", index);
    return NULL;
  }

  mtex = BKE_texture_mtex_add_id(self_id, index);

  /* for redraw only */
  WM_event_add_notifier(C, NC_TEXTURE, CTX_data_scene(C));

  return mtex;
}

void rna_mtex_texture_slots_clear(ID *self_id, struct bContext *C, ReportList *reports, int index)
{
  MTex **mtex_ar;
  short act;

  give_active_mtex(self_id, &mtex_ar, &act);

  if (mtex_ar == NULL) {
    BKE_report(reports, RPT_ERROR, "Mtex not found for this type");
    return;
  }

  if (index < 0 || index >= MAX_MTEX) {
    BKE_reportf(reports, RPT_ERROR, "Index %d is invalid", index);
    return;
  }

  if (mtex_ar[index]) {
    id_us_min((ID *)mtex_ar[index]->tex);
    MEM_freeN(mtex_ar[index]);
    mtex_ar[index] = NULL;
    DEG_id_tag_update(self_id, 0);
  }

  /* for redraw only */
  WM_event_add_notifier(C, NC_TEXTURE, CTX_data_scene(C));
}

static void rna_TexPaintSlot_uv_layer_get(PointerRNA *ptr, char *value)
{
  TexPaintSlot *data = (TexPaintSlot *)(ptr->data);

  if (data->uvname != NULL) {
    BLI_strncpy_utf8(value, data->uvname, 64);
  }
  else {
    value[0] = '\0';
  }
}

static int rna_TexPaintSlot_uv_layer_length(PointerRNA *ptr)
{
  TexPaintSlot *data = (TexPaintSlot *)(ptr->data);
  return data->uvname == NULL ? 0 : strlen(data->uvname);
}

static void rna_TexPaintSlot_uv_layer_set(PointerRNA *ptr, const char *value)
{
  TexPaintSlot *data = (TexPaintSlot *)(ptr->data);

  if (data->uvname != NULL) {
    BLI_strncpy_utf8(data->uvname, value, 64);
  }
}

static bool rna_is_grease_pencil_get(PointerRNA *ptr)
{
  Material *ma = (Material *)ptr->data;
  if (ma->gp_style != NULL)
    return true;

  return false;
}

static void rna_gpcolordata_uv_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  /* update all uv strokes of this color */
  Material *ma = ptr->id.data;
  ED_gpencil_update_color_uv(bmain, ma);

  rna_MaterialGpencil_update(bmain, scene, ptr);
}

static char *rna_GpencilColorData_path(PointerRNA *UNUSED(ptr))
{
  return BLI_strdup("grease_pencil");
}

static int rna_GpencilColorData_is_stroke_visible_get(PointerRNA *ptr)
{
  MaterialGPencilStyle *pcolor = ptr->data;
  return (pcolor->stroke_rgba[3] > GPENCIL_ALPHA_OPACITY_THRESH);
}

static int rna_GpencilColorData_is_fill_visible_get(PointerRNA *ptr)
{
  MaterialGPencilStyle *pcolor = (MaterialGPencilStyle *)ptr->data;
  return ((pcolor->fill_rgba[3] > GPENCIL_ALPHA_OPACITY_THRESH) || (pcolor->fill_style > 0));
}

static void rna_GpencilColorData_stroke_image_set(struct ReportList *UNUSED(reports),
                                                  PointerRNA *ptr,
                                                  PointerRNA value)
{
  MaterialGPencilStyle *pcolor = ptr->data;
  ID *id = value.data;

  id_us_plus(id);
  pcolor->sima = (struct Image *)id;
}

static void rna_GpencilColorData_fill_image_set(struct ReportList *UNUSED(reports),
                                                PointerRNA *ptr,
                                                PointerRNA value)
{
  MaterialGPencilStyle *pcolor = (MaterialGPencilStyle *)ptr->data;
  ID *id = value.data;

  id_us_plus(id);
  pcolor->ima = (struct Image *)id;
}

#else

static void rna_def_material_display(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "diffuse_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, NULL, "r");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Diffuse Color", "Diffuse color of the material");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  prop = RNA_def_property(srna, "specular_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, NULL, "specr");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Specular Color", "Specular color of the material");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  prop = RNA_def_property(srna, "roughness", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "roughness");
  RNA_def_property_float_default(prop, 0.25f);
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_ui_text(prop, "Roughness", "Roughness of the material");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  prop = RNA_def_property(srna, "specular_intensity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "spec");
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_ui_text(prop, "Specular", "How intense (bright) the specular reflection is");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  prop = RNA_def_property(srna, "metallic", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "metallic");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Metallic", "Amount of mirror reflection for raytrace");
  RNA_def_property_update(prop, 0, "rna_Material_update");

  /* Freestyle line color */
  prop = RNA_def_property(srna, "line_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, NULL, "line_col");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Line Color", "Line color used for Freestyle line rendering");
  RNA_def_property_update(prop, 0, "rna_Material_update");

  prop = RNA_def_property(srna, "line_priority", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "line_priority");
  RNA_def_property_range(prop, 0, 32767);
  RNA_def_property_ui_text(
      prop, "Line Priority", "The line color of a higher priority is used at material boundaries");
  RNA_def_property_update(prop, 0, "rna_Material_update");
}

static void rna_def_material_greasepencil(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* mode type styles */
  static EnumPropertyItem gpcolordata_mode_types_items[] = {
      {GP_STYLE_MODE_LINE, "LINE", 0, "Line", "Draw strokes using a continuous line"},
      {GP_STYLE_MODE_DOTS, "DOTS", 0, "Dots", "Draw strokes using separated dots"},
      {GP_STYLE_MODE_BOX, "BOX", 0, "Boxes", "Draw strokes using separated rectangle boxes"},
      {0, NULL, 0, NULL, NULL},
  };

  /* stroke styles */
  static EnumPropertyItem stroke_style_items[] = {
      {GP_STYLE_STROKE_STYLE_SOLID, "SOLID", 0, "Solid", "Draw strokes with solid color"},
      {GP_STYLE_STROKE_STYLE_TEXTURE, "TEXTURE", 0, "Texture", "Draw strokes using texture"},
      {0, NULL, 0, NULL, NULL},
  };

  /* fill styles */
  static EnumPropertyItem fill_style_items[] = {
      {GP_STYLE_FILL_STYLE_SOLID, "SOLID", 0, "Solid", "Fill area with solid color"},
      {GP_STYLE_FILL_STYLE_GRADIENT, "GRADIENT", 0, "Gradient", "Fill area with gradient color"},
      {GP_STYLE_FILL_STYLE_CHESSBOARD,
       "CHESSBOARD",
       0,
       "Checker Board",
       "Fill area with chessboard pattern"},
      {GP_STYLE_FILL_STYLE_TEXTURE, "TEXTURE", 0, "Texture", "Fill area with image texture"},
      {0, NULL, 0, NULL, NULL},
  };

  static EnumPropertyItem fill_gradient_items[] = {
      {GP_STYLE_GRADIENT_LINEAR, "LINEAR", 0, "Linear", "Fill area with gradient color"},
      {GP_STYLE_GRADIENT_RADIAL, "RADIAL", 0, "Radial", "Fill area with radial gradient"},
      {0, NULL, 0, NULL, NULL},
  };

  static EnumPropertyItem alignment_draw_items[] = {
      {GP_STYLE_FOLLOW_PATH, "PATH", 0, "Path", "Follow stroke drawing path and object rotation"},
      {GP_STYLE_FOLLOW_OBJ, "OBJECT", 0, "Object", "Follow object rotation only"},
      {GP_STYLE_FOLLOW_FIXED,
       "FIXED",
       0,
       "Fixed",
       "Do not follow drawing path or object rotation and keeps aligned with viewport"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "MaterialGPencilStyle", NULL);
  RNA_def_struct_sdna(srna, "MaterialGPencilStyle");
  RNA_def_struct_ui_text(srna, "Grease Pencil Color", "");
  RNA_def_struct_path_func(srna, "rna_GpencilColorData_path");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_float_sdna(prop, NULL, "stroke_rgba");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Color", "");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* Fill Drawing Color */
  prop = RNA_def_property(srna, "fill_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "fill_rgba");
  RNA_def_property_array(prop, 4);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Fill Color", "Color for filling region bounded by each stroke");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* Secondary Drawing Color */
  prop = RNA_def_property(srna, "mix_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "mix_rgba");
  RNA_def_property_array(prop, 4);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Mix Color", "Color for mixing with primary filling color");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* Mix factor */
  prop = RNA_def_property(srna, "mix_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "mix_factor");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Mix", "Mix Adjustment Factor");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* Stroke Mix factor */
  prop = RNA_def_property(srna, "mix_stroke_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "mix_stroke_factor");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Mix", "Mix Stroke Color");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* Scale factor for uv coordinates */
  prop = RNA_def_property(srna, "pattern_scale", PROP_FLOAT, PROP_COORDS);
  RNA_def_property_float_sdna(prop, NULL, "gradient_scale");
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "Scale", "Scale Factor for UV coordinates");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* Shift factor to move pattern filling in 2d space */
  prop = RNA_def_property(srna, "pattern_shift", PROP_FLOAT, PROP_COORDS);
  RNA_def_property_float_sdna(prop, NULL, "gradient_shift");
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "Shift", "Shift filling pattern in 2d space");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* Gradient angle */
  prop = RNA_def_property(srna, "pattern_angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "gradient_angle");
  RNA_def_property_ui_text(prop, "Angle", "Pattern Orientation Angle");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* Gradient radius */
  prop = RNA_def_property(srna, "pattern_radius", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "gradient_radius");
  RNA_def_property_range(prop, 0.0001f, 10.0f);
  RNA_def_property_ui_text(prop, "Radius", "Pattern Radius");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* Box size */
  prop = RNA_def_property(srna, "pattern_gridsize", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "pattern_gridsize");
  RNA_def_property_range(prop, 0.0001f, 10.0f);
  RNA_def_property_ui_text(prop, "Size", "Box Size");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* Texture angle */
  prop = RNA_def_property(srna, "texture_angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "texture_angle");
  RNA_def_property_ui_text(prop, "Angle", "Texture Orientation Angle");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* Scale factor for texture */
  prop = RNA_def_property(srna, "texture_scale", PROP_FLOAT, PROP_COORDS);
  RNA_def_property_float_sdna(prop, NULL, "texture_scale");
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "Scale", "Scale Factor for Texture");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* Shift factor to move texture in 2d space */
  prop = RNA_def_property(srna, "texture_offset", PROP_FLOAT, PROP_COORDS);
  RNA_def_property_float_sdna(prop, NULL, "texture_offset");
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "Offset", "Shift Texture in 2d Space");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* Texture opacity size */
  prop = RNA_def_property(srna, "texture_opacity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "texture_opacity");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Opacity", "Texture Opacity");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* texture pixsize factor (used for UV along the stroke) */
  prop = RNA_def_property(srna, "pixel_size", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "texture_pixsize");
  RNA_def_property_range(prop, 1, 5000);
  RNA_def_property_ui_text(prop, "UV Factor", "Texture Pixel Size factor along the stroke");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_gpcolordata_uv_update");

  /* Flags */
  prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_STYLE_COLOR_HIDE);
  RNA_def_property_ui_icon(prop, ICON_HIDE_OFF, -1);
  RNA_def_property_ui_text(prop, "Hide", "Set color Visibility");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_nopreview_update");

  prop = RNA_def_property(srna, "lock", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_STYLE_COLOR_LOCKED);
  RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
  RNA_def_property_ui_text(
      prop, "Locked", "Protect color from further editing and/or frame changes");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_nopreview_update");

  prop = RNA_def_property(srna, "ghost", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_STYLE_COLOR_ONIONSKIN);
  RNA_def_property_ui_icon(prop, ICON_GHOST_ENABLED, 0);
  RNA_def_property_ui_text(
      prop, "Show in Ghosts", "Display strokes using this color when showing onion skins");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_nopreview_update");

  prop = RNA_def_property(srna, "texture_clamp", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_STYLE_COLOR_TEX_CLAMP);
  RNA_def_property_ui_text(prop, "Clamp", "Do not repeat texture and clamp to one instance only");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  prop = RNA_def_property(srna, "use_fill_texture_mix", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_STYLE_FILL_TEX_MIX);
  RNA_def_property_ui_text(prop, "Mix Texture", "Mix texture image with filling color");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  prop = RNA_def_property(srna, "use_stroke_texture_mix", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_STYLE_STROKE_TEX_MIX);
  RNA_def_property_ui_text(prop, "Mix Texture", "Mix texture image with stroke color");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  prop = RNA_def_property(srna, "flip", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_STYLE_COLOR_FLIP_FILL);
  RNA_def_property_ui_text(prop, "Flip", "Flip filling colors");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  prop = RNA_def_property(srna, "use_stroke_pattern", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_STYLE_STROKE_PATTERN);
  RNA_def_property_ui_text(prop, "Pattern", "Use Stroke Texture as a pattern to apply color");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  prop = RNA_def_property(srna, "use_fill_pattern", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_STYLE_FILL_PATTERN);
  RNA_def_property_ui_text(prop, "Pattern", "Use Fill Texture as a pattern to apply color");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  prop = RNA_def_property(srna, "show_stroke", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_STYLE_STROKE_SHOW);
  RNA_def_property_ui_text(prop, "Show Stroke", "Show stroke lines of this material");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  prop = RNA_def_property(srna, "show_fill", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_STYLE_FILL_SHOW);
  RNA_def_property_ui_text(prop, "Show Fill", "Show stroke fills of this material");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* Mode to align Dots and Boxes to drawing path and object rotation */
  prop = RNA_def_property(srna, "alignment_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "alignment_mode");
  RNA_def_property_enum_items(prop, alignment_draw_items);
  RNA_def_property_ui_text(
      prop, "Alignment", "Defines how align Dots and Boxes with drawing path and object rotation");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_nopreview_update");

  /* pass index for future compositing and editing tools */
  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "index");
  RNA_def_property_ui_text(prop, "Pass Index", "Index number for the \"Color Index\" pass");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_nopreview_update");

  /* mode type */
  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "mode");
  RNA_def_property_enum_items(prop, gpcolordata_mode_types_items);
  RNA_def_property_ui_text(prop, "Mode Type", "Select draw mode for stroke");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* stroke style */
  prop = RNA_def_property(srna, "stroke_style", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "stroke_style");
  RNA_def_property_enum_items(prop, stroke_style_items);
  RNA_def_property_ui_text(prop, "Stroke Style", "Select style used to draw strokes");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* stroke image texture */
  prop = RNA_def_property(srna, "stroke_image", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "sima");
  RNA_def_property_pointer_funcs(prop, NULL, "rna_GpencilColorData_stroke_image_set", NULL, NULL);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Image", "");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* fill style */
  prop = RNA_def_property(srna, "fill_style", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "fill_style");
  RNA_def_property_enum_items(prop, fill_style_items);
  RNA_def_property_ui_text(prop, "Fill Style", "Select style used to fill strokes");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* gradient type */
  prop = RNA_def_property(srna, "gradient_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "gradient_type");
  RNA_def_property_enum_items(prop, fill_gradient_items);
  RNA_def_property_ui_text(prop, "Gradient Type", "Select type of gradient used to fill strokes");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* fill image texture */
  prop = RNA_def_property(srna, "fill_image", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "ima");
  RNA_def_property_pointer_funcs(prop, NULL, "rna_GpencilColorData_fill_image_set", NULL, NULL);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Image", "");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_MaterialGpencil_update");

  /* Read-only state props (for simpler UI code) */
  prop = RNA_def_property(srna, "is_stroke_visible", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_GpencilColorData_is_stroke_visible_get", NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Is Stroke Visible", "True when opacity of stroke is set high enough to be visible");

  prop = RNA_def_property(srna, "is_fill_visible", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_GpencilColorData_is_fill_visible_get", NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Is Fill Visible", "True when opacity of fill is set high enough to be visible");
}

void RNA_def_material(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* Render Preview Types */
  static const EnumPropertyItem preview_type_items[] = {
      {MA_FLAT, "FLAT", ICON_MATPLANE, "Flat", "Flat XY plane"},
      {MA_SPHERE, "SPHERE", ICON_MATSPHERE, "Sphere", "Sphere"},
      {MA_CUBE, "CUBE", ICON_MATCUBE, "Cube", "Cube"},
      {MA_HAIR, "HAIR", ICON_HAIR, "Hair", "Hair strands"},
      {MA_SHADERBALL, "SHADERBALL", ICON_MATSHADERBALL, "Shader Ball", "Shader Ball"},
      {MA_CLOTH, "CLOTH", ICON_MATCLOTH, "Cloth", "Cloth"},
      {MA_FLUID, "FLUID", ICON_MATFLUID, "Fluid", "Fluid"},
      {0, NULL, 0, NULL, NULL},
  };

  static EnumPropertyItem prop_eevee_blend_items[] = {
      {MA_BM_SOLID, "OPAQUE", 0, "Opaque", "Render surface without transparency"},
      {MA_BM_ADD,
       "ADD",
       0,
       "Additive",
       "Render surface and blend the result with additive blending"},
      {MA_BM_MULTIPLY,
       "MULTIPLY",
       0,
       "Multiply",
       "Render surface and blend the result with multiplicative blending"},
      {MA_BM_CLIP,
       "CLIP",
       0,
       "Alpha Clip",
       "Use the alpha threshold to clip the visibility (binary visibility)"},
      {MA_BM_HASHED,
       "HASHED",
       0,
       "Alpha Hashed",
       "Use noise to dither the binary visibility (works well with multi-samples)"},
      {MA_BM_BLEND,
       "BLEND",
       0,
       "Alpha Blend",
       "Render polygon transparent, depending on alpha channel of the texture"},
      {0, NULL, 0, NULL, NULL},
  };

  static EnumPropertyItem prop_eevee_blend_shadow_items[] = {
      {MA_BS_NONE, "NONE", 0, "None", "Material will cast no shadow"},
      {MA_BS_SOLID, "OPAQUE", 0, "Opaque", "Material will cast shadows without transparency"},
      {MA_BS_CLIP,
       "CLIP",
       0,
       "Alpha Clip",
       "Use the alpha threshold to clip the visibility (binary visibility)"},
      {MA_BS_HASHED,
       "HASHED",
       0,
       "Alpha Hashed",
       "Use noise to dither the binary visibility and use filtering to reduce the noise"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "Material", "ID");
  RNA_def_struct_ui_text(
      srna,
      "Material",
      "Material data-block to define the appearance of geometric objects for rendering");
  RNA_def_struct_ui_icon(srna, ICON_MATERIAL_DATA);

  /* Blending (only Eevee for now) */
  prop = RNA_def_property(srna, "blend_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_eevee_blend_items);
  RNA_def_property_ui_text(prop, "Blend Mode", "Blend Mode for Transparent Faces");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  prop = RNA_def_property(srna, "shadow_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "blend_shadow");
  RNA_def_property_enum_items(prop, prop_eevee_blend_shadow_items);
  RNA_def_property_ui_text(prop, "Shadow Mode", "Shadow mapping method");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  prop = RNA_def_property(srna, "alpha_threshold", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0, 1);
  RNA_def_property_ui_text(prop,
                           "Clip Threshold",
                           "A pixel is rendered only if its alpha value is above this threshold");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  prop = RNA_def_property(srna, "show_transparent_back", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "blend_flag", MA_BL_HIDE_BACKFACE);
  RNA_def_property_ui_text(prop,
                           "Show Backface",
                           "Limit transparency to a single layer "
                           "(avoids transparency sorting problems)");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  prop = RNA_def_property(srna, "use_backface_culling", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "blend_flag", MA_BL_CULL_BACKFACE);
  RNA_def_property_ui_text(
      prop, "Backface Culling", "Use back face culling to hide the back side of faces");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  prop = RNA_def_property(srna, "use_screen_refraction", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "blend_flag", MA_BL_SS_REFRACTION);
  RNA_def_property_ui_text(
      prop, "Screen Space Refraction", "Use raytraced screen space refractions");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  prop = RNA_def_property(srna, "use_sss_translucency", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "blend_flag", MA_BL_TRANSLUCENCY);
  RNA_def_property_ui_text(
      prop, "Subsurface Translucency", "Add translucency effect to subsurface");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  prop = RNA_def_property(srna, "refraction_depth", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "refract_depth");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_text(prop,
                           "Refraction Depth",
                           "Approximate the thickness of the object to compute two refraction "
                           "event (0 is disabled)");
  RNA_def_property_update(prop, 0, "rna_Material_draw_update");

  /* For Preview Render */
  prop = RNA_def_property(srna, "preview_render_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "pr_type");
  RNA_def_property_enum_items(prop, preview_type_items);
  RNA_def_property_ui_text(prop, "Preview Render Type", "Type of preview render");
  RNA_def_property_update(prop, 0, "rna_Material_update_previews");

  prop = RNA_def_property(srna, "use_preview_world", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "pr_flag", MA_PREVIEW_WORLD);
  RNA_def_property_ui_text(
      prop, "Preview World", "Use the current world background to light the preview render");
  RNA_def_property_update(prop, 0, "rna_Material_update_previews");

  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "index");
  RNA_def_property_ui_text(
      prop, "Pass Index", "Index number for the \"Material Index\" render pass");
  RNA_def_property_update(prop, NC_OBJECT, "rna_Material_update");

  /* nodetree */
  prop = RNA_def_property(srna, "node_tree", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "nodetree");
  RNA_def_property_ui_text(prop, "Node Tree", "Node tree for node based materials");

  prop = RNA_def_property(srna, "use_nodes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "use_nodes", 1);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_ui_text(prop, "Use Nodes", "Use shader nodes to render the material");
  RNA_def_property_update(prop, 0, "rna_Material_use_nodes_update");

  /* common */
  rna_def_animdata_common(srna);
  rna_def_texpaint_slots(brna, srna);

  rna_def_material_display(srna);

  /* grease pencil */
  prop = RNA_def_property(srna, "grease_pencil", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "gp_style");
  RNA_def_property_ui_text(
      prop, "Grease Pencil Settings", "Grease pencil color settings for material");

  prop = RNA_def_property(srna, "is_grease_pencil", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_is_grease_pencil_get", NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Is Grease Pencil", "True if this material has grease pencil data");

  rna_def_material_greasepencil(brna);

  RNA_api_material(srna);
}

static void rna_def_texture_slots(BlenderRNA *brna,
                                  PropertyRNA *cprop,
                                  const char *structname,
                                  const char *structname_slots)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, structname_slots);
  srna = RNA_def_struct(brna, structname_slots, NULL);
  RNA_def_struct_sdna(srna, "ID");
  RNA_def_struct_ui_text(srna, "Texture Slots", "Collection of texture slots");

  /* functions */
  func = RNA_def_function(srna, "add", "rna_mtex_texture_slots_add");
  RNA_def_function_flag(func,
                        FUNC_USE_SELF_ID | FUNC_NO_SELF | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "mtex", structname, "", "The newly initialized mtex");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "create", "rna_mtex_texture_slots_create");
  RNA_def_function_flag(func,
                        FUNC_USE_SELF_ID | FUNC_NO_SELF | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  parm = RNA_def_int(
      func, "index", 0, 0, INT_MAX, "Index", "Slot index to initialize", 0, INT_MAX);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "mtex", structname, "", "The newly initialized mtex");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "clear", "rna_mtex_texture_slots_clear");
  RNA_def_function_flag(func,
                        FUNC_USE_SELF_ID | FUNC_NO_SELF | FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  parm = RNA_def_int(func, "index", 0, 0, INT_MAX, "Index", "Slot index to clear", 0, INT_MAX);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
}

void rna_def_mtex_common(BlenderRNA *brna,
                         StructRNA *srna,
                         const char *begin,
                         const char *activeget,
                         const char *activeset,
                         const char *activeeditable,
                         const char *structname,
                         const char *structname_slots,
                         const char *update,
                         const char *update_index)
{
  PropertyRNA *prop;

  /* mtex */
  prop = RNA_def_property(srna, "texture_slots", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, structname);
  RNA_def_property_collection_funcs(prop,
                                    begin,
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_ui_text(
      prop, "Textures", "Texture slots defining the mapping and influence of textures");
  rna_def_texture_slots(brna, prop, structname, structname_slots);

  prop = RNA_def_property(srna, "active_texture", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Texture");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  if (activeeditable) {
    RNA_def_property_editable_func(prop, activeeditable);
  }
  RNA_def_property_pointer_funcs(prop, activeget, activeset, NULL, NULL);
  RNA_def_property_ui_text(prop, "Active Texture", "Active texture slot being displayed");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING_LINKS, update);

  prop = RNA_def_property(srna, "active_texture_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "texact");
  RNA_def_property_range(prop, 0, MAX_MTEX - 1);
  RNA_def_property_ui_text(prop, "Active Texture Index", "Index of active texture slot");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING_LINKS, update_index);
}

static void rna_def_tex_slot(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "TexPaintSlot", NULL);
  RNA_def_struct_ui_text(
      srna, "Texture Paint Slot", "Slot that contains information about texture painting");

  prop = RNA_def_property(srna, "uv_layer", PROP_STRING, PROP_NONE);
  RNA_def_property_string_maxlength(prop, 64); /* else it uses the pointer size! */
  RNA_def_property_string_sdna(prop, NULL, "uvname");
  RNA_def_property_string_funcs(prop,
                                "rna_TexPaintSlot_uv_layer_get",
                                "rna_TexPaintSlot_uv_layer_length",
                                "rna_TexPaintSlot_uv_layer_set");
  RNA_def_property_ui_text(prop, "UV Map", "Name of UV map");
  RNA_def_property_update(prop, NC_GEOM | ND_DATA, "rna_Material_update");

  prop = RNA_def_property(srna, "is_valid", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "valid", 1);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Valid", "Slot has a valid image and UV map");
}

void rna_def_texpaint_slots(BlenderRNA *brna, StructRNA *srna)
{
  PropertyRNA *prop;

  rna_def_tex_slot(brna);

  /* mtex */
  prop = RNA_def_property(srna, "texture_paint_images", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "texpaintslot", NULL);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Material_texpaint_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_struct_type(prop, "Image");
  RNA_def_property_ui_text(
      prop, "Texture Slot Images", "Texture images used for texture painting");

  prop = RNA_def_property(srna, "texture_paint_slots", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Material_texpaint_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_struct_type(prop, "TexPaintSlot");
  RNA_def_property_ui_text(
      prop, "Texture Slots", "Texture slots defining the mapping and influence of textures");

  prop = RNA_def_property(srna, "paint_active_slot", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_range(prop, 0, SHRT_MAX);
  RNA_def_property_ui_text(
      prop, "Active Paint Texture Index", "Index of active texture paint slot");
  RNA_def_property_update(
      prop, NC_MATERIAL | ND_SHADING_LINKS, "rna_Material_active_paint_texture_index_update");

  prop = RNA_def_property(srna, "paint_clone_slot", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_range(prop, 0, SHRT_MAX);
  RNA_def_property_ui_text(prop, "Clone Paint Texture Index", "Index of clone texture paint slot");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING_LINKS, NULL);
}

#endif
