/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bgpencil
 */
#include "gpencil_io_base.hh"

namespace blender::io::gpencil {

class GpencilImporter : public GpencilIO {

 public:
  GpencilImporter(const GpencilIOParams *iparams);
  virtual bool read() = 0;

 protected:
  Object *create_object();
  int32_t create_material(const char *name, bool stroke, bool fill);

 private:
};

}  // namespace blender::io::gpencil
