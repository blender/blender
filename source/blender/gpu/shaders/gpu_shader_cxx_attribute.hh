/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * BSL attributes.
 *
 * Define them as standard attribute with similar placement to trigger compiler warning about typos
 * and error for misplacement.
 *
 * Actual implementation is done through the shader_tool and the attributes are not present in
 * final shader code.
 */

#pragma once

/* Specify a function is a compute shader entry point. */
#define compute maybe_unused
/* Specify a function is a vertex shader entry point. */
#define vertex maybe_unused
/* Specify a function is a fragment shader entry point. */
#define fragment maybe_unused
/* Set compute shader workgroup size. */
#define local_size(...) maybe_unused
/* Request performing fragment tests before the fragment function executes. */
#define early_fragment_tests maybe_unused

/* In a compute function, specify an input variable containing the 3-dimensional index of the local
 * work invocation within the work group that the current shader is executing in. */
#define local_invocation_id maybe_unused
/* In a compute function, specify a derived input variable containing the 3-dimensional index of
 * the work invocation within the global work group that the current shader is executing on. The
 * value is equal to work_group_id * work_group_size + local_invocation_id.  */
#define global_invocation_id maybe_unused
/* In a compute function, specify an 1-dimensional linearized index of the work invocation within
 * the work group that the current shader is executing on. */
#define local_invocation_index maybe_unused
/* In a compute function, specify an input variable containing the 3-dimensional index of the
 * global work group that the current compute shader invocation is executing within. */
#define work_group_id maybe_unused
/* In a compute function, specify an input variable containing the total number of work groups that
 * will execute for the current compute shader dispatch. */
#define num_work_groups maybe_unused

/* Specify a vertex attribute. */
#define attribute(slot) maybe_unused
/* Vertex attribute interpolation modes. */
#define flat maybe_unused
#define smooth maybe_unused
#define no_perspective maybe_unused

/* Vertex shader output position. */
#define position maybe_unused
/* Vertex shader output point size. */
#define point_size maybe_unused
/* Vertex shader output, distance from vertex to clipping plane. */
#define clip_distance maybe_unused
/* The render target array index. */
#define layer maybe_unused
/* The viewport (and scissor rectangle) index value of the primitive. */
#define viewport_index maybe_unused

/* Vertex shader input vertex index, which includes the base vertex if one is specified. */
#define vertex_id maybe_unused
/* Vertex shader input instance index, which includes the base instance if one is specified. */
#define instance_id maybe_unused
/* Vertex shader input base instance value added to each instance identifier before reading
 * per-instance data. */
#define base_instance maybe_unused

#define frag_coord maybe_unused
#define point_coord maybe_unused
#define front_facing maybe_unused

/* Fragment shader output. */
#define frag_color(slot) maybe_unused
/* Fragment shader output. */
#define frag_depth(mode) maybe_unused
/* Fragment shader output. Set stencil reference value per pixel.
 * Only supported on some platform. Check for compatibility first. */
#define frag_stencil_ref maybe_unused

/* Graphic pipeline stage in/out. */
#define in maybe_unused
#define out maybe_unused

/* Declare a dependency to a legacy create info whose name is the struct member name. */
#define legacy_info maybe_unused
/* Declare a sampler at the given slot. */
#define sampler(slot) maybe_unused
/* Declare a uniform buffer at the given slot. */
#define uniform(slot) maybe_unused
/* Declare a storage buffer at the given slot. */
#define storage(slot, qualifiers) maybe_unused
/* Declare a storage buffer at the given slot. */
#define image(slot, qualifiers, format) maybe_unused
#define compilation_constant maybe_unused
#define specialization_constant maybe_unused
#define push_constant maybe_unused
/* Declare a nested resource table member. */
#define resource_table maybe_unused
/* Only declare the member if cond evaluates to true. */
#define condition(cond) maybe_unused

/* Make a structure layout or enum shared between CPU and GPU code.
 * Required for structs defining storage and uniform buffer layout. */
#define host_shared

/* Make function callable thought the nodetree codegen system. */
#define node maybe_unused

/* Make the branch condition evaluate at compile time. */
#define static_branch likely
/* Unroll the loop at compile time. */
#define unroll likely
/**
 * Unroll the loop N time at compile time.
 * IMPORTANT: Will discard any iteration above N.
 */
#define unroll_n(N) likely
