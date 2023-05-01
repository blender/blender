/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

namespace blender::realtime_compositor {

/* -------------------------------------------------------------------------------------------------
 * Cached Resource.
 *
 * A cached resource is any resource that can be cached across compositor evaluations and across
 * multiple operations. Cached resources are managed by an instance of a StaticCacheManager, stored
 * in an instance of a CachedResourceContainer, and are freed when they are no longer needed, a
 * state which is represented by the `needed` member in the class. For more information on the
 * caching mechanism, see the StaticCacheManager class.
 *
 * To add a new cached resource:
 *
 * - Create a key class that can be used to identify the resource in a Map if needed.
 * - Create a derived class from CachedResource to represent the resource.
 * - Create a derived class from CachedResourceContainer to store the resources.
 * - Add an instance of the container to StaticCacheManager and call its reset method.
 *
 * See the existing cached resources for reference. */
class CachedResource {
 public:
  /* A flag that represents the needed status of the cached resource. See the StaticCacheManager
   * class for more information on how this member is utilized in the caching mechanism. */
  bool needed = true;
};

/* -------------------------------------------------------------------------------------------------
 * Cached Resource Container.
 *
 * A cached resource container stores all the cached resources for a specific cached resource type.
 * The cached resources are typically stored in a map identified by a key type. The reset method
 * should be implemented as described in StaticCacheManager::reset. An appropriate getter method
 * should be provided that properly sets the CachedResource::needed flag as described in the
 * description of the StaticCacheManager class.
 *
 * See the existing cached resources for reference. */
class CachedResourceContainer {
 public:
  /* Reset the container by deleting the cached resources that are no longer needed because they
   * weren't used in the last evaluation and prepare the remaining cached resources to track their
   * needed status in the next evaluation. See the description of the StaticCacheManager class for
   * more information. This should be called in StaticCacheManager::reset. */
  virtual void reset() = 0;
};

}  // namespace blender::realtime_compositor
