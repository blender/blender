/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#include "BKE_blender_project.hh"
#include "BLI_string_ref.hh"

#include "BLT_translation.hh"

#include "WM_api.hh"

#ifdef RNA_RUNTIME

namespace blender {

static void project_mark_dirty(bke::BlenderProject *project)
{
  BLI_assert(project != nullptr);
  bke::with_blender_project_write_lock([&] { project->is_dirty = true; });
}

/* For properties that AREN'T saved to disk as part of the project data. */
static void rna_BlenderProject_ui_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA * /*ptr*/)
{
  /* Force full redraw of all windows. */
  WM_main_add_notifier(NC_WINDOW, nullptr);
}

/* For properties that ARE saved to disk as part of the project data. */
static void rna_BlenderProject_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  bke::BlenderProject *project = static_cast<bke::BlenderProject *>(ptr->data);
  project_mark_dirty(project);

  /* Force full redraw of all windows. */
  WM_main_add_notifier(NC_WINDOW, nullptr);
}

static void rna_BlenderProject_name_get(PointerRNA *ptr, char *value)
{
  bke::with_blender_project_read_lock([&] {
    const bke::BlenderProject *project = static_cast<bke::BlenderProject *>(ptr->data);
    strcpy(value, project->get_name().c_str());
  });
}

static int rna_BlenderProject_name_length(PointerRNA *ptr)
{
  int name_length;
  bke::with_blender_project_read_lock([&] {
    const bke::BlenderProject *project = static_cast<bke::BlenderProject *>(ptr->data);
    name_length = project->get_name().size();
  });
  return name_length;
}

static void rna_BlenderProject_name_set(PointerRNA *ptr, const char *value)
{
  bke::with_blender_project_write_lock([&] {
    bke::BlenderProject *project = static_cast<bke::BlenderProject *>(ptr->data);

    StringRef name = StringRef(value);

    if (name.is_empty()) {
      /* Leave the name as-is when passed an empty (which is invalid) name. */
      return;
    }

    project->set_name(name);
  });
}

static void rna_BlenderProject_root_path_get(PointerRNA *ptr, char *value)
{
  bke::with_blender_project_read_lock([&] {
    const bke::BlenderProject *project = static_cast<bke::BlenderProject *>(ptr->data);
    strcpy(value, project->get_root_path().c_str());
  });
}

static int rna_BlenderProject_root_path_length(PointerRNA *ptr)
{
  int root_path_length;
  bke::with_blender_project_read_lock([&] {
    const bke::BlenderProject *project = static_cast<bke::BlenderProject *>(ptr->data);
    root_path_length = project->get_root_path().size();
  });
  return root_path_length;
}

static bool rna_BlenderProject_is_dirty_get(PointerRNA *ptr)
{
  bool is_dirty;
  bke::with_blender_project_read_lock([&] {
    bke::BlenderProject *project = static_cast<bke::BlenderProject *>(ptr->data);
    is_dirty = project->is_dirty;
  });
  return is_dirty;
}

static void rna_BlenderProject_is_dirty_set(PointerRNA *ptr, bool value)
{
  bke::with_blender_project_write_lock([&] {
    bke::BlenderProject *project = static_cast<bke::BlenderProject *>(ptr->data);
    project->is_dirty = value;
  });
}

}  // namespace blender

#else

namespace blender {

static void rna_def_blender_project(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "BlenderProject", nullptr);
  RNA_def_struct_ui_text(srna, "Blender Project", "");

  PropertyRNA *prop;

  prop = RNA_def_property(srna, "is_dirty", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_BlenderProject_is_dirty_get", "rna_BlenderProject_is_dirty_set");
  RNA_def_property_ui_text(prop, "Dirty", "Whether the project has unsaved changes");
  RNA_def_property_update(prop, 0, "rna_BlenderProject_ui_update");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_BlenderProject_name_get",
                                "rna_BlenderProject_name_length",
                                "rna_BlenderProject_name_set");
  RNA_def_property_ui_text(prop, "Name", "The project's name");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, 0, "rna_BlenderProject_update");

  prop = RNA_def_property(srna, "root_path", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(
      prop, "rna_BlenderProject_root_path_get", "rna_BlenderProject_root_path_length", nullptr);
  RNA_def_property_ui_text(prop, "Root Folder", "The path to the root folder of the project");
}

void RNA_def_blender_project(BlenderRNA *brna)
{
  rna_def_blender_project(brna);
}

}  // namespace blender

#endif
