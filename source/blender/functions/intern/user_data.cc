/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "FN_user_data.hh"

namespace blender::fn {

destruct_ptr<LocalUserData> UserData::get_local(LinearAllocator<> & /*allocator*/)
{
  return {};
}

}  // namespace blender::fn
