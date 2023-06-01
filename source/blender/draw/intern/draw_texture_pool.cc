/* SPDX-FileCopyrightText: 2021 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

  Vector<GPUTexture *> tmp_tex_pruned;
  Vector<GPUTexture *> tmp_tex_released;
  Vector<GPUTexture *> tmp_tex_acquired;
};

DRWTexturePool *DRW_texture_pool_create()
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

GPUTexture *DRW_texture_pool_query(DRWTexturePool *pool,
                                   int width,
                                   int height,
                                   eGPUTextureFormat format,
                                   eGPUTextureUsage usage,
                                   void *user)
{
  /* Texture pools have an implicit usage as a texture attachment. */
  BLI_assert_msg(usage & GPU_TEXTURE_USAGE_ATTACHMENT,
                 "Pool textures must be of usage type attachment.");
  usage = usage | GPU_TEXTURE_USAGE_ATTACHMENT;

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
        (GPU_texture_height(handle.texture) == height) &&
        (GPU_texture_usage(handle.texture) == usage))
    {
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
  handle.texture = GPU_texture_create_2d(name, width, height, 1, format, usage, nullptr);
  pool->handles.append(handle);
  /* Doing filtering for depth does not make sense when not doing shadow mapping,
   * and enabling texture filtering on integer texture make them unreadable. */
  bool do_filter = !GPU_texture_has_depth_format(handle.texture) &&
                   !GPU_texture_has_integer_format(handle.texture);
  GPU_texture_filter_mode(handle.texture, do_filter);

  return handle.texture;
}

GPUTexture *DRW_texture_pool_texture_acquire(
    DRWTexturePool *pool, int width, int height, eGPUTextureFormat format, eGPUTextureUsage usage)
{
  GPUTexture *tmp_tex = nullptr;
  int64_t found_index = 0;

  auto texture_match = [&](GPUTexture *tex) -> bool {
    /* TODO(@fclem): We could reuse texture using texture views if the formats are compatible. */
    return (GPU_texture_format(tex) == format) && (GPU_texture_width(tex) == width) &&
           (GPU_texture_height(tex) == height) && (GPU_texture_usage(tex) == usage);
  };

  /* Search released texture first. */
  for (auto i : pool->tmp_tex_released.index_range()) {
    if (texture_match(pool->tmp_tex_released[i])) {
      tmp_tex = pool->tmp_tex_released[i];
      found_index = i;
      break;
    }
  }

  if (tmp_tex) {
    pool->tmp_tex_released.remove_and_reorder(found_index);
  }
  else {
    /* Then search pruned texture. */
    for (auto i : pool->tmp_tex_pruned.index_range()) {
      if (texture_match(pool->tmp_tex_pruned[i])) {
        tmp_tex = pool->tmp_tex_pruned[i];
        found_index = i;
        break;
      }
    }

    if (tmp_tex) {
      pool->tmp_tex_pruned.remove_and_reorder(found_index);
    }
  }

  if (!tmp_tex) {
    /* Create a new texture in last resort. */
    char name[16] = "DRW_tex_pool";
    if (G.debug & G_DEBUG_GPU) {
      int texture_id = pool->handles.size();
      SNPRINTF(name, "DRW_tex_pool_%d", texture_id);
    }
    tmp_tex = GPU_texture_create_2d(name, width, height, 1, format, usage, nullptr);
  }

  pool->tmp_tex_acquired.append(tmp_tex);

  return tmp_tex;
}

void DRW_texture_pool_texture_release(DRWTexturePool *pool, GPUTexture *tmp_tex)
{
  pool->tmp_tex_acquired.remove_first_occurrence_and_reorder(tmp_tex);
  pool->tmp_tex_released.append(tmp_tex);
}

void DRW_texture_pool_take_texture_ownership(DRWTexturePool *pool, GPUTexture *tex)
{
  pool->tmp_tex_acquired.remove_first_occurrence_and_reorder(tex);
}

void DRW_texture_pool_give_texture_ownership(DRWTexturePool *pool, GPUTexture *tex)
{
  BLI_assert(pool->tmp_tex_acquired.first_index_of_try(tex) == -1 &&
             pool->tmp_tex_released.first_index_of_try(tex) == -1 &&
             pool->tmp_tex_pruned.first_index_of_try(tex) == -1);
  pool->tmp_tex_acquired.append(tex);
}

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

  BLI_assert_msg(pool->tmp_tex_acquired.is_empty(),
                 "Missing a TextureFromPool.release() before end of draw.");
  for (GPUTexture *tmp_tex : pool->tmp_tex_pruned) {
    GPU_texture_free(tmp_tex);
  }
  pool->tmp_tex_pruned = pool->tmp_tex_released;
  pool->tmp_tex_released.clear();
}
