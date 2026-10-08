/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

namespace blender {
struct bNodeSocket;
}

namespace blender::nodes::eval_log {
class NodeTreeLogger;
}

namespace blender::compositor {

class Result;
class Context;

void log_result(Context &context,
                nodes::eval_log::NodeTreeLogger &logger,
                const bNodeSocket &socket,
                const Result &result);

}  // namespace blender::compositor
