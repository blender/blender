/* SPDX-FileCopyrightText: 2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "session/cache_eviction.h"

#include <chrono>

#include "util/math_base.h"
#include "util/time.h"

CCL_NAMESPACE_BEGIN

CacheEvictionManager::CacheEvictionManager(bool background) : background_(background) {}

void CacheEvictionManager::reset()
{
  render_tile_count_ = 0;

  viewport_last_activity_ = 0.0;
  viewport_was_navigating_ = false;
  render_tile_count_ = 0;
}

void CacheEvictionManager::set_navigating(bool navigating)
{
  navigating_ = navigating;
}

bool CacheEvictionManager::is_navigating() const
{
  return (background_) ? false : navigating_;
}

bool CacheEvictionManager::need_eviction(bool idle, bool switched_to_new_tile)
{
  /* Final render. */
  if (background_) {
    /* Evict before rendering the next tile, except the first one where we
     * can't determine what was shared with other tiles. */
    if (idle || !switched_to_new_tile) {
      return false;
    }

    render_tile_count_++;
    return render_tile_count_ >= 2;
  }

  /* Viewport render. */
  const bool navigating = navigating_;
  if (navigating) {
    /* No eviction while navigating. */
    viewport_last_activity_ = 0.0;
  }
  else if (viewport_was_navigating_) {
    /* Start eviction timer when navigating stops. */
    viewport_last_activity_ = time_dt();
  }
  viewport_was_navigating_ = navigating;

  if (!idle) {
    /* Restart eviction timer while not idle. */
    viewport_last_activity_ = time_dt();
    return false;
  }

  if (wait_time(idle) != std::chrono::milliseconds::zero()) {
    /* Not ready to evict yet. */
    return false;
  }

  /* Eviction needed now, clear existing timer. */
  viewport_last_activity_ = 0.0;
  return true;
}

std::chrono::milliseconds CacheEvictionManager::wait_time(bool idle) const
{
  if (!idle || background_ || viewport_last_activity_ == 0.0 || viewport_was_navigating_) {
    /* No eviction pending. */
    return std::chrono::milliseconds::max();
  }

  /* Wait for VIEWPORT_EVICTION_DELAY after last activity. */
  const double elapsed = time_dt() - viewport_last_activity_;
  const double remaining = VIEWPORT_EVICTION_DELAY - elapsed;
  return std::chrono::milliseconds(int64_t(max(0.0, remaining) * 1000.0));
}

CCL_NAMESPACE_END
