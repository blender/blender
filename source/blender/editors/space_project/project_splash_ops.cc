/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spuserpref
 */

#include "BLI_fileops.hh"
#include "BLI_path_utils.hh"

#include "BKE_blender_project.hh"
#include "BKE_context.hh"
#include "BKE_report.hh"

#include "DNA_space_types.h"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "project_intern.hh"

namespace blender {

static bool project_poll(bContext *C)
{
  return BKE_blender_project_read_callback(
      CTX_data_main(C), [](const bke::BlenderProject *project) { return project != nullptr; });
}

static std::string splash_image_default_filepath(const bke::BlenderProject &project)
{
  char filepath[FILE_MAX];
  BLI_path_join(filepath,
                sizeof(filepath),
                project.get_root_path().c_str(),
                BLENDER_PROJECT_CONFIG_DIR,
                "splash.jpg");
  return filepath;
}

static void splash_image_path_set(Main *bmain, const StringRef filepath)
{
  BKE_blender_project_write_callback(bmain, [&](bke::BlenderProject *project) {
    if (project) {
      project->set_splash_image_path(
          BKE_blender_project_path_make_relative(filepath, false, *project));
    }
  });

  WM_main_add_notifier(NC_WINDOW, nullptr);
}

/* -------------------------------------------------------------------- */
/** \name Create Splash Image Operator
 * \{ */

static wmOperatorStatus project_splash_image_create_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const std::string splash_filepath = BKE_blender_project_read_callback(
      bmain, [](const bke::BlenderProject *project) {
        return project ? splash_image_default_filepath(*project) : std::string();
      });
  if (splash_filepath.empty()) {
    return OPERATOR_CANCELLED;
  }

  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);

  /* Use the existing image as is, re-encoding would only lose quality. */
  if (BLI_path_cmp_normalized(filepath, splash_filepath.c_str()) == 0) {
    splash_image_path_set(bmain, splash_filepath);
    return OPERATOR_FINISHED;
  }

  ImBuf *ibuf = WM_project_splash_image_load(filepath);
  if (ibuf == nullptr) {
    BKE_reportf(op->reports, RPT_ERROR, "Failed to load image \"%s\"", filepath);
    return OPERATOR_CANCELLED;
  }

  ibuf->ftype = IMB_FTYPE_JPG;
  ibuf->foptions.quality = 95;
  ibuf->color_mode = ImColorMode::RGB;
  const bool ok = BLI_file_ensure_parent_dir_exists(splash_filepath.c_str()) &&
                  IMB_save_image(ibuf, splash_filepath.c_str(), ImBufFlags::ByteData);
  IMB_freeImBuf(ibuf);

  if (!ok) {
    BKE_reportf(op->reports, RPT_ERROR, "Failed to write \"%s\"", splash_filepath.c_str());
    return OPERATOR_CANCELLED;
  }

  splash_image_path_set(bmain, splash_filepath);
  return OPERATOR_FINISHED;
}

static wmOperatorStatus project_splash_image_create_invoke(bContext *C,
                                                           wmOperator *op,
                                                           const wmEvent * /*event*/)
{
  /* Default to the existing splash.jpg, to easily restore after clear. */
  const std::string splash_filepath = BKE_blender_project_read_callback(
      CTX_data_main(C), [](const bke::BlenderProject *project) {
        return project ? splash_image_default_filepath(*project) : std::string();
      });
  if (!splash_filepath.empty() && BLI_is_file(splash_filepath.c_str())) {
    RNA_string_set(op->ptr, "filepath", splash_filepath.c_str());
  }

  WM_event_add_fileselect(C, op);
  return OPERATOR_RUNNING_MODAL;
}

void PROJECT_OT_splash_image_create(wmOperatorType *ot)
{
  ot->name = "Create Splash Artwork";
  ot->description =
      "Set the project splash artwork from an image file.\n"
      "The image will be cropped, scaled and saved as \".blender_project/splash.jpg\".\n"
      "For optimal results use a 2:1 aspect ratio";
  ot->idname = "PROJECT_OT_splash_image_create";

  ot->poll = project_poll;
  ot->exec = project_splash_image_create_exec;
  ot->invoke = project_splash_image_create_invoke;

  ot->flag = OPTYPE_INTERNAL;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_IMAGE,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear Splash Image Operator
 * \{ */

static bool project_splash_image_clear_poll(bContext *C)
{
  return BKE_blender_project_read_callback(
      CTX_data_main(C), [](const bke::BlenderProject *project) {
        return project && !project->get_splash_image_path().is_empty();
      });
}

static wmOperatorStatus project_splash_image_clear_exec(bContext *C, wmOperator * /*op*/)
{
  BKE_blender_project_write_callback(CTX_data_main(C), [](bke::BlenderProject *project) {
    if (project) {
      project->set_splash_image_path("");
    }
  });

  WM_event_add_notifier(C, NC_WINDOW, nullptr);
  return OPERATOR_FINISHED;
}

void PROJECT_OT_splash_image_clear(wmOperatorType *ot)
{
  ot->name = "Clear Splash Artwork";
  ot->description =
      "Clear the project custom splash artwork.\n"
      "The image file is not deleted from disk";
  ot->idname = "PROJECT_OT_splash_image_clear";

  ot->poll = project_splash_image_clear_poll;
  ot->exec = project_splash_image_clear_exec;

  ot->flag = OPTYPE_INTERNAL;
}

/** \} */

}  // namespace blender
