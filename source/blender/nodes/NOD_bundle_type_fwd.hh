/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>

namespace blender::nodes {
class FlatBundleType;
class NestedBundleType;
class BundleType;

using FlatBundleTypePtr = std::shared_ptr<const FlatBundleType>;
using NestedBundleTypePtr = std::shared_ptr<const NestedBundleType>;

}  // namespace blender::nodes
