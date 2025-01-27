/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_denoised_auxiliary_pass.hh"

namespace blender::compositor {

/* -------------------------------------------------------------------------------------------------
 * Derived Resources.
 *
 * Derived resources are resources that are computed from a particular result, stored in it, and
 * freed when the result is freed. The same resources might be needed by multiple operations, so
 * caching them on the result will improve performance at the cost of higher memory usage.
 *
 * The DerivedResources class stores instances of the container classes that store derived
 * resources. This is very similar in design to the StaticCacheManager, see its description for
 * more information. Destroying an instance of this class is expected to destroy all derived
 * resources in it.
 *
 * To add a new derived resource:
 *
 * - Create a key class that can be used to identify the resource in a Map if needed.
 * - Create a resource class to compute and store the resource.
 * - Create a container class to store the resources in a map identified by their keys.
 * - Add an instance of the container to the DerivedResources class.
 *
 * See the existing derived resources for reference. */
class DerivedResources {
 public:
  DenoisedAuxiliaryPassContainer denoised_auxiliary_passes;
};

}  // namespace blender::compositor
