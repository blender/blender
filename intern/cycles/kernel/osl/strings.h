/* SPDX-FileCopyrightText: 2009-2010 Sony Pictures Imageworks Inc., et al. All Rights Reserved.
 * SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Adapted code from Open Shading Language. */

#pragma once

#include "kernel/osl/types.h"

#ifndef __KERNEL_GPU__
#  include "util/param.h"
#endif

CCL_NAMESPACE_BEGIN

/* On CPU this is ustring (interned string pointer) and on GPU this is the hash.
 *
 * We don't use OSL headers for GPU, so can't rely on the constexpr to compute the hash
 * and need to hardcode it. The static assert checks it's correct and also shows the
 * value in the error message for adding new entries. */
#ifdef __KERNEL_GPU__
#  define OSL_DEVICE_STRING(name, str, hash) ccl_device_constant DeviceString name = hash;
#else
#  define OSL_DEVICE_STRING(name, str, hash) \
    static ustring name(str); \
    static_assert(OIIO::Strutil::strhash(str) == hash);
#endif

namespace DeviceStrings {

OSL_DEVICE_STRING(u_empty, "", 0ull)

OSL_DEVICE_STRING(u_common, "common", 14645198576927606093ull)
OSL_DEVICE_STRING(u_world, "world", 16436542438370751598ull)
OSL_DEVICE_STRING(u_shader, "shader", 4279676006089868ull)
OSL_DEVICE_STRING(u_object, "object", 973692718279674627ull)
OSL_DEVICE_STRING(u_ndc, "NDC", 5148305047403260775ull)
OSL_DEVICE_STRING(u_screen, "screen", 14159088609039777114ull)
OSL_DEVICE_STRING(u_camera, "camera", 2159505832145726196ull)
OSL_DEVICE_STRING(u_raster, "raster", 7759263238610201778ull)

OSL_DEVICE_STRING(u_colorsystem, "colorsystem", 1390623632464445670ull)

OSL_DEVICE_STRING(u_object_location, "object:location", 7846190347358762897ull)
OSL_DEVICE_STRING(u_object_color, "object:color", 12695623857059169556ull)
OSL_DEVICE_STRING(u_object_alpha, "object:alpha", 11165053919428293151ull)
OSL_DEVICE_STRING(u_object_index, "object:index", 6588325838217472556ull)
OSL_DEVICE_STRING(u_object_is_light, "object:is_light", 13979755312845091842ull)
OSL_DEVICE_STRING(u_object_random, "object:random", 15789063994977955884ull)

OSL_DEVICE_STRING(u_material_index, "material:index", 741770758159634623ull)

OSL_DEVICE_STRING(u_particle_index, "particle:index", 9489711748229903784ull)
OSL_DEVICE_STRING(u_particle_random, "particle:random", 17993722202766855761ull)
OSL_DEVICE_STRING(u_particle_age, "particle:age", 7380730644710951109ull)
OSL_DEVICE_STRING(u_particle_lifetime, "particle:lifetime", 16576828923156200061ull)
OSL_DEVICE_STRING(u_particle_location, "particle:location", 10309536211423573010ull)
OSL_DEVICE_STRING(u_particle_rotation, "particle:rotation", 17858543768041168459ull)
OSL_DEVICE_STRING(u_particle_size, "particle:size", 16461524249715420389ull)
OSL_DEVICE_STRING(u_particle_velocity, "particle:velocity", 13199101248768308863ull)
OSL_DEVICE_STRING(u_particle_angular_velocity,
                  "particle:angular_velocity",
                  16327930120486517910ull)

OSL_DEVICE_STRING(u_bump_map_normal, "geom:bump_map_normal", 9592102745179132106ull)
OSL_DEVICE_STRING(u_curve_length, "geom:curve_length", 11423459517663715453ull)
OSL_DEVICE_STRING(u_curve_random, "geom:curve_random", 15293085049960492358ull)
OSL_DEVICE_STRING(u_curve_tangent_normal, "geom:curve_tangent_normal", 12301397394034985633ull)
OSL_DEVICE_STRING(u_curve_thickness, "geom:curve_thickness", 10605802038397633852ull)
OSL_DEVICE_STRING(u_geom_dupli_generated, "geom:dupli_generated", 6715607178003388908ull)
OSL_DEVICE_STRING(u_geom_dupli_uv, "geom:dupli_uv", 1294253317490155849ull)
OSL_DEVICE_STRING(u_geom_name, "geom:name", 13606338128269760050ull)
OSL_DEVICE_STRING(u_geom_numpolyvertices, "geom:numpolyvertices", 382043551489988826ull)
OSL_DEVICE_STRING(u_geom_polyvertices, "geom:polyvertices", 1345577201967881769ull)
OSL_DEVICE_STRING(u_geom_trianglevertices, "geom:trianglevertices", 17839267571524187074ull)
OSL_DEVICE_STRING(u_geom_undisplaced, "geom:undisplaced", 12431586303019276305ull)
OSL_DEVICE_STRING(u_is_curve, "geom:is_curve", 129742495633653138ull)
OSL_DEVICE_STRING(u_is_point, "geom:is_point", 2511357849436175953ull)
OSL_DEVICE_STRING(u_is_smooth, "geom:is_smooth", 857544214094480123ull)
OSL_DEVICE_STRING(u_normal_map_normal, "geom:normal_map_normal", 10718948685686827073ull)
OSL_DEVICE_STRING(u_point_position, "geom:point_position", 15684484280742966916ull)
OSL_DEVICE_STRING(u_point_radius, "geom:point_radius", 9956381140398668479ull)
OSL_DEVICE_STRING(u_point_random, "geom:point_random", 5632627207092325544ull)

OSL_DEVICE_STRING(u_path_ray_length, "path:ray_length", 16391985802412544524ull)
OSL_DEVICE_STRING(u_path_ray_depth, "path:ray_depth", 16643933224879500399ull)
OSL_DEVICE_STRING(u_path_diffuse_depth, "path:diffuse_depth", 13191651286699118408ull)
OSL_DEVICE_STRING(u_path_glossy_depth, "path:glossy_depth", 15717768399057252940ull)
OSL_DEVICE_STRING(u_path_transparent_depth, "path:transparent_depth", 7821650266475578543ull)
OSL_DEVICE_STRING(u_path_transmission_depth, "path:transmission_depth", 15113408892323917624ull)
OSL_DEVICE_STRING(u_path_portal_depth, "path:portal_depth", 12594534445945123289ull)

OSL_DEVICE_STRING(u_sensor_size, "cam:sensor_size", 7525693591727141378ull)
OSL_DEVICE_STRING(u_image_resolution, "cam:image_resolution", 5199143367706113607ull)
OSL_DEVICE_STRING(u_aperture_aspect_ratio, "cam:aperture_aspect_ratio", 8708221138893210943ull)
OSL_DEVICE_STRING(u_aperture_size, "cam:aperture_size", 3708482920470008383ull)
OSL_DEVICE_STRING(u_aperture_position, "cam:aperture_position", 12926784411960338650ull)
OSL_DEVICE_STRING(u_focal_distance, "cam:focal_distance", 7162995161881858159ull)

OSL_DEVICE_STRING(u_distance, "distance", 5661183123366514158ull)
OSL_DEVICE_STRING(u_index, "index", 15907549540151602841ull)
OSL_DEVICE_STRING(u_trace, "trace", 13264932728578201327ull)
OSL_DEVICE_STRING(u_traceset_only_local, "__only_local__", 12891670648956128852ull)
OSL_DEVICE_STRING(u_hit, "hit", 7529033939518063282ull)
OSL_DEVICE_STRING(u_hitdist, "hitdist", 17066342024105335641ull)
OSL_DEVICE_STRING(u_hitself, "hitself", 12209754783026028319ull)
OSL_DEVICE_STRING(u_N, "N", 4171503098073482778ull)
OSL_DEVICE_STRING(u_Ng, "Ng", 8876595286590780628ull)
OSL_DEVICE_STRING(u_P, "P", 6583699458582498608ull)
OSL_DEVICE_STRING(u_I, "I", 939471844073562180ull)
OSL_DEVICE_STRING(u_u, "u", 24377992418299859ull)
OSL_DEVICE_STRING(u_v, "v", 5318568133543929321ull)

}  // namespace DeviceStrings

#undef OSL_DEVICE_STRING

CCL_NAMESPACE_END
