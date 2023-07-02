/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup select
 */

#include "DRW_render.h"

#include "GPU_capabilities.h"

#include "select_engine.h"

#include "../overlay/overlay_next_instance.hh"
#include "select_instance.hh"

using namespace blender::draw;

/* -------------------------------------------------------------------- */
/** \name Select-Next Engine
 * \{ */

using Instance = overlay::Instance;

struct SELECT_NextData {
  void *engine_type;
  DRWViewportEmptyList *fbl;
  DRWViewportEmptyList *txl;
  DRWViewportEmptyList *psl;
  DRWViewportEmptyList *stl;

  Instance *instance;
};

static void SELECT_next_engine_init(void *vedata)
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }

  OVERLAY_Data *ved = reinterpret_cast<OVERLAY_Data *>(vedata);

  if (ved->instance == nullptr) {
    ved->instance = new Instance(select::SelectionType::ENABLED);
  }

  reinterpret_cast<Instance *>(ved->instance)->init();
}

static void SELECT_next_cache_init(void *vedata)
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }
  reinterpret_cast<Instance *>(reinterpret_cast<OVERLAY_Data *>(vedata)->instance)->begin_sync();
}

static void SELECT_next_cache_populate(void *vedata, Object *object)
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }
  ObjectRef ref;
  ref.object = object;
  ref.dupli_object = DRW_object_get_dupli(object);
  ref.dupli_parent = DRW_object_get_dupli_parent(object);

  reinterpret_cast<Instance *>(reinterpret_cast<OVERLAY_Data *>(vedata)->instance)
      ->object_sync(ref, *DRW_manager_get());
}

static void SELECT_next_cache_finish(void *vedata)
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }
  reinterpret_cast<Instance *>(reinterpret_cast<OVERLAY_Data *>(vedata)->instance)->end_sync();
}

static void SELECT_next_draw_scene(void *vedata)
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }

  reinterpret_cast<Instance *>(reinterpret_cast<OVERLAY_Data *>(vedata)->instance)
      ->draw(*DRW_manager_get());
}

static void SELECT_next_instance_free(void *instance_)
{
  auto *instance = (Instance *)instance_;
  if (instance != nullptr) {
    delete instance;
  }
}

static const DrawEngineDataSize SELECT_next_data_size = DRW_VIEWPORT_DATA_SIZE(SELECT_NextData);

DrawEngineType draw_engine_select_next_type = {
    nullptr,
    nullptr,
    N_("Select-Next"),
    &SELECT_next_data_size,
    &SELECT_next_engine_init,
    nullptr,
    &SELECT_next_instance_free,
    &SELECT_next_cache_init,
    &SELECT_next_cache_populate,
    &SELECT_next_cache_finish,
    &SELECT_next_draw_scene,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

/** \} */
