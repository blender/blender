/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Sort a buffer of surfel list by distance along a direction.
 * The resulting surfel lists are then the equivalent of a series of ray cast in the same
 * direction. The fact that the surfels are sorted gives proper occlusion.
 *
 * Sort by increasing `ray_distance`. Start of list is smallest value.
 *
 * Dispatched as 1 thread per list.
 */

#pragma BLENDER_REQUIRE(gpu_shader_utildefines_lib.glsl)

/**
 * A doubly-linked list implementation.
 * IMPORTANT: It is not general purpose as it only cover the cases needed by this shader.
 */
struct List {
  int first, last;
};

/* Return the split list after link_index. */
List list_split_after(inout List original, int link_index)
{
  int next_link = surfel_buf[link_index].next;
  int last_link = original.last;

  original.last = link_index;

  List split;
  split.first = next_link;
  split.last = last_link;

  surfel_buf[link_index].next = -1;
  surfel_buf[next_link].prev = -1;

  return split;
}

void list_add_tail(inout List list, int link_index)
{
  surfel_buf[link_index].next = -1;
  surfel_buf[link_index].prev = list.last;
  surfel_buf[list.last].next = link_index;
  list.last = link_index;
}

void list_insert_link_before(inout List list, int next_link, int new_link)
{
  if (list.first == next_link) {
    /* At beginning of list. */
    list.first = new_link;
  }
  int prev_link = surfel_buf[next_link].prev;
  surfel_buf[new_link].next = next_link;
  surfel_buf[new_link].prev = prev_link;
  surfel_buf[next_link].prev = new_link;
  if (prev_link != -1) {
    surfel_buf[prev_link].next = new_link;
  }
}

/**
 * Return true if link from `surfel[a]` to `surfel[b]` is valid.
 * WARNING: this function is not commutative : `f(a, b) != f(b, a)`
 */
bool is_valid_surfel_link(int a, int b)
{
  vec3 link_vector = normalize(surfel_buf[b].position - surfel_buf[a].position);
  float link_angle_cos = dot(surfel_buf[a].normal, link_vector);
  bool is_coplanar = abs(link_angle_cos) < 1.0e-3;
  return !is_coplanar;
}

void main()
{
  int list_index = int(gl_GlobalInvocationID.x);
  if (list_index >= list_info_buf.list_max) {
    return;
  }

  int list_start = list_start_buf[list_index];

  if (list_start == -1) {
    /* Empty list. */
    return;
  }

  /* Create Surfel.prev pointers. */
  int prev_id = -1;
  for (int i = list_start; i > -1; i = surfel_buf[i].next) {
    surfel_buf[i].prev = prev_id;
    prev_id = i;
  }

  List sorted_list;
  sorted_list.first = list_start;
  sorted_list.last = prev_id;

  if (sorted_list.first == sorted_list.last) {
    /* Only one item. Nothing to sort. */
    return;
  }

  /* Using insertion sort as it is easier to implement. */

  List unsorted_list = list_split_after(sorted_list, sorted_list.first);

  /* Mutable for-each. */
  for (int i = unsorted_list.first, next = 0; i > -1; i = next) {
    next = surfel_buf[i].next;

    bool insert = false;
    for (int j = sorted_list.first; j > -1; j = surfel_buf[j].next) {
      if (surfel_buf[j].ray_distance < surfel_buf[i].ray_distance) {
        list_insert_link_before(sorted_list, j, i);
        insert = true;
        break;
      }
    }
    if (insert == false) {
      list_add_tail(sorted_list, i);
    }
  }

  /* Update list start for irradiance sample capture. */
  list_start_buf[list_index] = sorted_list.first;

  /* Now that we have a sorted list, try to avoid connection from coplanar surfels.
   * For that we disconnect them and link them to the first non-coplanar surfel.
   * Note that this changes the list to a tree, which doesn't affect the rest of the algorithm.
   *
   * This is a really important step since it allows to clump more surfels into one ray list and
   * avoid light leaking through surfaces. If we don't disconnect coplanar surfels, we loose many
   * good rays by evaluating null radiance transfer between the coplanar surfels for rays that
   * are not directly perpendicular to the surface. */

  /* Mutable foreach. */
  for (int i = sorted_list.first, next = 0; i > -1; i = next) {
    next = surfel_buf[i].next;

    int valid_next = surfel_buf[i].next;
    int valid_prev = surfel_buf[i].prev;

    /* Search the list for the first valid next and previous surfel. */
    while (valid_next > -1) {
      if (is_valid_surfel_link(i, valid_next)) {
        break;
      }
      valid_next = surfel_buf[valid_next].next;
    }
    while (valid_prev > -1) {
      if (is_valid_surfel_link(i, valid_prev)) {
        break;
      }
      valid_prev = surfel_buf[valid_prev].prev;
    }

    surfel_buf[i].next = valid_next;
    surfel_buf[i].prev = valid_prev;
  }
}
