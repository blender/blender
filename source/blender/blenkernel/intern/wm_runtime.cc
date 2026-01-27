/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_report.hh"
#include "BKE_undo_system.hh"
#include "BKE_wm_runtime.hh"

#include "BLI_ghash.h"
#include "BLI_listbase.h"

#include "WM_api.hh"

#include "WM_message.hh"

namespace blender::bke {

WindowManagerRuntime::WindowManagerRuntime()
{
  BKE_reports_init(&this->reports, RPT_STORE);
}

WindowManagerRuntime::~WindowManagerRuntime()
{
  BKE_reports_free(&this->reports);

  BLI_freelistN(&this->notifier_queue);

  while (wmOperator *op = static_cast<wmOperator *>(BLI_pophead(&this->operators))) {
    WM_operator_free(op);
  }

  BLI_freelistN(&this->paintcursors);

  /* NOTE(@ideasman42): typically timers are associated with windows and timers will have been
   * freed when the windows are removed. However timers can be created which don't have windows
   * and in this case it's necessary to free them on exit, see: #109953. */
  while (wmTimer *timer = static_cast<wmTimer *>(BLI_pophead(&this->timers))) {
    WM_event_timer_free_data(timer);
    MEM_delete(timer);
  }

  while (wmKeyConfig *keyconf = static_cast<wmKeyConfig *>(BLI_pophead(&this->keyconfigs))) {
    WM_keyconfig_free(keyconf);
  }

  WM_drag_free_list(&this->drags);

  if (this->undo_stack) {
    BKE_undosys_stack_destroy(this->undo_stack);
  }

  if (this->message_bus != nullptr) {
    WM_msgbus_destroy(this->message_bus);
  }
}

WindowRuntime::~WindowRuntime()
{
#ifdef WITH_INPUT_IME
  BLI_assert(this->ime_data == nullptr);
#endif
  /** The event_queue should be freed when the window is freed. */
  BLI_assert(BLI_listbase_is_empty(&this->event_queue));
}

}  // namespace blender::bke
