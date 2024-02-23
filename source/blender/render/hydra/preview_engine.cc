/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "preview_engine.hh"

namespace blender::render::hydra {

void PreviewEngine::notify_status(float /*progress*/,
                                  const std::string & /*title*/,
                                  const std::string & /*info*/)
{
  /* Empty function. */
}

}  // namespace blender::render::hydra
