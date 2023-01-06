/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

namespace blender::realtime_compositor {

/* -------------------------------------------------------------------------------------------------
 * Cached Resource.
 *
 * A cached resource is any resource that can be cached across compositor evaluations and across
 * multiple operations. Cached resources are managed by an instance of a StaticCacheManager and are
 * freed when they are no longer needed, a state which is represented by the `needed` member in the
 * class. For more information on the caching mechanism, see the StaticCacheManager class.
 *
 * To add a new cached resource:
 *
 * - Create a derived class from CachedResource to represent the resource.
 * - Create a key class that can be used in a Map to identify the resource.
 * - Add a new Map to StaticCacheManager mapping the key to the resource.
 * - Reset the contents of the added map in StaticCacheManager::reset.
 * - Add an appropriate getter method in StaticCacheManager.
 *
 * See the existing cached resources for reference. */
class CachedResource {
 public:
  /* A flag that represents the needed status of the cached resource. See the StaticCacheManager
   * class for more information on how this member is utilized in the caching mechanism. */
  bool needed = true;
};

}  // namespace blender::realtime_compositor
