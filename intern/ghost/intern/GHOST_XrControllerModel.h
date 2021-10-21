/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup GHOST
 */

/* Note: Requires OpenXR headers to be included before this one for OpenXR types (XrInstance,
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
  XrPath m_subaction_path = XR_NULL_PATH;
  XrControllerModelKeyMSFT m_model_key = XR_NULL_CONTROLLER_MODEL_KEY_MSFT;

  std::future<void> m_load_task;
  std::atomic<bool> m_data_loaded = false;

  std::vector<GHOST_XrControllerModelVertex> m_vertices;
  std::vector<uint32_t> m_indices;
  std::vector<GHOST_XrControllerModelComponent> m_components;
  std::vector<GHOST_XrControllerModelNode> m_nodes;
  /** Maps node states to nodes. */
  std::vector<int32_t> m_node_state_indices;

  void loadControllerModel(XrSession session);
};
