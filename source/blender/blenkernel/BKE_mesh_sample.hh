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
 */

#pragma once

/** \file
 * \ingroup bke
 */

#include "FN_generic_virtual_array.hh"

#include "BLI_float3.hh"

#include "BKE_attribute.h"

struct Mesh;

namespace blender::bke::mesh_surface_sample {

using fn::CPPType;
using fn::GMutableSpan;
using fn::GSpan;
using fn::GVArray;

void sample_point_attribute(const Mesh &mesh,
                            Span<int> looptri_indices,
                            Span<float3> bary_coords,
                            const GVArray &data_in,
                            GMutableSpan data_out);

void sample_corner_attribute(const Mesh &mesh,
                             Span<int> looptri_indices,
                             Span<float3> bary_coords,
                             const GVArray &data_in,
                             GMutableSpan data_out);

void sample_face_attribute(const Mesh &mesh,
                           Span<int> looptri_indices,
                           const GVArray &data_in,
                           GMutableSpan data_out);

}  // namespace blender::bke::mesh_surface_sample
