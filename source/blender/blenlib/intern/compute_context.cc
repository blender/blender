/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_compute_context.hh"
#include "BLI_stack.hh"

#include <sstream>
#include <xxhash.h>

namespace blender {

ComputeContextHash ComputeContextHash::from_bytes(const void *data, const int64_t len)
{
  ComputeContextHash final_hash;
  const XXH128_hash_t hash = XXH3_128bits(data, len);
  /* Cast to void * to avoid warnings. */
  memcpy(static_cast<void *>(&final_hash), &hash, sizeof(hash));
  static_assert(sizeof(ComputeContextHash) == sizeof(hash));
  return final_hash;
}

std::ostream &operator<<(std::ostream &stream, const ComputeContextHash &hash)
{
  std::stringstream ss;
  ss << "0x" << std::hex << hash.v1 << hash.v2;
  stream << ss.str();
  return stream;
}

void ComputeContext::print_stack(std::ostream &stream, StringRef name) const
{
  Stack<const ComputeContext *> stack;
  for (const ComputeContext *current = this; current; current = current->parent_) {
    stack.push(current);
  }
  stream << "Context Stack: " << name << "\n";
  while (!stack.is_empty()) {
    const ComputeContext *current = stack.pop();
    stream << "-> ";
    current->print_current_in_line(stream);
    const ComputeContextHash &current_hash = current->hash_;
    stream << " \t(hash: " << current_hash << ")\n";
  }
}

std::ostream &operator<<(std::ostream &stream, const ComputeContext &compute_context)
{
  compute_context.print_stack(stream, "");
  return stream;
}

}  // namespace blender
