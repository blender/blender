/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_context.hh"
#include "COM_result.hh"

namespace blender::realtime_compositor {

/* Computes a jump flooding table from the given input and writes the result to the output. A jump
 * flooding table computes for each pixel the location of the closest "seed pixel" as well as the
 * distance to it. A seed pixel is a pixel that is marked as such in the input, more on this later.
 * This table is useful to compute a Voronoi diagram where the centroids are the seed pixels, it
 * can be used to accurately approximate an euclidean distance transform, finally, it can be used
 * to flood fill regions of an image.
 *
 * The input is expected to be initialized by the initialize_jump_flooding_value function from the
 * gpu_shader_compositor_jump_flooding_lib.glsl library. Seed pixels should specify true for the
 * is_seed argument, and false otherwise. The texel input should be the texel location of the
 * pixel.
 *
 * To compute a Voronoi diagram, the pixels lying at the centroid of the Voronoi cell should be
 * marked as seed pixels. To compute an euclidean distance transform of a region or flood fill a
 * region, the boundary pixels of the region should be marked as seed. The closest seed pixel and
 * the distance to it can be retrieved from the table using the extract_jump_flooding_* functions
 * from the gpu_shader_compositor_jump_flooding_lib.glsl library.
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

}  // namespace blender::realtime_compositor
