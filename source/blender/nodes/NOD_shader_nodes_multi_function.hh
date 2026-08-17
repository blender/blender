/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "FN_user_data.hh"

namespace blender::nodes {

class ShaderNodesMultiFunctionUserData : public fn::UserData {
 public:
  Vector<std::string> error_messages;
};

}  // namespace blender::nodes
