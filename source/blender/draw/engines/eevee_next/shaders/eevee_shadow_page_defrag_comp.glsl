
/**
 * Virtual shadowmapping: Defrag.
 *
 * Defragment the cached page buffer making one continuous array.
 *
 * Also pop_front the cached pages if there is not enough free pages for the needed allocations.
 * Here is an example of the behavior of this buffer during one update cycle:
 *
 *   Initial state: 5 cached pages. Buffer starts at index 2 and ends at 6.
 *     [--xxxxx---------]
 *   After page free step: 2 cached pages were removed (r), 3 pages were inserted in the cache (i).
 *     [--xrxrxiii------]
 *   After page defrag step: The buffer is compressed into only 6 pages.
 *     [----xxxxxx------]
 */

#pragma BLENDER_REQUIRE(eevee_shadow_page_ops_lib.glsl)

const uint max_page = SHADOW_MAX_PAGE;

void find_first_valid(inout uint src, uint dst)
{
  for (; src < dst; src++) {
    if (pages_cached_buf[src % max_page].x != uint(-1)) {
      return;
    }
  }
}

void page_cached_free(uint page_index)
{
  uint tile_index = pages_cached_buf[page_index].y;
  ShadowTileData tile = shadow_tile_unpack(tiles_buf[tile_index]);

  shadow_page_cache_remove(tile);
  shadow_page_free(tile);

  tiles_buf[tile_index] = shadow_tile_pack(tile);
}

#if 0 /* Can be used to debug heap and invalid pages inside the free buffer. */
#  define check_heap_integrity(heap, start, size, invalid_val, result) \
    result = true; \
    for (int i = 0; i < max_page; i++) { \
      if ((i >= start) && (i < (start + size))) { \
        result = result && (heap[i].x != invalid_val); \
      } \
      else { \
        result = result && (heap[i].x == invalid_val); \
      } \
    }
#else
#  define check_heap_integrity(heap, start, size, invalid_val, result)
#endif

void main()
{
  /* Pages we need to get off the cache for the allocation pass. */
  int additional_pages = pages_infos_buf.page_alloc_count - pages_infos_buf.page_free_count;

  uint src = pages_infos_buf.page_cached_start;
  uint end = pages_infos_buf.page_cached_end;

  find_first_valid(src, end);

  bool valid_pre;
  check_heap_integrity(pages_free_buf, 0, pages_infos_buf.page_free_count, uint(-1), valid_pre);

  /* First free as much pages as needed from the end of the cached range to fulfill the allocation.
   * Avoid defragmenting to then free them. */
  for (; additional_pages > 0 && src < end; additional_pages--) {
    page_cached_free(src % max_page);
    find_first_valid(src, end);
  }

  /* Defrag page in "old" range. */
  bool is_empty = (src == end);
  if (!is_empty) {
    /* `page_cached_end` refers to the next empty slot.
     * Decrement by one to refer to the first slot we can defrag. */
    for (uint dst = end - 1; dst > src; dst--) {
      /* Find hole. */
      if (pages_cached_buf[dst % max_page].x != uint(-1)) {
        continue;
      }
      /* Update corresponding reference in tile. */
      shadow_page_cache_update_page_ref(src % max_page, dst % max_page);
      /* Move page. */
      pages_cached_buf[dst % max_page] = pages_cached_buf[src % max_page];
      pages_cached_buf[src % max_page] = uvec2(-1);

      find_first_valid(src, dst);
    }
  }

  end = pages_infos_buf.page_cached_next;
  /* Free pages in the "new" range (these are compact). */
  for (; additional_pages > 0 && src < end; additional_pages--, src++) {
    page_cached_free(src % max_page);
  }

  bool valid_post;
  check_heap_integrity(pages_free_buf, 0, pages_infos_buf.page_free_count, uint(-1), valid_post);

  pages_infos_buf.page_cached_start = src;
  pages_infos_buf.page_cached_end = end;
  pages_infos_buf.page_alloc_count = 0;
  pages_infos_buf.view_count = 0;

  /* Stats. */
  statistics_buf.page_used_count = 0;
  statistics_buf.page_update_count = 0;
  statistics_buf.page_allocated_count = 0;
  statistics_buf.page_rendered_count = 0;

  /* Wrap the cursor to avoid unsigned overflow. We do not do modulo arithmetic because it would
   * produce a 0 length buffer if the buffer is full. */
  if (pages_infos_buf.page_cached_start > max_page) {
    pages_infos_buf.page_cached_next -= max_page;
    pages_infos_buf.page_cached_start -= max_page;
    pages_infos_buf.page_cached_end -= max_page;
  }

  /* Reset clear command indirect buffer. */
  clear_dispatch_buf.num_groups_x = pages_infos_buf.page_size / SHADOW_PAGE_CLEAR_GROUP_SIZE;
  clear_dispatch_buf.num_groups_y = pages_infos_buf.page_size / SHADOW_PAGE_CLEAR_GROUP_SIZE;
  clear_dispatch_buf.num_groups_z = 0;
}
