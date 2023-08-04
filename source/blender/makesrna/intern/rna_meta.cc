/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"

#include "BLI_utildefines.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#ifdef RNA_RUNTIME

#  include "BLI_math.h"

#  include "MEM_guardedalloc.h"

#  include "DNA_object_types.h"
#  include "DNA_scene_types.h"

#  include "BKE_main.h"
#  include "BKE_mball.h"
#  include "BKE_scene.h"

#  include "DEG_depsgraph.h"

#  include "WM_api.hh"
#  include "WM_types.hh"

static int rna_Meta_texspace_editable(PointerRNA *ptr, const char ** /*r_info*/)
{
  MetaBall *mb = (MetaBall *)ptr->data;
  return (mb->texspace_flag & MB_TEXSPACE_FLAG_AUTO) ? 0 : int(PROP_EDITABLE);
}

static void rna_Meta_texspace_location_get(PointerRNA *ptr, float *values)
{
  MetaBall *mb = (MetaBall *)ptr->data;

  /* tex_space_mball() needs object.. ugh */

  copy_v3_v3(values, mb->texspace_location);
}

static void rna_Meta_texspace_location_set(PointerRNA *ptr, const float *values)
{
  MetaBall *mb = (MetaBall *)ptr->data;

  copy_v3_v3(mb->texspace_location, values);
}

static void rna_Meta_texspace_size_get(PointerRNA *ptr, float *values)
{
  MetaBall *mb = (MetaBall *)ptr->data;

  /* tex_space_mball() needs object.. ugh */

  copy_v3_v3(values, mb->texspace_size);
}

static void rna_Meta_texspace_size_set(PointerRNA *ptr, const float *values)
{
  MetaBall *mb = (MetaBall *)ptr->data;

  copy_v3_v3(mb->texspace_size, values);
}

static void rna_MetaBall_redraw_data(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  ID *id = ptr->owner_id;

  DEG_id_tag_update(id, ID_RECALC_COPY_ON_WRITE);
  WM_main_add_notifier(NC_GEOM | ND_DATA, id);
}

static void rna_MetaBall_update_data(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  MetaBall *mb = (MetaBall *)ptr->owner_id;

  /* NOTE: The check on the number of users allows to avoid many repetitive (slow) updates in some
   * cases, like e.g. importers. Calling `BKE_mball_properties_copy` on an obdata with no users
   * would be meaningless anyway, as by definition it would not be used by any object, so not part
   * of any meta-ball group. */
  if (mb->id.us > 0) {
    BKE_mball_properties_copy(bmain, mb);

    DEG_id_tag_update(&mb->id, 0);
    WM_main_add_notifier(NC_GEOM | ND_DATA, mb);
  }
}

static void rna_MetaBall_update_rotation(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  MetaElem *ml = static_cast<MetaElem *>(ptr->data);
  normalize_qt(ml->quat);
  rna_MetaBall_update_data(bmain, scene, ptr);
}

static MetaElem *rna_MetaBall_elements_new(MetaBall *mb, int type)
{
  MetaElem *ml = BKE_mball_element_add(mb, type);

  /* cheating way for importers to avoid slow updates */
  if (mb->id.us > 0) {
    DEG_id_tag_update(&mb->id, 0);
    WM_main_add_notifier(NC_GEOM | ND_DATA, &mb->id);
  }

  return ml;
}

static void rna_MetaBall_elements_remove(MetaBall *mb, ReportList *reports, PointerRNA *ml_ptr)
{
  MetaElem *ml = static_cast<MetaElem *>(ml_ptr->data);

  if (BLI_remlink_safe(&mb->elems, ml) == false) {
    BKE_reportf(
        reports, RPT_ERROR, "Metaball '%s' does not contain spline given", mb->id.name + 2);
    return;
  }

  MEM_freeN(ml);
  RNA_POINTER_INVALIDATE(ml_ptr);

  /* cheating way for importers to avoid slow updates */
  if (mb->id.us > 0) {
    DEG_id_tag_update(&mb->id, 0);
    WM_main_add_notifier(NC_GEOM | ND_DATA, &mb->id);
  }
}

static void rna_MetaBall_elements_clear(MetaBall *mb)
{
  BLI_freelistN(&mb->elems);

  /* cheating way for importers to avoid slow updates */
  if (mb->id.us > 0) {
    DEG_id_tag_update(&mb->id, 0);
    WM_main_add_notifier(NC_GEOM | ND_DATA, &mb->id);
  }
}

static bool rna_Meta_is_editmode_get(PointerRNA *ptr)
{
  MetaBall *mb = (MetaBall *)ptr->owner_id;
  return (mb->editelems != nullptr);
}

static char *rna_MetaElement_path(const PointerRNA *ptr)
{
  const MetaBall *mb = (MetaBall *)ptr->owner_id;
  const MetaElem *ml = static_cast<MetaElem *>(ptr->data);
  int index = -1;

  if (mb->editelems) {
    index = BLI_findindex(mb->editelems, ml);
  }
  if (index == -1) {
    index = BLI_findindex(&mb->elems, ml);
  }
  if (index == -1) {
    return nullptr;
  }

  return BLI_sprintfN("elements[%d]", index);
}

#else

static void rna_def_metaelement(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MetaElement", nullptr);
  RNA_def_struct_sdna(srna, "MetaElem");
  RNA_def_struct_ui_text(srna, "Metaball Element", "Blobby element in a metaball data-block");
  RNA_def_struct_path_func(srna, "rna_MetaElement_path");
  RNA_def_struct_ui_icon(srna, ICON_OUTLINER_DATA_META);

  /* enums */
  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_metaelem_type_items);
  RNA_def_property_ui_text(prop, "Type", "Metaball type");
  RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");

  /* number values */
  prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_sdna(prop, nullptr, "x");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Location", "");
  RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");

  prop = RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_QUATERNION);
  RNA_def_property_float_sdna(prop, nullptr, "quat");
  RNA_def_property_ui_text(prop, "Rotation", "Normalized quaternion rotation");
  RNA_def_property_update(prop, 0, "rna_MetaBall_update_rotation");

  prop = RNA_def_property(srna, "radius", PROP_FLOAT, PROP_UNSIGNED | PROP_UNIT_LENGTH);
  RNA_def_property_float_sdna(prop, nullptr, "rad");
  RNA_def_property_ui_text(prop, "Radius", "");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");

  prop = RNA_def_property(srna, "size_x", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "expx");
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_range(prop, 0.0f, 20.0f);
  RNA_def_property_ui_text(
      prop, "Size X", "Size of element, use of components depends on element type");
  RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");

  prop = RNA_def_property(srna, "size_y", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "expy");
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_range(prop, 0.0f, 20.0f);
  RNA_def_property_ui_text(
      prop, "Size Y", "Size of element, use of components depends on element type");
  RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");

  prop = RNA_def_property(srna, "size_z", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "expz");
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_range(prop, 0.0f, 20.0f);
  RNA_def_property_ui_text(
      prop, "Size Z", "Size of element, use of components depends on element type");
  RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");

  prop = RNA_def_property(srna, "stiffness", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "s");
  RNA_def_property_range(prop, 0.0f, 10.0f);
  RNA_def_property_ui_text(prop, "Stiffness", "Stiffness defines how much of the element to fill");
  RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");

  /* flags */
  prop = RNA_def_property(srna, "use_negative", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", MB_NEGATIVE);
  RNA_def_property_ui_text(prop, "Negative", "Set metaball as negative one");
  RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");

  prop = RNA_def_property(srna, "use_scale_stiffness", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", MB_SCALE_RAD);
  RNA_def_property_ui_text(prop, "Scale Stiffness", "Scale stiffness instead of radius");
  RNA_def_property_update(prop, 0, "rna_MetaBall_redraw_data");

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", 1); /* SELECT */
  RNA_def_property_ui_text(prop, "Select", "Select element");
  RNA_def_property_update(prop, 0, "rna_MetaBall_redraw_data");

  prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", MB_HIDE);
  RNA_def_property_ui_text(prop, "Hide", "Hide element");
  RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");
}

/* mball.elements */
static void rna_def_metaball_elements(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "MetaBallElements");
  srna = RNA_def_struct(brna, "MetaBallElements", nullptr);
  RNA_def_struct_sdna(srna, "MetaBall");
  RNA_def_struct_ui_text(srna, "Metaball Elements", "Collection of metaball elements");

  func = RNA_def_function(srna, "new", "rna_MetaBall_elements_new");
  RNA_def_function_ui_description(func, "Add a new element to the metaball");
  RNA_def_enum(func,
               "type",
               rna_enum_metaelem_type_items,
               MB_BALL,
               "",
               "Type for the new metaball element");
  parm = RNA_def_pointer(func, "element", "MetaElement", "", "The newly created metaball element");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_MetaBall_elements_remove");
  RNA_def_function_ui_description(func, "Remove an element from the metaball");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "element", "MetaElement", "", "The element to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  func = RNA_def_function(srna, "clear", "rna_MetaBall_elements_clear");
  RNA_def_function_ui_description(func, "Remove all elements from the metaball");

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "lastelem");
  RNA_def_property_ui_text(prop, "Active Element", "Last selected element");
}

static void rna_def_metaball(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  static const EnumPropertyItem prop_update_items[] = {
      {MB_UPDATE_ALWAYS, "UPDATE_ALWAYS", 0, "Always", "While editing, update metaball always"},
      {MB_UPDATE_HALFRES,
       "HALFRES",
       0,
       "Half",
       "While editing, update metaball in half resolution"},
      {MB_UPDATE_FAST, "FAST", 0, "Fast", "While editing, update metaball without polygonization"},
      {MB_UPDATE_NEVER, "NEVER", 0, "Never", "While editing, don't update metaball at all"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "MetaBall", "ID");
  RNA_def_struct_ui_text(srna, "MetaBall", "Metaball data-block to define blobby surfaces");
  RNA_def_struct_ui_icon(srna, ICON_META_DATA);

  prop = RNA_def_property(srna, "elements", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "elems", nullptr);
  RNA_def_property_struct_type(prop, "MetaElement");
  RNA_def_property_ui_text(prop, "Elements", "Metaball elements");
  rna_def_metaball_elements(brna, prop);

  /* enums */
  prop = RNA_def_property(srna, "update_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "flag");
  RNA_def_property_enum_items(prop, prop_update_items);
  RNA_def_property_ui_text(prop, "Update", "Metaball edit update behavior");
  RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");

  /* number values */
  prop = RNA_def_property(srna, "resolution", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "wiresize");
  RNA_def_property_range(prop, 0.005f, 10000.0f);
  RNA_def_property_ui_range(prop, 0.05f, 1000.0f, 2.5f, 3);
  RNA_def_property_ui_text(prop, "Viewport Size", "Polygonization resolution in the 3D viewport");
  RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");

  prop = RNA_def_property(srna, "render_resolution", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "rendersize");
  RNA_def_property_range(prop, 0.005f, 10000.0f);
  RNA_def_property_ui_range(prop, 0.025f, 1000.0f, 2.5f, 3);
  RNA_def_property_ui_text(prop, "Render Size", "Polygonization resolution in rendering");
  RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");

  prop = RNA_def_property(srna, "threshold", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "thresh");
  RNA_def_property_range(prop, 0.0f, 5.0f);
  RNA_def_property_ui_text(prop, "Threshold", "Influence of metaball elements");
  RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");

  /* texture space */
  prop = RNA_def_property(srna, "use_auto_texspace", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "texspace_flag", MB_TEXSPACE_FLAG_AUTO);
  RNA_def_property_ui_text(
      prop,
      "Auto Texture Space",
      "Adjust active object's texture space automatically when transforming object");

  prop = RNA_def_property(srna, "texspace_location", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Texture Space Location", "Texture space location");
  RNA_def_property_editable_func(prop, "rna_Meta_texspace_editable");
  RNA_def_property_float_funcs(
      prop, "rna_Meta_texspace_location_get", "rna_Meta_texspace_location_set", nullptr);
  RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");

  prop = RNA_def_property(srna, "texspace_size", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_array(prop, 3);
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_ui_text(prop, "Texture Space Size", "Texture space size");
  RNA_def_property_editable_func(prop, "rna_Meta_texspace_editable");
  RNA_def_property_float_funcs(
      prop, "rna_Meta_texspace_size_get", "rna_Meta_texspace_size_set", nullptr);
  RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");

  /* not supported yet */
#  if 0
  prop = RNA_def_property(srna, "texspace_rot", PROP_FLOAT, PROP_EULER);
  RNA_def_property_float(prop, nullptr, "rot");
  RNA_def_property_ui_text(prop, "Texture Space Rotation", "Texture space rotation");
  RNA_def_property_editable_func(prop, "rna_Meta_texspace_editable");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 100, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, 0, "rna_MetaBall_update_data");
#  endif

  /* materials */
  prop = RNA_def_property(srna, "materials", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "mat", "totcol");
  RNA_def_property_struct_type(prop, "Material");
  RNA_def_property_ui_text(prop, "Materials", "");
  RNA_def_property_srna(prop, "IDMaterials"); /* see rna_ID.cc */
  RNA_def_property_collection_funcs(prop,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    "rna_IDMaterials_assign_int");

  prop = RNA_def_property(srna, "is_editmode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_Meta_is_editmode_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Is Editmode", "True when used in editmode");

  /* anim */
  rna_def_animdata_common(srna);

  RNA_api_meta(srna);
}

void RNA_def_meta(BlenderRNA *brna)
{
  rna_def_metaelement(brna);
  rna_def_metaball(brna);
}

#endif
