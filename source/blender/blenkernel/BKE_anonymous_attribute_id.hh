/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <atomic>

#include "BLI_set.hh"
#include "BLI_string_ref.hh"
#include "BLI_user_counter.hh"

namespace blender::bke {

/**
 * An #AnonymousAttributeID contains information about a specific anonymous attribute.
 * Like normal attributes, anonymous attributes are also identified by their name, so one should
 * not have to compare #AnonymousAttributeID pointers.
 *
 * Anonymous attributes don't need additional information besides their name, with a few
 * exceptions:
 * - The name of anonymous attributes is generated automatically, so it is generally not human
 *   readable (just random characters). #AnonymousAttributeID can provide more context as where a
 *   specific anonymous attribute was created which can simplify debugging.
 * - [Not yet supported.] When anonymous attributes are contained in on-disk caches, we have to map
 *   those back to anonymous attributes at run-time. The issue is that (for various reasons) we
 *   might change how anonymous attribute names are generated in the future, which would lead to a
 *   mis-match between stored and new attribute names. To work around it, we should cache
 *   additional information for anonymous attributes on disk (like which node created it). This
 *   information can then be used to map stored attributes to their run-time counterpart.
 *
 * Once created, #AnonymousAttributeID is immutable. Also it is intrinsically reference counted so
 * that it can have shared ownership. `std::shared_ptr` can't be used for that purpose here,
 * because that is not available in C code. If possible, the #AutoAnonymousAttributeID wrapper
 * should be used to avoid manual reference counting in C++ code.
 */
class AnonymousAttributeID {
 private:
  mutable std::atomic<int> users_ = 1;

 protected:
  std::string name_;

 public:
  virtual ~AnonymousAttributeID() = default;

  StringRefNull name() const
  {
    return name_;
  }

  virtual std::string user_name() const;

  void user_add() const
  {
    users_.fetch_add(1);
  }

  void user_remove() const
  {
    const int new_users = users_.fetch_sub(1) - 1;
    if (new_users == 0) {
      MEM_delete(this);
    }
  }
};

/** Wrapper for #AnonymousAttributeID that avoids manual reference counting. */
using AutoAnonymousAttributeID = UserCounter<const AnonymousAttributeID>;

/**
 * A set of anonymous attribute names that is passed around in geometry nodes.
 */
class AnonymousAttributeSet {
 public:
  /**
   * This uses `std::shared_ptr` because attributes sets are passed around by value during geometry
   * nodes evaluation, and this makes it very small if there is no name. Also it makes copying very
   * cheap.
   */
  std::shared_ptr<Set<std::string>> names;
};

/**
 * Can be passed to algorithms which propagate attributes. It can tell the algorithm which
 * anonymous attributes should be propagated and can be skipped.
 */
class AnonymousAttributePropagationInfo {
 public:
  /**
   * This uses `std::shared_ptr` because it's usually initialized from an #AnonymousAttributeSet
   * and then the set doesn't have to be copied.
   */
  std::shared_ptr<Set<std::string>> names;

  /**
   * Propagate all anonymous attributes even if the set above is empty.
   */
  bool propagate_all = true;

  /**
   * Return true when the anonymous attribute should be propagated and false otherwise.
   */
  bool propagate(const AnonymousAttributeID &anonymous_id) const;
};

}  // namespace blender::bke
