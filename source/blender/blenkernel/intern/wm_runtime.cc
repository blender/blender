/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_report.hh"
#include "BKE_wm_runtime.hh"

#include "BLI_ghash.h"
#include "BLI_listbase.h"

namespace blender::bke {

WindowManagerRuntime::WindowManagerRuntime()
{
  BKE_reports_init(&this->reports, RPT_STORE);
}

WindowManagerRuntime::~WindowManagerRuntime()
{
  BKE_reports_free(&this->reports);

  BLI_freelistN(&this->notifier_queue);
  if (this->notifier_queue_set) {
    BLI_gset_free(this->notifier_queue_set, nullptr);
  }
}

}  // namespace blender::bke
