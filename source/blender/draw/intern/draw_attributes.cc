/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "GPU_shader.hh"

#include "draw_attributes.hh"

namespace blender::draw {

void drw_attributes_merge(VectorSet<std::string> *dst, const VectorSet<std::string> *src)
{
  dst->add_multiple(src->as_span());
}

bool drw_attributes_overlap(const VectorSet<std::string> *a, const VectorSet<std::string> *b)
{
  for (const std::string &req : b->as_span()) {
    if (!a->contains(req)) {
      return false;
    }
  }

  return true;
}

void drw_attributes_add_request(VectorSet<std::string> *attrs, const StringRef name)
{
  if (attrs->size() >= GPU_MAX_ATTR) {
    return;
  }
  attrs->add_as(name);
}

}  // namespace blender::draw
