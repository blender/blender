/* SPDX-FileCopyrightText: 2002-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */
#pragma once

class GHOST_TaskBarX11 {
 public:
  static bool init();
  static void free();

  GHOST_TaskBarX11(const char *name);

  bool is_valid();
  void set_progress(double progress);
  void set_progress_enabled(bool enabled);

 private:
  void *handle;
};
