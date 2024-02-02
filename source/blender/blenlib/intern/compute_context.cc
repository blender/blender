/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_compute_context.hh"
#include "BLI_hash_md5.hh"
#include <sstream>

namespace blender {

void ComputeContextHash::mix_in(const void *data, int64_t len)
{
  DynamicStackBuffer<> buffer_owner(HashSizeInBytes + len, 8);
  char *buffer = static_cast<char *>(buffer_owner.buffer());
  memcpy(buffer, this, HashSizeInBytes);
  memcpy(buffer + HashSizeInBytes, data, len);

  BLI_hash_md5_buffer(buffer, HashSizeInBytes + len, this);
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
