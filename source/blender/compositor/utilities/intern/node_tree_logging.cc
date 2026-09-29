/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_node_types.h"

#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "NOD_eval_log.hh"

#include "COM_bundle_item.hh"
#include "COM_result.hh"
#include "COM_utilities_node_tree_logging.hh"

namespace blender::compositor {

static destruct_ptr<nodes::eval_log::ImageInfoLog> get_image_info_log(LinearAllocator<> *allocator,
                                                                      const Result &result)
{
  const Domain &domain = result.domain();
  return allocator->construct<nodes::eval_log::ImageInfoLog>(
      domain.data_size,
      domain.display_size,
      domain.data_offset,
      domain.transformation,
      to_string(domain.realization_options.interpolation),
      to_string(domain.realization_options.extension_x),
      to_string(domain.realization_options.extension_y),
      to_string(result.precision()));
}

static destruct_ptr<nodes::eval_log::BundleValueLog> get_bundle_info_log(
    Context &context, LinearAllocator<> *allocator, const Result &result)
{
  Vector<nodes::eval_log::BundleValueLog::Item> items;
  for (const auto &item : result.get_single_value<nodes::BundlePtr>()->items()) {
    Result bundle_result = BundleItem::get_result(context, item.value);
    BLI_SCOPED_DEFER([&]() { bundle_result.release(); });
    items.append({item.key.ustr(), {Result::type_name(bundle_result.type())}});
  }
  return allocator->construct<nodes::eval_log::BundleValueLog>(std::move(items));
}

void log_result(Context &context,
                nodes::eval_log::NodeTreeLogger &logger,
                const bNodeSocket &socket,
                const Result &result)
{
  if (!result.is_allocated()) {
    return;
  }

  auto &socket_values = socket.in_out == SOCK_IN ? logger.input_socket_values :
                                                   logger.output_socket_values;

  if (!result.is_single_value()) {
    socket_values.append(*logger.allocator,
                         {socket.owner_node().identifier,
                          socket.index(),
                          get_image_info_log(logger.allocator, result)});
    return;
  }

  if (result.type() != ResultType::Bundle) {
    logger.log_value(socket.owner_node(), socket, result.single_value());
    return;
  }

  socket_values.append(*logger.allocator,
                       {socket.owner_node().identifier,
                        socket.index(),
                        get_bundle_info_log(context, logger.allocator, result)});
}

}  // namespace blender::compositor
