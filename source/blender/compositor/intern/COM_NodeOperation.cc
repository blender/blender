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
 *
 * Copyright 2011, Blender Foundation.
 */

#include <cstdio>
#include <memory>
#include <typeinfo>

#include "COM_BufferOperation.h"
#include "COM_ExecutionSystem.h"
#include "COM_ReadBufferOperation.h"
#include "COM_defines.h"

#include "COM_NodeOperation.h" /* own include */

namespace blender::compositor {

/*******************
 **** NodeOperation ****
 *******************/

NodeOperation::NodeOperation()
{
  canvas_input_index_ = 0;
  canvas_ = COM_AREA_NONE;
  this->m_btree = nullptr;
}

/** Get constant value when operation is constant, otherwise return default_value. */
float NodeOperation::get_constant_value_default(float default_value)
{
  BLI_assert(m_outputs.size() > 0 && getOutputSocket()->getDataType() == DataType::Value);
  return *get_constant_elem_default(&default_value);
}

/** Get constant elem when operation is constant, otherwise return default_elem. */
const float *NodeOperation::get_constant_elem_default(const float *default_elem)
{
  BLI_assert(m_outputs.size() > 0);
  if (get_flags().is_constant_operation) {
    return static_cast<ConstantOperation *>(this)->get_constant_elem();
  }

  return default_elem;
}

/**
 * Generate a hash that identifies the operation result in the current execution.
 * Requires `hash_output_params` to be implemented, otherwise `std::nullopt` is returned.
 * If the operation parameters or its linked inputs change, the hash must be re-generated.
 */
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
  if (m_outputs.size() > 0) {
    BLI_assert(m_outputs.size() == 1);
    hash_param(this->getOutputSocket()->getDataType());
  }
  NodeOperationHash hash;
  hash.params_hash_ = params_hash_;

  hash.parents_hash_ = 0;
  for (NodeOperationInput &socket : m_inputs) {
    if (!socket.isConnected()) {
      continue;
    }

    NodeOperation &input = socket.getLink()->getOperation();
    const bool is_constant = input.get_flags().is_constant_operation;
    combine_hashes(hash.parents_hash_, get_default_hash(is_constant));
    if (is_constant) {
      const float *elem = ((ConstantOperation *)&input)->get_constant_elem();
      const int num_channels = COM_data_type_num_channels(socket.getDataType());
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

NodeOperationOutput *NodeOperation::getOutputSocket(unsigned int index)
{
  return &m_outputs[index];
}

NodeOperationInput *NodeOperation::getInputSocket(unsigned int index)
{
  return &m_inputs[index];
}

void NodeOperation::addInputSocket(DataType datatype, ResizeMode resize_mode)
{
  m_inputs.append(NodeOperationInput(this, datatype, resize_mode));
}

void NodeOperation::addOutputSocket(DataType datatype)
{
  m_outputs.append(NodeOperationOutput(this, datatype));
}

void NodeOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  unsigned int used_canvas_index = 0;
  if (canvas_input_index_ == RESOLUTION_INPUT_ANY) {
    for (NodeOperationInput &input : m_inputs) {
      rcti any_area = COM_AREA_NONE;
      const bool determined = input.determine_canvas(preferred_area, any_area);
      if (determined) {
        r_area = any_area;
        break;
      }
      used_canvas_index += 1;
    }
  }
  else if (canvas_input_index_ < m_inputs.size()) {
    NodeOperationInput &input = m_inputs[canvas_input_index_];
    input.determine_canvas(preferred_area, r_area);
    used_canvas_index = canvas_input_index_;
  }

  if (modify_determined_canvas_fn_) {
    modify_determined_canvas_fn_(r_area);
  }

  rcti unused_area;
  const rcti &local_preferred_area = r_area;
  for (unsigned int index = 0; index < m_inputs.size(); index++) {
    if (index == used_canvas_index) {
      continue;
    }
    NodeOperationInput &input = m_inputs[index];
    if (input.isConnected()) {
      input.determine_canvas(local_preferred_area, unused_area);
    }
  }
}

void NodeOperation::set_canvas_input_index(unsigned int index)
{
  this->canvas_input_index_ = index;
}

void NodeOperation::init_data()
{
  /* Pass. */
}
void NodeOperation::initExecution()
{
  /* pass */
}

void NodeOperation::initMutex()
{
  BLI_mutex_init(&this->m_mutex);
}

void NodeOperation::lockMutex()
{
  BLI_mutex_lock(&this->m_mutex);
}

void NodeOperation::unlockMutex()
{
  BLI_mutex_unlock(&this->m_mutex);
}

void NodeOperation::deinitMutex()
{
  BLI_mutex_end(&this->m_mutex);
}

void NodeOperation::deinitExecution()
{
  /* pass */
}

void NodeOperation::set_canvas(const rcti &canvas_area)
{
  canvas_ = canvas_area;
  flags.is_canvas_set = true;
}

const rcti &NodeOperation::get_canvas() const
{
  return canvas_;
}

/**
 * Mainly used for re-determining canvas of constant operations in cases where preferred canvas
 * depends on the constant element.
 */
void NodeOperation::unset_canvas()
{
  BLI_assert(m_inputs.size() == 0);
  flags.is_canvas_set = false;
}

SocketReader *NodeOperation::getInputSocketReader(unsigned int inputSocketIndex)
{
  return this->getInputSocket(inputSocketIndex)->getReader();
}

NodeOperation *NodeOperation::getInputOperation(unsigned int inputSocketIndex)
{
  NodeOperationInput *input = getInputSocket(inputSocketIndex);
  if (input && input->isConnected()) {
    return &input->getLink()->getOperation();
  }

  return nullptr;
}

bool NodeOperation::determineDependingAreaOfInterest(rcti *input,
                                                     ReadBufferOperation *readOperation,
                                                     rcti *output)
{
  if (m_inputs.size() == 0) {
    BLI_rcti_init(output, input->xmin, input->xmax, input->ymin, input->ymax);
    return false;
  }

  rcti tempOutput;
  bool first = true;
  for (int i = 0; i < getNumberOfInputSockets(); i++) {
    NodeOperation *inputOperation = this->getInputOperation(i);
    if (inputOperation &&
        inputOperation->determineDependingAreaOfInterest(input, readOperation, &tempOutput)) {
      if (first) {
        output->xmin = tempOutput.xmin;
        output->ymin = tempOutput.ymin;
        output->xmax = tempOutput.xmax;
        output->ymax = tempOutput.ymax;
        first = false;
      }
      else {
        output->xmin = MIN2(output->xmin, tempOutput.xmin);
        output->ymin = MIN2(output->ymin, tempOutput.ymin);
        output->xmax = MAX2(output->xmax, tempOutput.xmax);
        output->ymax = MAX2(output->ymax, tempOutput.ymax);
      }
    }
  }
  return !first;
}

/* -------------------------------------------------------------------- */
/** \name Full Frame Methods
 * \{ */

/**
 * \brief Get input operation area being read by this operation on rendering given output area.
 *
 * Implementation don't need to ensure r_input_area is within input operation bounds. The
 * caller must clamp it.
 * TODO: See if it's possible to use parameter overloading (input_id for example).
 *
 * \param input_idx: Input operation index for which we want to calculate the area being read.
 * \param output_area: Area being rendered by this operation.
 * \param r_input_area: Returned input operation area that needs to be read in order to render
 * given output area.
 */
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
    NodeOperation *input_op = getInputOperation(input_idx);
    r_input_area = input_op->get_canvas();
  }
}

void NodeOperation::get_area_of_interest(NodeOperation *input_op,
                                         const rcti &output_area,
                                         rcti &r_input_area)
{
  for (int i = 0; i < getNumberOfInputSockets(); i++) {
    if (input_op == getInputOperation(i)) {
      get_area_of_interest(i, output_area, r_input_area);
      return;
    }
  }
  BLI_assert_msg(0, "input_op is not an input operation.");
}

/**
 * Executes operation image manipulation algorithm rendering given areas.
 * \param output_buf: Buffer to write result to.
 * \param areas: Areas within this operation bounds to render.
 * \param inputs_bufs: Inputs operations buffers.
 */
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

/**
 * Renders given areas using operations full frame implementation.
 */
void NodeOperation::render_full_frame(MemoryBuffer *output_buf,
                                      Span<rcti> areas,
                                      Span<MemoryBuffer *> inputs_bufs)
{
  initExecution();
  for (const rcti &area : areas) {
    update_memory_buffer(output_buf, area, inputs_bufs);
  }
  deinitExecution();
}

/**
 * Renders given areas using operations tiled implementation.
 */
void NodeOperation::render_full_frame_fallback(MemoryBuffer *output_buf,
                                               Span<rcti> areas,
                                               Span<MemoryBuffer *> inputs_bufs)
{
  Vector<NodeOperationOutput *> orig_input_links = replace_inputs_with_buffers(inputs_bufs);

  initExecution();
  const bool is_output_operation = getNumberOfOutputSockets() == 0;
  if (!is_output_operation && output_buf->is_a_single_elem()) {
    float *output_elem = output_buf->get_elem(0, 0);
    readSampled(output_elem, 0, 0, PixelSampler::Nearest);
  }
  else {
    for (const rcti &rect : areas) {
      exec_system_->execute_work(rect, [=](const rcti &split_rect) {
        rcti tile_rect = split_rect;
        if (is_output_operation) {
          executeRegion(&tile_rect, 0);
        }
        else {
          render_tile(output_buf, &tile_rect);
        }
      });
    }
  }
  deinitExecution();

  remove_buffers_and_restore_original_inputs(orig_input_links);
}

void NodeOperation::render_tile(MemoryBuffer *output_buf, rcti *tile_rect)
{
  const bool is_complex = get_flags().complex;
  void *tile_data = is_complex ? initializeTileData(tile_rect) : nullptr;
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
        readSampled(output_elem, x, y, PixelSampler::Nearest);
        output_elem += elem_stride;
      }
    }
  }
  if (tile_data) {
    deinitializeTileData(tile_rect, tile_data);
  }
}

/**
 * \return Replaced inputs links.
 */
Vector<NodeOperationOutput *> NodeOperation::replace_inputs_with_buffers(
    Span<MemoryBuffer *> inputs_bufs)
{
  BLI_assert(inputs_bufs.size() == getNumberOfInputSockets());
  Vector<NodeOperationOutput *> orig_links(inputs_bufs.size());
  for (int i = 0; i < inputs_bufs.size(); i++) {
    NodeOperationInput *input_socket = getInputSocket(i);
    BufferOperation *buffer_op = new BufferOperation(inputs_bufs[i], input_socket->getDataType());
    orig_links[i] = input_socket->getLink();
    input_socket->setLink(buffer_op->getOutputSocket());
    buffer_op->initExecution();
  }
  return orig_links;
}

void NodeOperation::remove_buffers_and_restore_original_inputs(
    Span<NodeOperationOutput *> original_inputs_links)
{
  BLI_assert(original_inputs_links.size() == getNumberOfInputSockets());
  for (int i = 0; i < original_inputs_links.size(); i++) {
    NodeOperation *buffer_op = get_input_operation(i);
    BLI_assert(buffer_op != nullptr);
    BLI_assert(typeid(*buffer_op) == typeid(BufferOperation));
    buffer_op->deinitExecution();
    NodeOperationInput *input_socket = getInputSocket(i);
    input_socket->setLink(original_inputs_links[i]);
    delete buffer_op;
  }
}

/** \} */

/*****************
 **** OpInput ****
 *****************/

NodeOperationInput::NodeOperationInput(NodeOperation *op, DataType datatype, ResizeMode resizeMode)
    : m_operation(op), m_datatype(datatype), m_resizeMode(resizeMode), m_link(nullptr)
{
}

SocketReader *NodeOperationInput::getReader()
{
  if (isConnected()) {
    return &m_link->getOperation();
  }

  return nullptr;
}

/**
 * \return Whether canvas area could be determined.
 */
bool NodeOperationInput::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  if (m_link) {
    m_link->determine_canvas(preferred_area, r_area);
    return !BLI_rcti_is_empty(&r_area);
  }
  return false;
}

/******************
 **** OpOutput ****
 ******************/

NodeOperationOutput::NodeOperationOutput(NodeOperation *op, DataType datatype)
    : m_operation(op), m_datatype(datatype)
{
}

void NodeOperationOutput::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  NodeOperation &operation = getOperation();
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
    const MemoryProxy *proxy = read_operation->getMemoryProxy();
    if (proxy) {
      const WriteBufferOperation *write_operation = proxy->getWriteBufferOperation();
      if (write_operation) {
        os << ",write=" << (NodeOperation &)*write_operation;
      }
    }
  }
  os << ")";

  return os;
}

}  // namespace blender::compositor
