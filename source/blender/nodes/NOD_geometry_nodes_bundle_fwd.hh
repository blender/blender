/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_implicit_sharing_ptr.hh"

namespace blender::nodes {

class Bundle;
using BundlePtr = ImplicitSharingPtr<Bundle>;
struct BundleItemValue;

}  // namespace blender::nodes
