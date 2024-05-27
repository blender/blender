/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "vk_common.hh"

namespace blender::gpu {
class VKContext;
class VKCommandBuffers;

#if (defined(__GNUC__) && __GNUC__ >= 14 && !defined(__clang__))
#  pragma GCC diagnostic push
/* CPP20 compiler warnings in GCC14+.
 * Must be resolved before upgrading to a newer C++, avoid noisy warnings for now. */
#  pragma GCC diagnostic ignored "-Wtemplate-id-cdtor"
#endif

/**
 * In vulkan multiple commands can be in flight simultaneously.
 *
 * These commands can share the same resources like descriptor sets
 * or push constants. When between commands these resources are updated
 * a new version of these resources should be created.
 *
 * When a resource is updated it should check the submission id of the
 * command buffer. If it is different, then the resource can be reused.
 * If the submission id is the same a new version of the resource to now
 * intervene with other commands that uses the resource.
 *
 * VKSubmissionID is the identifier to keep track if a new submission is
 * being recorded.
 */
struct VKSubmissionID {
 private:
  int64_t id_ = -1;

 public:
  VKSubmissionID() = default;

 private:
  /**
   * Reset the submission id.
   *
   * This should only be called during initialization of the command buffer.
   * As it leads to undesired behavior after resources are already tracking
   * the submission id.
   */
  void reset()
  {
    id_ = 0;
  }

  /**
   * Change the submission id.
   *
   * Is called when submitting a command buffer to the queue. In this case resource
   * known that the next time it is used that it can free its sub resources used by
   * the previous submission.
   */
  void next()
  {
    id_++;
  }

 public:
  const VKSubmissionID &operator=(const VKSubmissionID &other)
  {
    id_ = other.id_;
    return *this;
  }

  bool operator==(const VKSubmissionID &other)
  {
    return id_ == other.id_;
  }

  bool operator!=(const VKSubmissionID &other)
  {
    return id_ != other.id_;
  }

  friend class VKCommandBuffers;
};

/**
 * Submission tracker keeps track of the last known submission id of the
 * command buffer.
 */
class VKSubmissionTracker {
  VKSubmissionID last_known_id_;

 public:
  /**
   * Check if the submission_id has changed since the last time it was called
   * on this VKSubmissionTracker.
   */
  bool is_changed(VKContext &context);
};

/**
 * VKResourceTracker will keep track of resources.
 */
template<typename Resource> class VKResourceTracker : NonCopyable {
  VKSubmissionTracker submission_tracker_;
  Vector<std::unique_ptr<Resource>> tracked_resources_;

 protected:
  VKResourceTracker<Resource>() = default;
  VKResourceTracker<Resource>(VKResourceTracker<Resource> &&other)
      : submission_tracker_(other.submission_tracker_),
        tracked_resources_(std::move(other.tracked_resources_))
  {
  }

  VKResourceTracker<Resource> &operator=(VKResourceTracker<Resource> &&other)
  {
    submission_tracker_ = other.submission_tracker_;
    tracked_resources_ = std::move(other.tracked_resources_);
    return *this;
  }

  virtual ~VKResourceTracker()
  {
    free_tracked_resources();
  }

  /**
   * Get a resource what can be used by the resource tracker.
   *
   * When a different submission was detected all previous resources
   * will be freed and a new resource will be returned.
   *
   * When still in the same submission and we need to update the resource
   * (is_dirty=true) then a new resource will be returned. Otherwise
   * the previous used resource will be used.
   *
   * When no resources exists, a new resource will be created.
   *
   * The resource given back is owned by this resource tracker. And
   * the resource should not be stored outside this class as it might
   * be destroyed when the next submission is detected.
   */
  std::unique_ptr<Resource> &tracked_resource_for(VKContext &context, const bool is_dirty)
  {
    if (submission_tracker_.is_changed(context)) {
      free_tracked_resources();
      tracked_resources_.append(create_resource(context));
    }
    else if (is_dirty || tracked_resources_.is_empty()) {
      tracked_resources_.append(create_resource(context));
    }
    return active_resource();
  }

  /**
   * Callback to create a new resource. Can be called by the `tracked_resource_for` method.
   */
  virtual std::unique_ptr<Resource> create_resource(VKContext &context) = 0;

  /**
   * Does this instance have an active resource.
   */
  bool has_active_resource()
  {
    return !tracked_resources_.is_empty();
  }

  /**
   * Return the active resource of the tracker.
   */
  std::unique_ptr<Resource> &active_resource()
  {
    BLI_assert(!tracked_resources_.is_empty());
    return tracked_resources_.last();
  }

 private:
  void free_tracked_resources()
  {
    tracked_resources_.clear();
  }
};

#if (defined(__GNUC__) && __GNUC__ >= 14 && !defined(__clang__))
#  pragma GCC diagnostic pop
#endif

}  // namespace blender::gpu
