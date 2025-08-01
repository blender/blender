/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_implicit_sharing_ptr.hh"

namespace blender::nodes {

class List;
using ListPtr = ImplicitSharingPtr<List>;

}  // namespace blender::nodes
