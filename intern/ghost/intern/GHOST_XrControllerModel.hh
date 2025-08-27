/* SPDX-FileCopyrightText: 2021-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

/* NOTE: Requires OpenXR headers to be included before this one for OpenXR types (XrInstance,
 * XrSession, etc.). */

#pragma once

#include <atomic>
#include <future>
#include <vector>

struct GHOST_XrControllerModelNode;

/**
 * OpenXR glTF controller model.
 */
class GHOST_XrControllerModel {
 public:
  GHOST_XrControllerModel(XrInstance instance, const char *subaction_path);
  ~GHOST_XrControllerModel();

  void load(XrSession session);
  void updateComponents(XrSession session);
  void getData(GHOST_XrControllerModelData &r_data);

 private:
  XrPath subaction_path_ = XR_NULL_PATH;
  XrControllerModelKeyMSFT model_key_ = XR_NULL_CONTROLLER_MODEL_KEY_MSFT;

  std::future<void> load_task_;
  std::atomic<bool> data_loaded_ = false;

  std::vector<GHOST_XrControllerModelVertex> vertices_;
  std::vector<uint32_t> indices_;
  std::vector<GHOST_XrControllerModelComponent> components_;
  std::vector<GHOST_XrControllerModelNode> nodes_;
  /** Maps node states to nodes. */
  std::vector<int32_t> node_state_indices_;

  void loadControllerModel(XrSession session);
};
