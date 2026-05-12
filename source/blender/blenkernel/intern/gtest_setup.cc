/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_appdir.hh"
#include "BKE_brush.hh"
#include "BKE_callbacks.hh"
#include "BKE_cpp_types.hh"
#include "BKE_gtest_setup.hh"
#include "BKE_icons.hh"
#include "BKE_idtype.hh"
#include "BKE_material.hh"
#include "BKE_modifier.hh"
#include "BKE_node.hh"
#include "BKE_particle.h"
#include "BKE_shader_fx.hh"
#include "BKE_sound.hh"
#include "BKE_volume.hh"

#include "BLI_fftw.hh"
#include "BLI_task.h"
#include "BLI_threads.h"

#include "CLG_log.h"

#include "DEG_depsgraph.hh"

#include "DNA_genfile.h"

#include "FN_init.hh"

#include "IMB_cache.hh"
#include "IMB_imbuf.hh"

#include "MOV_util.hh"

#include "RE_engine.h"
#include "RE_texture.h"

#include "RNA_define.hh"

#include "SEQ_modifier.hh"

namespace blender::bke {

void gtest_setup()
{
  CLG_init();
  BLI_threadapi_init();
  DNA_sdna_current_init();
  BKE_cpp_types_init();
  fn::multi_function::register_common_functions();
  BKE_idtype_init();
  BKE_modifier_init();
  seq::modifiers_init();
  BKE_shaderfx_init();
  BKE_volumes_init();
  DEG_register_node_types();
  BKE_callback_global_init();
  BKE_appdir_init();
  BLI_task_scheduler_init();
  fftw::initialize_float();
  IMB_init();
  MOV_init();
  RNA_init();
  RE_texture_rng_init();
  RE_engines_init();
  bke::node_system_init();
  BKE_brush_system_init();
  BKE_particle_init_rng();
  BKE_sound_init_once();
  BKE_materials_init();
  IMB_cache_init();

  /* Value should be larger than #BIFICONID_LAST_STATIC which is not available here. */
  BKE_icons_init(4096);
}

void gtest_teardown()
{
  BKE_icons_free();
  BKE_materials_exit();
  bke::node_system_exit();
  RNA_exit();
  DNA_sdna_current_free();
  BLI_threadapi_exit();
  RE_texture_rng_exit();
  RE_engines_exit();
  BLI_task_scheduler_exit();
  BKE_brush_system_exit();
  BKE_sound_exit_once();
  BKE_appdir_exit();
  IMB_cache_destruct();
  IMB_exit();
  CLG_exit();
}

}  // namespace blender::bke
