/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdio>

#include "COM_BufferOperation.h"
#include "COM_ExecutionSystem.h"
#include "COM_ReadBufferOperation.h"

#include "COM_NodeOperation.h" /* own include */

namespace blender::compositor {

/*******************
 **** NodeOperation ****
 *******************/

NodeOperation::NodeOperation()
{
  canvas_input_index_ = 0;
  canvas_ = COM_AREA_NONE;
  btree_ = nullptr;
}

float NodeOperation::get_constant_value_default(float default_value)
{
  BLI_assert(outputs_.size() > 0 && get_output_socket()->get_data_type() == DataType::Value);
  return *get_constant_elem_default(&default_value);
}

const float *NodeOperation::get_constant_elem_default(const float *default_elem)
{
  BLI_assert(outputs_.size() > 0);
  if (get_flags().is_constant_operation) {
    return static_cast<ConstantOperation *>(this)->get_constant_elem();
  }

  return default_elem;
}

std::optional<NodeOperationHash> NodeOperation::generate_hash()
{
  params_hash_ = get_default_hash_2(canvas_.xmin, canvas_.xmax);

  /* Hash subclasses params. */
  is_hash_output_params_implemented_ = true;
  hash_output_params();
  if (!is_hash_output_params_implemented_) {
    return std::nullopt;
  }

  hash_params(canvas_.ymin, canvas_.ymax);
  if (outputs_.size() > 0) {
    BLI_assert(outputs_.size() == 1);
    hash_param(this->get_output_socket()->get_data_type());
  }
  NodeOperationHash hash;
  hash.params_hash_ = params_hash_;

  hash.parents_hash_ = 0;
  for (NodeOperationInput &socket : inputs_) {
    if (!socket.is_connected()) {
      continue;
    }

    NodeOperation &input = socket.get_link()->get_operation();
    const bool is_constant = input.get_flags().is_constant_operation;
    combine_hashes(hash.parents_hash_, get_default_hash(is_constant));
    if (is_constant) {
      const float *elem = ((ConstantOperation *)&input)->get_constant_elem();
      const int num_channels = COM_data_type_num_channels(socket.get_data_type());
      for (const int i : IndexRange(num_channels)) {
        combine_hashes(hash.parents_hash_, get_default_hash(elem[i]));
      }
    }
    else {
      combine_hashes(hash.parents_hash_, get_default_hash(input.get_id()));
    }
  }

  hash.type_hash_ = typeid(*this).hash_code();
  hash.operation_ = this;

  return hash;
}

NodeOperationOutput *NodeOperation::get_output_socket(uint index)
{
  return &outputs_[index];
}

NodeOperationInput *NodeOperation::get_input_socket(uint index)
{
  return &inputs_[index];
}

void NodeOperation::add_input_socket(DataType datatype, ResizeMode resize_mode)
{
  inputs_.append(NodeOperationInput(this, datatype, resize_mode));
}

void NodeOperation::add_output_socket(DataType datatype)
{
  outputs_.append(NodeOperationOutput(this, datatype));
}

void NodeOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  uint used_canvas_index = 0;
  if (canvas_input_index_ == RESOLUTION_INPUT_ANY) {
    for (NodeOperationInput &input : inputs_) {
      rcti any_area = COM_AREA_NONE;
      const bool determined = input.determine_canvas(preferred_area, any_area);
      if (determined) {
        r_area = any_area;
        break;
      }
      used_canvas_index += 1;
    }
  }
  else if (canvas_input_index_ < inputs_.size()) {
    NodeOperationInput &input = inputs_[canvas_input_index_];
    input.determine_canvas(preferred_area, r_area);
    used_canvas_index = canvas_input_index_;
  }

  if (modify_determined_canvas_fn_) {
    modify_determined_canvas_fn_(r_area);
  }

  rcti unused_area = COM_AREA_NONE;
  const rcti &local_preferred_area = r_area;
  for (uint index = 0; index < inputs_.size(); index++) {
    if (index == used_canvas_index) {
      continue;
    }
    NodeOperationInput &input = inputs_[index];
    if (input.is_connected()) {
      input.determine_canvas(local_preferred_area, unused_area);
    }
  }
}

void NodeOperation::set_canvas_input_index(uint index)
{
  this->canvas_input_index_ = index;
}

void NodeOperation::init_data()
{
  /* Pass. */
}
void NodeOperation::init_execution()
{
  /* pass */
}

void NodeOperation::init_mutex()
{
  BLI_mutex_init(&mutex_);
}

void NodeOperation::lock_mutex()
{
  BLI_mutex_lock(&mutex_);
}

void NodeOperation::unlock_mutex()
{
  BLI_mutex_unlock(&mutex_);
}

void NodeOperation::deinit_mutex()
{
  BLI_mutex_end(&mutex_);
}

void NodeOperation::deinit_execution()
{
  /* pass */
}

void NodeOperation::set_canvas(const rcti &canvas_area)
{
  canvas_ = canvas_area;
  flags_.is_canvas_set = true;
}

const rcti &NodeOperation::get_canvas() const
{
  return canvas_;
}

void NodeOperation::unset_canvas()
{
  BLI_assert(inputs_.size() == 0);
  flags_.is_canvas_set = false;
}

SocketReader *NodeOperation::get_input_socket_reader(uint index)
{
  return this->get_input_socket(index)->get_reader();
}

NodeOperation *NodeOperation::get_input_operation(int index)
{
  NodeOperationInput *input = get_input_socket(index);
  if (input && input->is_connected()) {
    return &input->get_link()->get_operation();
  }

  return nullptr;
}

bool NodeOperation::determine_depending_area_of_interest(rcti *input,
                                                         ReadBufferOperation *read_operation,
                                                         rcti *output)
{
  if (inputs_.size() == 0) {
    BLI_rcti_init(output, input->xmin, input->xmax, input->ymin, input->ymax);
    return false;
  }

  rcti temp_output;
  bool first = true;
  for (int i = 0; i < get_number_of_input_sockets(); i++) {
    NodeOperation *input_operation = this->get_input_operation(i);
    if (input_operation &&
        input_operation->determine_depending_area_of_interest(input, read_operation, &temp_output))
    {
      if (first) {
        output->xmin = temp_output.xmin;
        output->ymin = temp_output.ymin;
        output->xmax = temp_output.xmax;
        output->ymax = temp_output.ymax;
        first = false;
      }
      else {
        output->xmin = MIN2(output->xmin, temp_output.xmin);
        output->ymin = MIN2(output->ymin, temp_output.ymin);
        output->xmax = MAX2(output->xmax, temp_output.xmax);
        output->ymax = MAX2(output->ymax, temp_output.ymax);
      }
    }
  }
  return !first;
}

/* -------------------------------------------------------------------- */
/** \name Full Frame Methods
 * \{ */

void NodeOperation::get_area_of_interest(const int input_idx,
                                         const rcti &output_area,
                                         rcti &r_input_area)
{
  if (get_flags().is_fullframe_operation) {
    r_input_area = output_area;
  }
  else {
    /* Non full-frame operations never implement this method. To ensure correctness assume
     * whole area is used. */
    NodeOperation *input_op = get_input_operation(input_idx);
    r_input_area = input_op->get_canvas();
  }
}

void NodeOperation::get_area_of_interest(NodeOperation *input_op,
                                         const rcti &output_area,
                                         rcti &r_input_area)
{
  for (int i = 0; i < get_number_of_input_sockets(); i++) {
    if (input_op == get_input_operation(i)) {
      get_area_of_interest(i, output_area, r_input_area);
      return;
    }
  }
  BLI_assert_msg(0, "input_op is not an input operation.");
}

void NodeOperation::render(MemoryBuffer *output_buf,
                           Span<rcti> areas,
                           Span<MemoryBuffer *> inputs_bufs)
{
  if (get_flags().is_fullframe_operation) {
    render_full_frame(output_buf, areas, inputs_bufs);
  }
  else {
    render_full_frame_fallback(output_buf, areas, inputs_bufs);
  }
}

void NodeOperation::render_full_frame(MemoryBuffer *output_buf,
                                      Span<rcti> areas,
                                      Span<MemoryBuffer *> inputs_bufs)
{
  init_execution();
  for (const rcti &area : areas) {
    update_memory_buffer(output_buf, area, inputs_bufs);
  }
  deinit_execution();
}

void NodeOperation::render_full_frame_fallback(MemoryBuffer *output_buf,
                                               Span<rcti> areas,
                                               Span<MemoryBuffer *> inputs_bufs)
{
  Vector<NodeOperationOutput *> orig_input_links = replace_inputs_with_buffers(inputs_bufs);

  init_execution();
  const bool is_output_operation = get_number_of_output_sockets() == 0;
  if (!is_output_operation && output_buf->is_a_single_elem()) {
    float *output_elem = output_buf->get_elem(0, 0);
    read_sampled(output_elem, 0, 0, PixelSampler::Nearest);
  }
  else {
    for (const rcti &rect : areas) {
      exec_system_->execute_work(rect, [=](const rcti &split_rect) {
        rcti tile_rect = split_rect;
        if (is_output_operation) {
          execute_region(&tile_rect, 0);
        }
        else {
          render_tile(output_buf, &tile_rect);
        }
      });
    }
  }
  deinit_execution();

  remove_buffers_and_restore_original_inputs(orig_input_links);
}

void NodeOperation::render_tile(MemoryBuffer *output_buf, rcti *tile_rect)
{
  const bool is_complex = get_flags().complex;
  void *tile_data = is_complex ? initialize_tile_data(tile_rect) : nullptr;
  const int elem_stride = output_buf->elem_stride;
  for (int y = tile_rect->ymin; y < tile_rect->ymax; y++) {
    float *output_elem = output_buf->get_elem(tile_rect->xmin, y);
    if (is_complex) {
      for (int x = tile_rect->xmin; x < tile_rect->xmax; x++) {
        read(output_elem, x, y, tile_data);
        output_elem += elem_stride;
      }
    }
    else {
      for (int x = tile_rect->xmin; x < tile_rect->xmax; x++) {
        read_sampled(output_elem, x, y, PixelSampler::Nearest);
        output_elem += elem_stride;
      }
    }
  }
  if (tile_data) {
    deinitialize_tile_data(tile_rect, tile_data);
  }
}

Vector<NodeOperationOutput *> NodeOperation::replace_inputs_with_buffers(
    Span<MemoryBuffer *> inputs_bufs)
{
  BLI_assert(inputs_bufs.size() == get_number_of_input_sockets());
  Vector<NodeOperationOutput *> orig_links(inputs_bufs.size());
  for (int i = 0; i < inputs_bufs.size(); i++) {
    NodeOperationInput *input_socket = get_input_socket(i);
    BufferOperation *buffer_op = new BufferOperation(inputs_bufs[i],
                                                     input_socket->get_data_type());
    orig_links[i] = input_socket->get_link();
    input_socket->set_link(buffer_op->get_output_socket());
    buffer_op->init_execution();
  }
  return orig_links;
}

void NodeOperation::remove_buffers_and_restore_original_inputs(
    Span<NodeOperationOutput *> original_inputs_links)
{
  BLI_assert(original_inputs_links.size() == get_number_of_input_sockets());
  for (int i = 0; i < original_inputs_links.size(); i++) {
    NodeOperation *buffer_op = get_input_operation(i);
    BLI_assert(buffer_op != nullptr);
    BLI_assert(typeid(*buffer_op) == typeid(BufferOperation));
    buffer_op->deinit_execution();
    NodeOperationInput *input_socket = get_input_socket(i);
    input_socket->set_link(original_inputs_links[i]);
    delete buffer_op;
  }
}

/** \} */

/*****************
 **** OpInput ****
 *****************/

NodeOperationInput::NodeOperationInput(NodeOperation *op,
                                       DataType datatype,
                                       ResizeMode resize_mode)
    : operation_(op), datatype_(datatype), resize_mode_(resize_mode), link_(nullptr)
{
}

SocketReader *NodeOperationInput::get_reader()
{
  if (is_connected()) {
    return &link_->get_operation();
  }

  return nullptr;
}

bool NodeOperationInput::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  if (link_) {
    link_->determine_canvas(preferred_area, r_area);
    return !BLI_rcti_is_empty(&r_area);
  }
  return false;
}

/******************
 **** OpOutput ****
 ******************/

NodeOperationOutput::NodeOperationOutput(NodeOperation *op, DataType datatype)
    : operation_(op), datatype_(datatype)
{
}

void NodeOperationOutput::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  NodeOperation &operation = get_operation();
  if (operation.get_flags().is_canvas_set) {
    r_area = operation.get_canvas();
  }
  else {
    operation.determine_canvas(preferred_area, r_area);
    if (!BLI_rcti_is_empty(&r_area)) {
      operation.set_canvas(r_area);
    }
  }
}

std::ostream &operator<<(std::ostream &os, const NodeOperationFlags &node_operation_flags)
{
  if (node_operation_flags.complex) {
    os << "complex,";
  }
  if (node_operation_flags.open_cl) {
    os << "open_cl,";
  }
  if (node_operation_flags.single_threaded) {
    os << "single_threaded,";
  }
  if (node_operation_flags.use_render_border) {
    os << "render_border,";
  }
  if (node_operation_flags.use_viewer_border) {
    os << "view_border,";
  }
  if (node_operation_flags.is_canvas_set) {
    os << "canvas_set,";
  }
  if (node_operation_flags.is_set_operation) {
    os << "set_operation,";
  }
  if (node_operation_flags.is_write_buffer_operation) {
    os << "write_buffer,";
  }
  if (node_operation_flags.is_read_buffer_operation) {
    os << "read_buffer,";
  }
  if (node_operation_flags.is_proxy_operation) {
    os << "proxy,";
  }
  if (node_operation_flags.is_viewer_operation) {
    os << "viewer,";
  }
  if (node_operation_flags.is_preview_operation) {
    os << "preview,";
  }
  if (!node_operation_flags.use_datatype_conversion) {
    os << "no_conversion,";
  }
  if (node_operation_flags.is_fullframe_operation) {
    os << "full_frame,";
  }
  if (node_operation_flags.is_constant_operation) {
    os << "contant_operation,";
  }
  if (node_operation_flags.can_be_constant) {
    os << "can_be_constant,";
  }

  return os;
}

std::ostream &operator<<(std::ostream &os, const NodeOperation &node_operation)
{
  NodeOperationFlags flags = node_operation.get_flags();
  os << "NodeOperation(";
  os << "id=" << node_operation.get_id();
  if (!node_operation.get_name().empty()) {
    os << ",name=" << node_operation.get_name();
  }
  os << ",flags={" << flags << "}";
  if (flags.is_read_buffer_operation) {
    const ReadBufferOperation *read_operation = (const ReadBufferOperation *)&node_operation;
    const MemoryProxy *proxy = read_operation->get_memory_proxy();
    if (proxy) {
      const WriteBufferOperation *write_operation = proxy->get_write_buffer_operation();
      if (write_operation) {
        os << ",write=" << (NodeOperation &)*write_operation;
      }
    }
  }
  os << ")";

  return os;
}

}  // namespace blender::compositor
