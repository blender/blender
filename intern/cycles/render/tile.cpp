/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "render/tile.h"

#include <atomic>

#include "graph/node.h"
#include "render/background.h"
#include "render/film.h"
#include "render/integrator.h"
#include "render/scene.h"
#include "util/util_algorithm.h"
#include "util/util_foreach.h"
#include "util/util_logging.h"
#include "util/util_path.h"
#include "util/util_string.h"
#include "util/util_system.h"
#include "util/util_types.h"

CCL_NAMESPACE_BEGIN

/* --------------------------------------------------------------------
 * Internal functions.
 */

static const char *ATTR_PASSES_COUNT = "cycles.passes.count";
static const char *ATTR_PASS_SOCKET_PREFIX_FORMAT = "cycles.passes.%d.";
static const char *ATTR_BUFFER_SOCKET_PREFIX = "cycles.buffer.";
static const char *ATTR_DENOISE_SOCKET_PREFIX = "cycles.denoise.";

/* Global counter of ToleManager object instances. */
static std::atomic<uint64_t> g_instance_index = 0;

/* Construct names of EXR channels which will ensure order of all channels to match exact offsets
 * in render buffers corresponding to the given passes.
 *
 * Returns `std` datatypes so that it can be assigned directly to the OIIO's `ImageSpec`. */
static std::vector<std::string> exr_channel_names_for_passes(const BufferParams &buffer_params)
{
  static const char *component_suffixes[] = {"R", "G", "B", "A"};

  int pass_index = 0;
  int num_channels = 0;
  std::vector<std::string> channel_names;
  for (const BufferPass &pass : buffer_params.passes) {
    if (pass.offset == PASS_UNUSED) {
      continue;
    }

    const PassInfo pass_info = pass.get_info();
    num_channels += pass_info.num_components;

    /* EXR canonically expects first part of channel names to be sorted alphabetically, which is
     * not guaranteed to be the case with passes names. Assign a prefix based on the pass index
     * with a fixed width to ensure ordering. This makes it possible to dump existing render
     * buffers memory to disk and read it back without doing extra mapping. */
    const string prefix = string_printf("%08d", pass_index);

    const string channel_name_prefix = prefix + string(pass.name) + ".";

    for (int i = 0; i < pass_info.num_components; ++i) {
      channel_names.push_back(channel_name_prefix + component_suffixes[i]);
    }

    ++pass_index;
  }

  return channel_names;
}

inline string node_socket_attribute_name(const SocketType &socket, const string &attr_name_prefix)
{
  return attr_name_prefix + string(socket.name);
}

template<typename ValidateValueFunc, typename GetValueFunc>
static bool node_socket_generic_to_image_spec_atttributes(
    ImageSpec *image_spec,
    const Node *node,
    const SocketType &socket,
    const string &attr_name_prefix,
    const ValidateValueFunc &validate_value_func,
    const GetValueFunc &get_value_func)
{
  if (!validate_value_func(node, socket)) {
    return false;
  }

  image_spec->attribute(node_socket_attribute_name(socket, attr_name_prefix),
                        get_value_func(node, socket));

  return true;
}

static bool node_socket_to_image_spec_atttributes(ImageSpec *image_spec,
                                                  const Node *node,
                                                  const SocketType &socket,
                                                  const string &attr_name_prefix)
{
  const string attr_name = node_socket_attribute_name(socket, attr_name_prefix);

  switch (socket.type) {
    case SocketType::ENUM: {
      const ustring value = node->get_string(socket);

      /* Validate that the node is consistent with the node type definition. */
      const NodeEnum &enum_values = *socket.enum_values;
      if (!enum_values.exists(value)) {
        LOG(DFATAL) << "Node enum contains invalid value " << value;
        return false;
      }

      image_spec->attribute(attr_name, value);

      return true;
    }

    case SocketType::STRING:
      image_spec->attribute(attr_name, node->get_string(socket));
      return true;

    case SocketType::INT:
      image_spec->attribute(attr_name, node->get_int(socket));
      return true;

    case SocketType::FLOAT:
      image_spec->attribute(attr_name, node->get_float(socket));
      return true;

    case SocketType::BOOLEAN:
      image_spec->attribute(attr_name, node->get_bool(socket));
      return true;

    default:
      LOG(DFATAL) << "Unhandled socket type " << socket.type << ", should never happen.";
      return false;
  }
}

static bool node_socket_from_image_spec_atttributes(Node *node,
                                                    const SocketType &socket,
                                                    const ImageSpec &image_spec,
                                                    const string &attr_name_prefix)
{
  const string attr_name = node_socket_attribute_name(socket, attr_name_prefix);

  switch (socket.type) {
    case SocketType::ENUM: {
      /* TODO(sergey): Avoid construction of `ustring` by using `string_view` in the Node API. */
      const ustring value(image_spec.get_string_attribute(attr_name, ""));

      /* Validate that the node is consistent with the node type definition. */
      const NodeEnum &enum_values = *socket.enum_values;
      if (!enum_values.exists(value)) {
        LOG(ERROR) << "Invalid enumerator value " << value;
        return false;
      }

      node->set(socket, enum_values[value]);

      return true;
    }

    case SocketType::STRING:
      /* TODO(sergey): Avoid construction of `ustring` by using `string_view` in the Node API. */
      node->set(socket, ustring(image_spec.get_string_attribute(attr_name, "")));
      return true;

    case SocketType::INT:
      node->set(socket, image_spec.get_int_attribute(attr_name, 0));
      return true;

    case SocketType::FLOAT:
      node->set(socket, image_spec.get_float_attribute(attr_name, 0));
      return true;

    case SocketType::BOOLEAN:
      node->set(socket, static_cast<bool>(image_spec.get_int_attribute(attr_name, 0)));
      return true;

    default:
      LOG(DFATAL) << "Unhandled socket type " << socket.type << ", should never happen.";
      return false;
  }
}

static bool node_to_image_spec_atttributes(ImageSpec *image_spec,
                                           const Node *node,
                                           const string &attr_name_prefix)
{
  for (const SocketType &socket : node->type->inputs) {
    if (!node_socket_to_image_spec_atttributes(image_spec, node, socket, attr_name_prefix)) {
      return false;
    }
  }

  return true;
}

static bool node_from_image_spec_atttributes(Node *node,
                                             const ImageSpec &image_spec,
                                             const string &attr_name_prefix)
{
  for (const SocketType &socket : node->type->inputs) {
    if (!node_socket_from_image_spec_atttributes(node, socket, image_spec, attr_name_prefix)) {
      return false;
    }
  }

  return true;
}

static bool buffer_params_to_image_spec_atttributes(ImageSpec *image_spec,
                                                    const BufferParams &buffer_params)
{
  if (!node_to_image_spec_atttributes(image_spec, &buffer_params, ATTR_BUFFER_SOCKET_PREFIX)) {
    return false;
  }

  /* Passes storage is not covered by the node socket. so "expand" the loop manually. */

  const int num_passes = buffer_params.passes.size();
  image_spec->attribute(ATTR_PASSES_COUNT, num_passes);

  for (int pass_index = 0; pass_index < num_passes; ++pass_index) {
    const string attr_name_prefix = string_printf(ATTR_PASS_SOCKET_PREFIX_FORMAT, pass_index);

    const BufferPass *pass = &buffer_params.passes[pass_index];
    if (!node_to_image_spec_atttributes(image_spec, pass, attr_name_prefix)) {
      return false;
    }
  }

  return true;
}

static bool buffer_params_from_image_spec_atttributes(BufferParams *buffer_params,
                                                      const ImageSpec &image_spec)
{
  if (!node_from_image_spec_atttributes(buffer_params, image_spec, ATTR_BUFFER_SOCKET_PREFIX)) {
    return false;
  }

  /* Passes storage is not covered by the node socket. so "expand" the loop manually. */

  const int num_passes = image_spec.get_int_attribute(ATTR_PASSES_COUNT, 0);
  if (num_passes == 0) {
    LOG(ERROR) << "Missing passes count attribute.";
    return false;
  }

  for (int pass_index = 0; pass_index < num_passes; ++pass_index) {
    const string attr_name_prefix = string_printf(ATTR_PASS_SOCKET_PREFIX_FORMAT, pass_index);

    BufferPass pass;

    if (!node_from_image_spec_atttributes(&pass, image_spec, attr_name_prefix)) {
      return false;
    }

    buffer_params->passes.emplace_back(std::move(pass));
  }

  buffer_params->update_passes();

  return true;
}

/* Configure image specification for the given buffer parameters and passes.
 *
 * Image channels will be strictly ordered to match content of corresponding buffer, and the
 * metadata will be set so that the render buffers and passes can be reconstructed from it.
 *
 * If the tile size different from (0, 0) the image specification will be configured to use the
 * given tile size for tiled IO. */
static bool configure_image_spec_from_buffer(ImageSpec *image_spec,
                                             const BufferParams &buffer_params,
                                             const int2 tile_size = make_int2(0, 0))
{
  const std::vector<std::string> channel_names = exr_channel_names_for_passes(buffer_params);
  const int num_channels = channel_names.size();

  *image_spec = ImageSpec(
      buffer_params.width, buffer_params.height, num_channels, TypeDesc::FLOAT);

  image_spec->channelnames = move(channel_names);

  if (!buffer_params_to_image_spec_atttributes(image_spec, buffer_params)) {
    return false;
  }

  if (tile_size.x != 0 || tile_size.y != 0) {
    DCHECK_GT(tile_size.x, 0);
    DCHECK_GT(tile_size.y, 0);

    image_spec->tile_width = tile_size.x;
    image_spec->tile_height = tile_size.y;
  }

  return true;
}

/* --------------------------------------------------------------------
 * Tile Manager.
 */

TileManager::TileManager()
{
  /* Use process ID to separate different processes.
   * To ensure uniqueness from within a process use combination of object address and instance
   * index. This solves problem of possible object re-allocation at the same time, and solves
   * possible conflict when the counter overflows while there are still active instances of the
   * class. */
  const int tile_manager_id = g_instance_index.fetch_add(1, std::memory_order_relaxed);
  tile_file_unique_part_ = to_string(system_self_process_id()) + "-" +
                           to_string(reinterpret_cast<uintptr_t>(this)) + "-" +
                           to_string(tile_manager_id);
}

TileManager::~TileManager()
{
}

void TileManager::reset_scheduling(const BufferParams &params, int2 tile_size)
{
  VLOG(3) << "Using tile size of " << tile_size;

  close_tile_output();

  tile_size_ = tile_size;

  tile_state_.num_tiles_x = divide_up(params.width, tile_size_.x);
  tile_state_.num_tiles_y = divide_up(params.height, tile_size_.y);
  tile_state_.num_tiles = tile_state_.num_tiles_x * tile_state_.num_tiles_y;

  tile_state_.next_tile_index = 0;

  tile_state_.current_tile = Tile();
}

void TileManager::update(const BufferParams &params, const Scene *scene)
{
  DCHECK_NE(params.pass_stride, -1);

  buffer_params_ = params;

  /* TODO(sergey): Proper Error handling, so that if configuration has failed we don't attempt to
   * write to a partially configured file. */
  configure_image_spec_from_buffer(&write_state_.image_spec, buffer_params_, tile_size_);

  const DenoiseParams denoise_params = scene->integrator->get_denoise_params();
  node_to_image_spec_atttributes(
      &write_state_.image_spec, &denoise_params, ATTR_DENOISE_SOCKET_PREFIX);
}

bool TileManager::done()
{
  return tile_state_.next_tile_index == tile_state_.num_tiles;
}

bool TileManager::next()
{
  if (done()) {
    return false;
  }

  tile_state_.current_tile = get_tile_for_index(tile_state_.next_tile_index);

  ++tile_state_.next_tile_index;

  return true;
}

Tile TileManager::get_tile_for_index(int index) const
{
  /* TODO(sergey): Consider using hilbert spiral, or. maybe, even configurable. Not sure this
   * brings a lot of value since this is only applicable to BIG tiles. */

  const int tile_y = index / tile_state_.num_tiles_x;
  const int tile_x = index - tile_y * tile_state_.num_tiles_x;

  Tile tile;

  tile.x = tile_x * tile_size_.x;
  tile.y = tile_y * tile_size_.y;
  tile.width = tile_size_.x;
  tile.height = tile_size_.y;

  tile.width = min(tile.width, buffer_params_.width - tile.x);
  tile.height = min(tile.height, buffer_params_.height - tile.y);

  return tile;
}

const Tile &TileManager::get_current_tile() const
{
  return tile_state_.current_tile;
}

bool TileManager::open_tile_output()
{
  write_state_.filename = path_temp_get("cycles-tile-buffer-" + tile_file_unique_part_ + "-" +
                                        to_string(write_state_.tile_file_index) + ".exr");

  write_state_.tile_out = ImageOutput::create(write_state_.filename);
  if (!write_state_.tile_out) {
    LOG(ERROR) << "Error creating image output for " << write_state_.filename;
    return false;
  }

  if (!write_state_.tile_out->supports("tiles")) {
    LOG(ERROR) << "Progress tile file format does not support tiling.";
    return false;
  }

  write_state_.tile_out->open(write_state_.filename, write_state_.image_spec);
  write_state_.num_tiles_written = 0;

  VLOG(3) << "Opened tile file " << write_state_.filename;

  return true;
}

bool TileManager::close_tile_output()
{
  if (!write_state_.tile_out) {
    return true;
  }

  const bool success = write_state_.tile_out->close();
  write_state_.tile_out = nullptr;

  if (!success) {
    LOG(ERROR) << "Error closing tile file.";
    return false;
  }

  VLOG(3) << "Tile output is closed.";

  return true;
}

bool TileManager::write_tile(const RenderBuffers &tile_buffers)
{
  if (!write_state_.tile_out) {
    if (!open_tile_output()) {
      return false;
    }
  }

  DCHECK_EQ(tile_buffers.params.pass_stride, buffer_params_.pass_stride);

  const BufferParams &tile_params = tile_buffers.params;

  vector<float> pixel_storage;
  const float *pixels = tile_buffers.buffer.data();

  /* Tiled writing expects pixels to contain data for an entire tile. Pad the render buffers with
   * empty pixels for tiles which are on the image boundary. */
  if (tile_params.width != tile_size_.x || tile_params.height != tile_size_.y) {
    const int64_t pass_stride = tile_params.pass_stride;
    const int64_t src_row_stride = tile_params.width * pass_stride;

    const int64_t dst_row_stride = tile_size_.x * pass_stride;
    pixel_storage.resize(dst_row_stride * tile_size_.y);

    const float *src = tile_buffers.buffer.data();
    float *dst = pixel_storage.data();
    pixels = dst;

    for (int y = 0; y < tile_params.height; ++y, src += src_row_stride, dst += dst_row_stride) {
      memcpy(dst, src, src_row_stride * sizeof(float));
    }
  }

  const int tile_x = tile_params.full_x - buffer_params_.full_x;
  const int tile_y = tile_params.full_y - buffer_params_.full_y;

  VLOG(3) << "Write tile at " << tile_x << ", " << tile_y;
  if (!write_state_.tile_out->write_tile(tile_x, tile_y, 0, TypeDesc::FLOAT, pixels)) {
    LOG(ERROR) << "Error writing tile " << write_state_.tile_out->geterror();
  }

  ++write_state_.num_tiles_written;

  return true;
}

void TileManager::finish_write_tiles()
{
  if (!write_state_.tile_out) {
    /* None of the tiles were written hence the file was not created.
     * Avoid creation of fully empty file since it is redundant. */
    return;
  }

  /* EXR expects all tiles to present in file. So explicitly write missing tiles as all-zero. */
  if (write_state_.num_tiles_written < tile_state_.num_tiles) {
    vector<float> pixel_storage(tile_size_.x * tile_size_.y * buffer_params_.pass_stride);

    for (int tile_index = write_state_.num_tiles_written; tile_index < tile_state_.num_tiles;
         ++tile_index) {
      const Tile tile = get_tile_for_index(tile_index);

      VLOG(3) << "Write dummy tile at " << tile.x << ", " << tile.y;

      write_state_.tile_out->write_tile(tile.x, tile.y, 0, TypeDesc::FLOAT, pixel_storage.data());
    }
  }

  close_tile_output();

  if (full_buffer_written_cb) {
    full_buffer_written_cb(write_state_.filename);
  }

  /* Advance the counter upon explicit finish of the file.
   * Makes it possible to re-use tile manager for another scene, and avoids unnecessary increments
   * of the tile-file-within-session index. */
  ++write_state_.tile_file_index;

  write_state_.filename = "";
}

bool TileManager::read_full_buffer_from_disk(const string_view filename,
                                             RenderBuffers *buffers,
                                             DenoiseParams *denoise_params)
{
  unique_ptr<ImageInput> in(ImageInput::open(filename));
  if (!in) {
    LOG(ERROR) << "Error opening tile file " << filename;
    return false;
  }

  const ImageSpec &image_spec = in->spec();

  BufferParams buffer_params;
  if (!buffer_params_from_image_spec_atttributes(&buffer_params, image_spec)) {
    return false;
  }
  buffers->reset(buffer_params);

  if (!node_from_image_spec_atttributes(denoise_params, image_spec, ATTR_DENOISE_SOCKET_PREFIX)) {
    return false;
  }

  if (!in->read_image(TypeDesc::FLOAT, buffers->buffer.data())) {
    LOG(ERROR) << "Error reading pixels from the tile file " << in->geterror();
    return false;
  }

  if (!in->close()) {
    LOG(ERROR) << "Error closing tile file " << in->geterror();
    return false;
  }

  return true;
}

CCL_NAMESPACE_END
