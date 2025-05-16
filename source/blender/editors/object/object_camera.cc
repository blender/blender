/* SPDX-FileCopyrightText: 2001-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 */

#include "object_intern.hh"

#include "DNA_camera_types.h"

#include "BKE_context.hh"

#include "ED_object.hh"

#include "RE_engine.h"

#include "WM_api.hh"

namespace blender::ed::object {

/* -------------------------------------------------------------------- */
/** \name Custom Camera Update
 * \{ */

static bool object_camera_custom_update_poll(bContext *C)
{
  Object *ob = context_active_object(C);
  RenderEngineType *type = CTX_data_engine_type(C);

  /* Test if we have a render engine that supports custom cameras. */
  if (!(type && type->update_custom_camera)) {
    return false;
  }

  /* See if we have a custom camera in context. */
  if (ob == nullptr || ob->type != OB_CAMERA) {
    return false;
  }
  Camera *cam = static_cast<Camera *>(ob->data);
  if (cam == nullptr || cam->type != CAM_CUSTOM) {
    return false;
  }

  if (cam->custom_mode == CAM_CUSTOM_SHADER_EXTERNAL) {
    return cam->custom_filepath[0] != '\0';
  }
  else {
    return cam->custom_shader != nullptr;
  }
}

static wmOperatorStatus object_camera_custom_update_exec(bContext *C, wmOperator *op)
{
  Object *ob = context_active_object(C);
  RenderEngineType *type = CTX_data_engine_type(C);
  Camera *cam = static_cast<Camera *>(ob->data);

  /* setup render engine */
  RenderEngine *engine = RE_engine_create(type);
  engine->reports = op->reports;

  type->update_custom_camera(engine, cam);

  RE_engine_free(engine);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_camera_custom_update(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Custom Camera Update";
  ot->description = "Update custom camera with new parameters from the shader";
  ot->idname = "OBJECT_OT_camera_custom_update";

  /* API callbacks. */
  ot->exec = object_camera_custom_update_exec;
  ot->poll = object_camera_custom_update_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

}  // namespace blender::ed::object
