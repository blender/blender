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
 * Copyright 2021, Blender Foundation.
 */

/** \file
 * \ingroup draw
 */

#include "BKE_global.h"

#include "BLI_vector.hh"

#include "draw_texture_pool.h"

using namespace blender;

struct DRWTexturePoolHandle {
  uint64_t users_bits;
  GPUTexture *texture;
};

struct DRWTexturePool {
  Vector<void *, 16> users;
  Vector<DRWTexturePoolHandle> handles;
  /* Cache last result to avoid linear search each time. */
  int last_user_id = -1;
};

DRWTexturePool *DRW_texture_pool_create(void)
{
  return new DRWTexturePool();
}

void DRW_texture_pool_free(DRWTexturePool *pool)
{
  /* Resetting the pool twice will effectively free all textures. */
  DRW_texture_pool_reset(pool);
  DRW_texture_pool_reset(pool);
  delete pool;
}

/**
 * Try to find a texture corresponding to params into the texture pool.
 * If no texture was found, create one and add it to the pool.
 */
GPUTexture *DRW_texture_pool_query(
    DRWTexturePool *pool, int width, int height, eGPUTextureFormat format, void *user)
{
  int user_id = pool->last_user_id;
  /* Try cached value. */
  if (user_id != -1) {
    if (pool->users[user_id] != user) {
      user_id = -1;
    }
  }
  /* Try to find inside previous users. */
  if (user_id == -1) {
    user_id = pool->users.first_index_of_try(user);
  }
  /* No chance, needs to add it to the user list. */
  if (user_id == -1) {
    user_id = pool->users.size();
    pool->users.append(user);
    /* If there is more than 63 users, better refactor this system. */
    BLI_assert(user_id < 64);
  }
  pool->last_user_id = user_id;

  uint64_t user_bit = 1llu << user_id;
  for (DRWTexturePoolHandle &handle : pool->handles) {
    /* Skip if the user is already using this texture. */
    if (user_bit & handle.users_bits) {
      continue;
    }
    /* If everything matches reuse the texture. */
    if ((GPU_texture_format(handle.texture) == format) &&
        (GPU_texture_width(handle.texture) == width) &&
        (GPU_texture_height(handle.texture) == height)) {
      handle.users_bits |= user_bit;
      return handle.texture;
    }
  }

  char name[16] = "DRW_tex_pool";
  if (G.debug & G_DEBUG_GPU) {
    int texture_id = pool->handles.size();
    SNPRINTF(name, "DRW_tex_pool_%d", texture_id);
  }

  DRWTexturePoolHandle handle;
  handle.users_bits = user_bit;
  handle.texture = GPU_texture_create_2d(name, width, height, 1, format, nullptr);
  pool->handles.append(handle);
  /* Doing filtering for depth does not make sense when not doing shadow mapping,
   * and enabling texture filtering on integer texture make them unreadable. */
  bool do_filter = !GPU_texture_depth(handle.texture) && !GPU_texture_integer(handle.texture);
  GPU_texture_filter_mode(handle.texture, do_filter);

  return handle.texture;
}

/* Resets the user bits for each texture in the pool and delete unused ones. */
void DRW_texture_pool_reset(DRWTexturePool *pool)
{
  pool->last_user_id = -1;

  for (auto it = pool->handles.rbegin(); it != pool->handles.rend(); ++it) {
    DRWTexturePoolHandle &handle = *it;
    if (handle.users_bits == 0) {
      if (handle.texture) {
        GPU_texture_free(handle.texture);
        handle.texture = nullptr;
      }
    }
    else {
      handle.users_bits = 0;
    }
  }

  /* Reverse iteration to make sure we only reorder with known good handles. */
  for (int i = pool->handles.size() - 1; i >= 0; i--) {
    if (!pool->handles[i].texture) {
      pool->handles.remove_and_reorder(i);
    }
  }
}
