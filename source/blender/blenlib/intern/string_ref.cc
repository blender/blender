/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_string_ref.hh"
#include "BLI_string_utf8.hh"

#include <ostream>

namespace blender {

std::ostream &operator<<(std::ostream &stream, StringRef ref)
{
  stream << std::string(ref);
  return stream;
}

std::ostream &operator<<(std::ostream &stream, StringRefNull ref)
{
  stream << std::string(ref.data(), size_t(ref.size()));
  return stream;
}

void StringRefBase::copy_utf8_truncated(MutableCStr dst) const
{
  /* Destination must at least hold the null terminator. */
  BLI_assert(dst.size() >= 1);
  /* The current #StringRef is assumed to contain valid UTF8. */
  BLI_assert(!data_ || BLI_str_utf8_invalid_byte(data_, size_) == -1);

  /* Common case when the string can just be copied over entirely. */
  if (size_ < dst.size()) {
    this->copy_unsafe(dst.data());
    return;
  }

  const int64_t max_copy_num_without_terminator = std::min(size_, dst.size() - 1);
  const size_t new_len = BLI_strncpy_utf8_rlen_unterminated(
      dst.take_front(max_copy_num_without_terminator), data_);
  dst[new_len] = '\0';
}

void StringRefBase::copy_bytes_truncated(MutableCStr dst) const
{
  /* Destination must at least hold the null terminator. */
  BLI_assert(dst.size() >= 1);

  /* Common case when the string can just be copied over entirely. */
  if (size_ < dst.size()) {
    this->copy_unsafe(dst.data());
    return;
  }

  const int64_t new_len = std::min(size_, dst.size() - 1);
  memcpy(dst.data(), data_, new_len);
  dst[new_len] = '\0';
}

}  // namespace blender
