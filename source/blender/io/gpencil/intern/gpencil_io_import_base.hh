/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation */
#pragma once

/** \file
 * \ingroup bgpencil
 */
#include "gpencil_io_base.hh"

namespace blender::io::gpencil {

class GpencilImporter : public GpencilIO {

 public:
  GpencilImporter(const struct GpencilIOParams *iparams);
  virtual bool read() = 0;

 protected:
  struct Object *create_object();
  int32_t create_material(const char *name, bool stroke, bool fill);

 private:
};

}  // namespace blender::io::gpencil
