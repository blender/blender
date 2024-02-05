/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "engine.hh"

namespace blender::render::hydra {

class FinalEngine : public Engine {
 private:
  Map<std::string, pxr::TfToken> aov_tokens_;

 public:
  using Engine::Engine;

  void render() override;
  void set_render_setting(const std::string &key, const pxr::VtValue &val) override;

 protected:
  void notify_status(float progress, const std::string &title, const std::string &info) override;

 private:
  void update_render_result(int width, int height, const char *layer_name);
};

}  // namespace blender::render::hydra
