/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "session/buffers.h"
#include "util/image.h"
#include "util/string.h"
#include "util/unique_ptr.h"

CCL_NAMESPACE_BEGIN

class DenoiseParams;
class Scene;

/* --------------------------------------------------------------------
 * Tile.
 */

class Tile {
 public:
  int x = 0, y = 0;
  int width = 0, height = 0;

  int window_x = 0, window_y = 0;
  int window_width = 0, window_height = 0;

  Tile() {}
};

/* --------------------------------------------------------------------
 * Tile Manager.
 */

class TileManager {
 public:
  /* This callback is invoked by whenever on-dist tiles storage file is closed after writing. */
  function<void(string_view)> full_buffer_written_cb;

  TileManager();
  ~TileManager();

  TileManager(const TileManager &other) = delete;
  TileManager(TileManager &&other) noexcept = delete;
  TileManager &operator=(const TileManager &other) = delete;
  TileManager &operator=(TileManager &&other) = delete;

  /* Reset current progress and start new rendering of the full-frame parameters in tiles of the
   * given size.
   * Only touches scheduling-related state of the tile manager. */
  /* TODO(sergey): Consider using tile area instead of exact size to help dealing with extreme
   * cases of stretched renders. */
  void reset_scheduling(const BufferParams &params, int2 tile_size);

  /* Update for the known buffer passes and scene parameters.
   * Will store all parameters needed for buffers access outside of the scene graph. */
  void update(const BufferParams &params, const Scene *scene);

  void set_temp_dir(const string &temp_dir);

  inline int get_num_tiles() const
  {
    return tile_state_.num_tiles;
  }

  inline bool has_multiple_tiles() const
  {
    return tile_state_.num_tiles > 1;
  }

  inline int get_tile_overscan() const
  {
    return overscan_;
  }

  bool next();
  bool done();

  const Tile &get_current_tile() const;
  const int2 get_size() const;

  /* Write render buffer of a tile to a file on disk.
   *
   * Opens file for write when first tile is written.
   *
   * Returns true on success. */
  bool write_tile(const RenderBuffers &tile_buffers);

  /* Inform the tile manager that no more tiles will be written to disk.
   * The file will be considered final, all handles to it will be closed. */
  void finish_write_tiles();

  /* Check whether any tile has been written to disk. */
  inline bool has_written_tiles() const
  {
    return write_state_.num_tiles_written != 0;
  }

  /* Read full frame render buffer from tiles file on disk.
   *
   * Returns true on success. */
  bool read_full_buffer_from_disk(string_view filename,
                                  RenderBuffers *buffers,
                                  DenoiseParams *denoise_params);

  /* Compute valid tile size compatible with image saving. */
  int compute_render_tile_size(const int suggested_tile_size) const;

  /* Tile size in the image file. */
  static const int IMAGE_TILE_SIZE = 128;

  /* Maximum supported tile size.
   * Needs to be safe from allocation on a GPU point of view: the display driver needs to be able
   * to allocate texture with the side size of this value.
   * Use conservative value which is safe for most of OpenGL drivers and GPUs. */
  static const int MAX_TILE_SIZE = 8192;

 protected:
  /* Get tile configuration for its index.
   * The tile index must be within [0, state_.tile_state_). */
  Tile get_tile_for_index(int index) const;

  bool open_tile_output();
  bool close_tile_output();

  string temp_dir_;

  /* Part of an on-disk tile file name which avoids conflicts between several Cycles instances or
   * several sessions. */
  string tile_file_unique_part_;

  int2 tile_size_ = make_int2(0, 0);

  /* Number of extra pixels around the actual tile to render. */
  int overscan_ = 0;

  BufferParams buffer_params_;

  /* Tile scheduling state. */
  struct {
    int num_tiles_x = 0;
    int num_tiles_y = 0;
    int num_tiles = 0;

    int next_tile_index;

    Tile current_tile;
  } tile_state_;

  /* State of tiles writing to a file on disk. */
  struct {
    /* Index of a tile file used during the current session.
     * This number is used for the file name construction, making it possible to render several
     * scenes throughout duration of the session and keep all results available for later read
     * access. */
    int tile_file_index = 0;

    string filename;

    /* Specification of the tile image which corresponds to the buffer parameters.
     * Contains channels configured according to the passes configuration in the path traces.
     *
     * Output images are saved using this specification, input images are expected to have matched
     * specification. */
    ImageSpec image_spec;

    /* Output handle for the tile file.
     *
     * This file can not be closed until all tiles has been provided, so the handle is stored in
     * the state and is created whenever writing is requested. */
    unique_ptr<ImageOutput> tile_out;

    int num_tiles_written = 0;
  } write_state_;
};

CCL_NAMESPACE_END
