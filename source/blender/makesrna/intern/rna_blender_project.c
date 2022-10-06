/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include "RNA_define.h"

#include "rna_internal.h"

#ifdef RNA_RUNTIME

#  include "BKE_blender_project.h"

#  include "BLT_translation.h"

#  include "WM_api.h"

static void rna_BlenderProject_update(Main *UNUSED(bmain),
                                      Scene *UNUSED(scene),
                                      PointerRNA *UNUSED(ptr))
{
  /* TODO evaluate which props should send which notifiers. */
  /* Force full redraw of all windows. */
  WM_main_add_notifier(NC_WINDOW, NULL);
}

static void rna_BlenderProject_name_get(PointerRNA *ptr, char *value)
{
  BlenderProject *project = ptr->data;
  if (!project) {
    value[0] = '\0';
    return;
  }

  strcpy(value, BKE_project_name_get(project));
}

static int rna_BlenderProject_name_length(PointerRNA *ptr)
{
  BlenderProject *project = ptr->data;
  if (!project) {
    return 0;
  }

  return strlen(BKE_project_name_get(project));
}

static void rna_BlenderProject_name_set(PointerRNA *ptr, const char *value)
{
  BlenderProject *project = ptr->data;

  if (!project) {
    return;
  }

  BKE_project_name_set(project, value);
}

static void rna_BlenderProject_root_path_get(PointerRNA *ptr, char *value)
{
  BlenderProject *project = ptr->data;
  if (!project) {
    value[0] = '\0';
    return;
  }

  strcpy(value, BKE_project_root_path_get(project));
}

static int rna_BlenderProject_root_path_length(PointerRNA *ptr)
{
  BlenderProject *project = ptr->data;
  if (!project) {
    return 0;
  }

  return strlen(BKE_project_root_path_get(project));
}

static void rna_BlenderProject_root_path_set(PointerRNA *UNUSED(ptr), const char *UNUSED(value))
{
  /* Property is not editable, see #rna_BlenderProject_root_path_editable(). */
  BLI_assert_unreachable();
}

static int rna_BlenderProject_root_path_editable(PointerRNA *UNUSED(ptr), const char **r_info)
{
  /* Path is never editable (setting up a project is an operation), but return a nicer disabled
   * hint. */
  *r_info = N_("Project location cannot be changed, displayed for informal purposes only");
  return 0;
}

#else

void RNA_def_blender_project(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "BlenderProject", NULL);
  RNA_def_struct_ui_text(srna, "Blender Project", "");

  PropertyRNA *prop;

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_BlenderProject_name_get",
                                "rna_BlenderProject_name_length",
                                "rna_BlenderProject_name_set");
  RNA_def_property_ui_text(prop, "Name", "The identifier for the project");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, 0, "rna_BlenderProject_update");

  prop = RNA_def_property(srna, "root_path", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_BlenderProject_root_path_get",
                                "rna_BlenderProject_root_path_length",
                                "rna_BlenderProject_root_path_set");
  RNA_def_property_editable_func(prop, "rna_BlenderProject_root_path_editable");
  RNA_def_property_ui_text(prop, "Location", "The location of the project on disk");
}

#endif
