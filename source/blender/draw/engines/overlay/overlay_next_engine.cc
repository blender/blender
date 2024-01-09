/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Engine for drawing a selection map where the pixels indicate the selection indices.
 */

#include "DRW_engine.hh"
#include "DRW_render.hh"

#include "DEG_depsgraph_query.hh"

#include "ED_view3d.hh"

#include "UI_interface.hh"

#include "BKE_duplilist.h"
#include "BKE_object.hh"
#include "BKE_paint.hh"

#include "GPU_capabilities.h"

#include "DNA_space_types.h"

#include "draw_manager.hh"
#include "overlay_next_instance.hh"

#include "overlay_engine.h"
#include "overlay_next_private.hh"

using namespace blender::draw;

using Instance = blender::draw::overlay::Instance;

/* -------------------------------------------------------------------- */
/** \name Engine Instance
 * \{ */

static void OVERLAY_next_engine_init(void *vedata)
{
  OVERLAY_Data *ved = reinterpret_cast<OVERLAY_Data *>(vedata);

  if (ved->instance == nullptr) {
    ved->instance = new Instance(select::SelectionType::DISABLED);
  }

  reinterpret_cast<Instance *>(ved->instance)->init();
}

static void OVERLAY_next_cache_init(void *vedata)
{
  reinterpret_cast<Instance *>(reinterpret_cast<OVERLAY_Data *>(vedata)->instance)->begin_sync();
}

static void OVERLAY_next_cache_populate(void *vedata, Object *object)
{
  ObjectRef ref;
  ref.object = object;
  ref.dupli_object = DRW_object_get_dupli(object);
  ref.dupli_parent = DRW_object_get_dupli_parent(object);

  reinterpret_cast<Instance *>(reinterpret_cast<OVERLAY_Data *>(vedata)->instance)
      ->object_sync(ref, *DRW_manager_get());
}

static void OVERLAY_next_cache_finish(void *vedata)
{
  reinterpret_cast<Instance *>(reinterpret_cast<OVERLAY_Data *>(vedata)->instance)->end_sync();
}

static void OVERLAY_next_draw_scene(void *vedata)
{
  reinterpret_cast<Instance *>(reinterpret_cast<OVERLAY_Data *>(vedata)->instance)
      ->draw(*DRW_manager_get());
}

static void OVERLAY_next_instance_free(void *instance_)
{
  Instance *instance = (Instance *)instance_;
  if (instance != nullptr) {
    delete instance;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Engine Type
 * \{ */

static const DrawEngineDataSize overlay_data_size = DRW_VIEWPORT_DATA_SIZE(OVERLAY_Data);

DrawEngineType draw_engine_overlay_next_type = {
    /*next*/ nullptr,
    /*prev*/ nullptr,
    /*idname*/ N_("Overlay"),
    /*vedata_size*/ &overlay_data_size,
    /*engine_init*/ &OVERLAY_next_engine_init,
    /*engine_free*/ nullptr,
    /*instance_free*/ &OVERLAY_next_instance_free,
    /*cache_init*/ &OVERLAY_next_cache_init,
    /*cache_populate*/ &OVERLAY_next_cache_populate,
    /*cache_finish*/ &OVERLAY_next_cache_finish,
    /*draw_scene*/ &OVERLAY_next_draw_scene,
    /*view_update*/ nullptr,
    /*id_update*/ nullptr,
    /*render_to_image*/ nullptr,
    /*store_metadata*/ nullptr,
};

/** \} */
