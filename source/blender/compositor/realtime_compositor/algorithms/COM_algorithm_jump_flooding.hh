/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_context.hh"
#include "COM_result.hh"

namespace blender::realtime_compositor {

/* Computes a jump flooding table from the given input and writes the result to the output. A jump
 * flooding table computes for each pixel the texel location of the closest "seed pixel". A seed
 * pixel is a pixel that is marked as such in the input, more on this later. This table is useful
 * to compute a Voronoi diagram where the centroids are the seed pixels, it can be used to
 * accurately approximate an euclidean distance transform, finally, it can be used to flood fill
 * regions of an image.
 *
 * The input is expected to be initialized by the initialize_jump_flooding_value function. Seed
 * pixels should specify true for the is_seed argument, and false otherwise. The texel input should
 * be the texel location of the pixel. Both the input and output results should be of type
 * ResultType::Int2.
 *
 * To compute a Voronoi diagram, the pixels lying at the centroid of the Voronoi cell should be
 * marked as seed pixels. To compute an euclidean distance transform of a region or flood fill a
 * region, the boundary pixels of the region should be marked as seed.
 *
 * The algorithm is based on the paper:
 *
 *   Rong, Guodong, and Tiow-Seng Tan. "Jump flooding in GPU with applications to Voronoi diagram
 *   and distance transform." Proceedings of the 2006 symposium on Interactive 3D graphics and
 *   games. 2006.
 *
 * But uses the more accurate 1+JFA variant from the paper:
 *
 *   Rong, Guodong, and Tiow-Seng Tan. "Variants of jump flooding algorithm for computing discrete
 *   Voronoi diagrams." 4th international symposium on voronoi diagrams in science and engineering
 *   (ISVD 2007). IEEE, 2007.*
 *
 * The algorithm is O(log2(n)) per pixel where n is the maximum dimension of the input, it follows
 * that the execution time is independent of the number of the seed pixels. However, the developer
 * should try to minimize the number of seed pixels because their number is proportional to the
 * error of the algorithm as can be seen in "Figure 3: Errors of variants of JFA" in the variants
 * paper. */
void jump_flooding(Context &context, Result &input, Result &output);

/* A special value that indicates that the pixel has not be flooded yet, and consequently is not a
 * seed pixel. */
#define JUMP_FLOODING_NON_FLOODED_VALUE int2(-1)

/* Given the texel location of the closest seed pixel and whether the pixel is flooded, encode that
 * information in an int2. */
inline int2 encode_jump_flooding_value(const int2 &closest_seed_texel, const bool is_flooded)
{
  return is_flooded ? closest_seed_texel : JUMP_FLOODING_NON_FLOODED_VALUE;
}

/* Initialize the pixel at the given texel location for the algorithm as being seed or background.
 * This essentially calls encode_jump_flooding_value with the texel location, because the pixel is
 * the closest seed to itself. */
inline int2 initialize_jump_flooding_value(const int2 &texel, const bool is_seed)
{
  return encode_jump_flooding_value(texel, is_seed);
}

}  // namespace blender::realtime_compositor
