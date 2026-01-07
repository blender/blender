/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 */

#include "BLI_math_axis_angle.hh"
#include "BLI_rect.h"

#include "BKE_image.hh"
#include "BKE_lib_id.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_studiolight.h"

#include "NOD_shader.h"

#include "GPU_material.hh"

#include "draw_cache.hh"
#include "draw_view_data.hh"

#include "eevee_instance.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name Viewport Override Node-Tree
 * \{ */

LookdevWorld::LookdevWorld()
{
  /* Create a dummy World data block to hold the nodetree generated for studio-lights. */
  world = BKE_id_new_nomain<blender::World>("Lookdev");

  using namespace bke;

  world->nodetree = node_tree_add_tree_embedded(
      nullptr, &world->id, "Lookdev World Nodetree", ntreeType_Shader->idname);

  bNodeTree &ntree = *world->nodetree;

  bNode &coordinate = *node_add_static_node(nullptr, ntree, SH_NODE_TEX_COORD);
  bNodeSocket &generated_sock = *node_find_socket(coordinate, SOCK_OUT, "Generated");

  bNode &transform = *node_add_static_node(nullptr, ntree, SH_NODE_VECT_TRANSFORM);
  bNodeSocket &transform_in = *node_find_socket(transform, SOCK_IN, "Vector");
  bNodeSocket &transform_out = *node_find_socket(transform, SOCK_OUT, "Vector");
  NodeShaderVectTransform &nodeprop = *static_cast<NodeShaderVectTransform *>(transform.storage);
  nodeprop.convert_from = SHD_VECT_TRANSFORM_SPACE_WORLD;
  xform_socket_ = &nodeprop.convert_to;

  node_add_link(ntree, coordinate, generated_sock, transform, transform_in);

  /* Flip Y axis because of compatibility axis flipping inside the vector transform node. */
  bNode &flip_y_mul = *node_add_static_node(nullptr, ntree, SH_NODE_VECTOR_MATH);
  flip_y_mul.custom1 = NODE_VECTOR_MATH_MULTIPLY;
  auto &flip_y_value_out = *node_find_socket(flip_y_mul, SOCK_OUT, "Vector");
  auto &flip_y_value_in0 = *static_cast<bNodeSocket *>(BLI_findlink(&flip_y_mul.inputs, 0));
  auto &flip_y_value_in1 = *static_cast<bNodeSocket *>(BLI_findlink(&flip_y_mul.inputs, 1));
  flip_y_socket_ = static_cast<bNodeSocketValueVector *>(flip_y_value_in1.default_value);
  flip_y_socket_->value[0] = 1.0f;
  flip_y_socket_->value[1] = 1.0f;
  flip_y_socket_->value[2] = 1.0f;

  node_add_link(ntree, transform, transform_out, flip_y_mul, flip_y_value_in0);

  bNode &rotate_x = *node_add_static_node(nullptr, ntree, SH_NODE_VECTOR_ROTATE);
  rotate_x.custom1 = NODE_VECTOR_ROTATE_TYPE_AXIS_X;
  auto &rotate_x_vector_in = *node_find_socket(rotate_x, SOCK_IN, "Vector");
  auto &rotate_x_vector_angle = *node_find_socket(rotate_x, SOCK_IN, "Angle");
  auto &rotate_x_out = *node_find_socket(rotate_x, SOCK_OUT, "Vector");
  rotation_x_socket_ =
      &static_cast<bNodeSocketValueFloat *>(rotate_x_vector_angle.default_value)->value;

  node_add_link(ntree, flip_y_mul, flip_y_value_out, rotate_x, rotate_x_vector_in);

  bNode &rotate_z = *node_add_static_node(nullptr, ntree, SH_NODE_VECTOR_ROTATE);
  rotate_z.custom1 = NODE_VECTOR_ROTATE_TYPE_AXIS_Z;
  auto &rotate_z_vector_in = *node_find_socket(rotate_z, SOCK_IN, "Vector");
  auto &rotate_z_vector_angle = *node_find_socket(rotate_z, SOCK_IN, "Angle");
  auto &rotate_z_out = *node_find_socket(rotate_z, SOCK_OUT, "Vector");
  angle_socket_ = static_cast<bNodeSocketValueFloat *>(rotate_z_vector_angle.default_value);

  node_add_link(ntree, rotate_x, rotate_x_out, rotate_z, rotate_z_vector_in);

  /* Discard the previous processing if we are rendering light probes. */

  bNode &light_path = *node_add_static_node(nullptr, ntree, SH_NODE_LIGHT_PATH);
  bNodeSocket &is_camera_out = *node_find_socket(light_path, SOCK_OUT, "Is Camera Ray");

  bNode &path_mix = *node_add_static_node(nullptr, ntree, SH_NODE_MIX);
  NodeShaderMix &path_mix_data = *static_cast<NodeShaderMix *>(path_mix.storage);
  path_mix_data.data_type = SOCK_VECTOR;
  path_mix_data.factor_mode = NODE_MIX_MODE_UNIFORM;
  path_mix_data.clamp_factor = false;
  auto &path_mix_out = *node_find_socket(path_mix, SOCK_OUT, "Result_Vector");
  auto &path_mix_fac = *static_cast<bNodeSocket *>(BLI_findlink(&path_mix.inputs, 0));
  auto &path_mix_in0 = *static_cast<bNodeSocket *>(BLI_findlink(&path_mix.inputs, 4));
  auto &path_mix_in1 = *static_cast<bNodeSocket *>(BLI_findlink(&path_mix.inputs, 5));

  node_add_link(ntree, light_path, is_camera_out, path_mix, path_mix_fac);
  node_add_link(ntree, coordinate, generated_sock, path_mix, path_mix_in0);
  node_add_link(ntree, rotate_z, rotate_z_out, path_mix, path_mix_in1);

  bNode &environment = *node_add_static_node(nullptr, ntree, SH_NODE_TEX_ENVIRONMENT);
  environment_node_ = &environment;
  NodeTexImage *environment_storage = static_cast<NodeTexImage *>(environment.storage);
  auto &environment_vector_in = *node_find_socket(environment, SOCK_IN, "Vector");
  auto &environment_out = *node_find_socket(environment, SOCK_OUT, "Color");

  node_add_link(ntree, path_mix, path_mix_out, environment, environment_vector_in);

  bNode &background = *node_add_static_node(nullptr, ntree, SH_NODE_BACKGROUND);
  auto &background_out = *node_find_socket(background, SOCK_OUT, "Background");
  auto &background_color_in = *node_find_socket(background, SOCK_IN, "Color");
  intensity_socket_ = static_cast<bNodeSocketValueFloat *>(
      node_find_socket(background, SOCK_IN, "Strength")->default_value);

  node_add_link(ntree, environment, environment_out, background, background_color_in);

  bNode &output = *node_add_static_node(nullptr, ntree, SH_NODE_OUTPUT_WORLD);
  auto &output_in = *node_find_socket(output, SOCK_IN, "Surface");

  node_add_link(ntree, background, background_out, output, output_in);
  node_set_active(ntree, output);

  /* Create a dummy image data block to hold GPU textures generated by studio-lights. */
  image = BKE_id_new_nomain<blender::Image>("Lookdev");
  image->type = IMA_TYPE_IMAGE;
  image->source = IMA_SRC_GENERATED;
  ImageTile *base_tile = BKE_image_get_tile(image, 0);
  base_tile->gen_x = 1;
  base_tile->gen_y = 1;
  base_tile->gen_type = IMA_GENTYPE_BLANK;
  copy_v4_fl(base_tile->gen_color, 0.0f);
  /* TODO: This works around the issue that the first time the texture is accessed the image would
   * overwrite the set GPU texture. A better solution would be to use image data-blocks as part of
   * the studio-lights, but that requires a larger refactoring. */
  BKE_image_get_gpu_texture(image, &environment_storage->iuser);
}

LookdevWorld::~LookdevWorld()
{
  BKE_id_free(nullptr, &image->id);
  BKE_id_free(nullptr, &world->id);
}

bool LookdevWorld::sync(const LookdevParameters &new_parameters)
{
  const bool parameters_changed = assign_if_different(parameters_, new_parameters);

  if (parameters_changed) {
    intensity_socket_->value = parameters_.intensity;

    GPU_TEXTURE_FREE_SAFE(image->runtime->gputexture[TEXTARGET_2D][0]);
    environment_node_->id = nullptr;

    StudioLight *sl = BKE_studiolight_find(parameters_.hdri.c_str(),
                                           STUDIOLIGHT_ORIENTATIONS_MATERIAL_MODE);
    if (sl) {
      BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_EQUIRECT_RADIANCE_GPUTEXTURE);
      gpu::Texture *texture = sl->equirect_radiance_gputexture;
      if (texture != nullptr) {
        GPU_texture_ref(texture);
        image->runtime->gputexture[TEXTARGET_2D][0] = texture;
        environment_node_->id = &image->id;
      }
    }

    if (parameters_.camera_space) {
      *xform_socket_ = SHD_VECT_TRANSFORM_SPACE_CAMERA;
      flip_y_socket_->value[0] = 1.0f;
      flip_y_socket_->value[1] = -1.0f;
      flip_y_socket_->value[2] = 1.0f;
      *rotation_x_socket_ = -M_PI / 2.0f;
    }
    else {
      *xform_socket_ = SHD_VECT_TRANSFORM_SPACE_WORLD;
      flip_y_socket_->value[0] = 1.0f;
      flip_y_socket_->value[1] = 1.0f;
      flip_y_socket_->value[2] = 1.0f;
      *rotation_x_socket_ = 0.0f;
    }
  }

  /* This isn't part of the main update check to avoid updating the probe capture.
   * This should only update the Nodetree UBO. */
  const bool rotation_changed = assign_if_different(angle_socket_->value, new_parameters.rot_z);

  if (rotation_changed || parameters_changed) {
    /* Propagate changes to nodetree. */
    GPU_material_free(&world->gpumaterial);
  }
  return parameters_changed;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lookdev
 *
 * \{ */

LookdevModule::LookdevModule(Instance &inst) : inst_(inst) {}

LookdevModule::~LookdevModule()
{
  for (gpu::Batch *batch : sphere_lod_) {
    GPU_BATCH_DISCARD_SAFE(batch);
  }
}

gpu::Batch *LookdevModule::sphere_get(const SphereLOD level_of_detail)
{
  BLI_assert(level_of_detail >= SphereLOD::LOW && level_of_detail < SphereLOD::MAX);

  /* GCC 15.x triggers an array-bounds warning without this. */
#if (defined(__GNUC__) && (__GNUC__ >= 15) && !defined(__clang__))
  [[assume((level_of_detail >= 0) && (level_of_detail < SphereLOD::MAX))]];
#endif

  if (sphere_lod_[level_of_detail] != nullptr) {
    return sphere_lod_[level_of_detail];
  }

  int lat_res;
  int lon_res;
  switch (level_of_detail) {
    case 2:
      lat_res = 80;
      lon_res = 60;
      break;
    case 1:
      lat_res = 64;
      lon_res = 48;
      break;
    default:
    case 0:
      lat_res = 32;
      lon_res = 24;
      break;
  }

  GPUVertFormat format = {0};
  GPU_vertformat_attr_add(&format, "pos", gpu::VertAttrType::SFLOAT_32_32_32);
  GPU_vertformat_attr_add(&format, "nor", gpu::VertAttrType::SFLOAT_32_32_32);
  struct Vert {
    float x, y, z;
    float nor_x, nor_y, nor_z;
  };

  gpu::VertBuf *vbo = GPU_vertbuf_create_with_format(format);
  int v_len = (lat_res - 1) * lon_res * 6;
  GPU_vertbuf_data_alloc(*vbo, v_len);

  const float lon_inc = 2 * M_PI / lon_res;
  const float lat_inc = M_PI / lat_res;
  float lon, lat;

  int v = 0;
  lon = 0.0f;

  auto sphere_lat_lon_vert = [&](float lat, float lon) {
    Vert vert;
    vert.nor_x = vert.x = sinf(lat) * cosf(lon);
    vert.nor_y = vert.y = cosf(lat);
    vert.nor_z = vert.z = sinf(lat) * sinf(lon);
    GPU_vertbuf_vert_set(vbo, v, &vert);
    v++;
  };

  for (int i = 0; i < lon_res; i++, lon += lon_inc) {
    lat = 0.0f;
    for (int j = 0; j < lat_res; j++, lat += lat_inc) {
      if (j != lat_res - 1) { /* Pole */
        sphere_lat_lon_vert(lat + lat_inc, lon + lon_inc);
        sphere_lat_lon_vert(lat + lat_inc, lon);
        sphere_lat_lon_vert(lat, lon);
      }
      if (j != 0) { /* Pole */
        sphere_lat_lon_vert(lat, lon + lon_inc);
        sphere_lat_lon_vert(lat + lat_inc, lon + lon_inc);
        sphere_lat_lon_vert(lat, lon);
      }
    }
  }

  sphere_lod_[level_of_detail] = GPU_batch_create_ex(
      GPU_PRIM_TRIS, vbo, nullptr, GPU_BATCH_OWNS_VBO);
  return sphere_lod_[level_of_detail];
}

void LookdevModule::init(const rcti *visible_rect)
{
  visible_rect_ = *visible_rect;
  use_reference_spheres_ = inst_.is_viewport() && inst_.overlays_enabled() &&
                           inst_.use_lookdev_overlay();

  if (use_reference_spheres_) {
    const int2 extent_dummy(1);
    constexpr eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_WRITE |
                                       GPU_TEXTURE_USAGE_SHADER_READ;
    dummy_cryptomatte_tx_.ensure_2d(gpu::TextureFormat::SFLOAT_32_32_32_32, extent_dummy, usage);
    dummy_aov_color_tx_.ensure_2d_array(
        gpu::TextureFormat::SFLOAT_16_16_16_16, extent_dummy, 1, usage);
    dummy_aov_value_tx_.ensure_2d_array(gpu::TextureFormat::SFLOAT_16, extent_dummy, 1, usage);
  }

  if (inst_.is_viewport()) {
    const blender::View3DShading &shading = inst_.v3d->shading;
    bool use_viewspace_lighting = (shading.flag & V3D_SHADING_STUDIOLIGHT_VIEW_ROTATION) != 0;
    if (assign_if_different(use_viewspace_lighting_, use_viewspace_lighting)) {
      inst_.sampling.reset();
    }
    if (assign_if_different(studio_light_rotation_z_, shading.studiolight_rot_z)) {
      inst_.sampling.reset();
    }
  }
}

float LookdevModule::calc_viewport_scale()
{
  const float viewport_scale = clamp_f(
      BLI_rcti_size_x(&visible_rect_) / (2000.0f * UI_SCALE_FAC), 0.5f, 1.0f);
  return viewport_scale;
}

LookdevModule::SphereLOD LookdevModule::calc_level_of_detail(const float viewport_scale)
{
  float res_scale = clamp_f(
      (U.lookdev_sphere_size / 400.0f) * viewport_scale * UI_SCALE_FAC, 0.1f, 1.0f);

  if (res_scale > 0.7f) {
    return LookdevModule::SphereLOD::HIGH;
  }
  if (res_scale > 0.25f) {
    return LookdevModule::SphereLOD::MEDIUM;
  }
  return LookdevModule::SphereLOD::LOW;
}

static int calc_sphere_extent(const float viewport_scale)
{
  const int sphere_radius = U.lookdev_sphere_size * UI_SCALE_FAC * viewport_scale;
  return sphere_radius * 2;
}

void LookdevModule::sync()
{
  if (!use_reference_spheres_) {
    return;
  }
  const float viewport_scale = calc_viewport_scale();
  const int2 extent = int2(calc_sphere_extent(viewport_scale));

  const gpu::TextureFormat color_format = gpu::TextureFormat::SFLOAT_16_16_16_16;

  for (int index : IndexRange(num_spheres)) {
    if (spheres_[index].color_tx_.ensure_2d(color_format, extent)) {
      /* Request redraw if the light-probe were off and the sampling was already finished. */
      if (inst_.is_viewport() && inst_.sampling.finished_viewport()) {
        inst_.sampling.reset();
      }
    }

    spheres_[index].framebuffer.ensure(GPU_ATTACHMENT_NONE,
                                       GPU_ATTACHMENT_TEXTURE(spheres_[index].color_tx_));
  }

  const Camera &cam = inst_.camera;
  float sphere_distance = cam.data_get().clip_near;
  int2 display_extent = inst_.film.display_extent_get();
  float pixel_radius = ShadowModule::screen_pixel_radius(
      cam.data_get().wininv, cam.is_perspective(), display_extent);

  if (cam.is_perspective()) {
    pixel_radius *= sphere_distance;
  }

  this->sphere_radius_ = (extent.x / 2) * pixel_radius;
  this->sphere_position_ = cam.position() -
                           cam.forward() * (sphere_distance + this->sphere_radius_);

  float4x4 model_m4 = float4x4(float3x3(cam.data_get().viewmat));
  model_m4.location() = this->sphere_position_;
  model_m4 = math::scale(model_m4, float3(this->sphere_radius_));

  ResourceHandleRange handle = inst_.manager->resource_handle(model_m4);
  gpu::Batch *geom = sphere_get(calc_level_of_detail(viewport_scale));

  sync_pass(spheres_[0].pass, geom, inst_.materials.metallic_mat, handle);
  sync_pass(spheres_[1].pass, geom, inst_.materials.diffuse_mat, handle);
  sync_display();
}

void LookdevModule::sync_pass(PassSimple &pass,
                              gpu::Batch *geom,
                              blender::Material *mat,
                              ResourceHandleRange res_handle)
{
  pass.init();
  pass.clear_color_depth_stencil(float4(0.0, 0.0, 0.0, 1.0), inst_.film.depth.clear_value, 0);

  const DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_CULL_BACK;

  GPUMaterial *gpumat = inst_.shaders.material_shader_get(
      mat, mat->nodetree, MAT_PIPE_FORWARD, MAT_GEOM_MESH, false, inst_.materials.default_surface);
  pass.state_set(state);
  pass.material_set(*inst_.manager, gpumat);
  pass.bind_texture(RBUFS_UTILITY_TEX_SLOT, inst_.pipelines.utility_tx);
  pass.bind_resources(inst_.uniform_data);
  pass.bind_resources(inst_.lights);
  pass.bind_resources(inst_.shadows);
  pass.bind_resources(inst_.volume.result);
  pass.bind_resources(inst_.sampling);
  pass.bind_resources(inst_.hiz_buffer.front);
  pass.bind_resources(inst_.volume_probes);
  pass.bind_resources(inst_.sphere_probes);
  pass.draw(geom, res_handle, 0);
}

void LookdevModule::sync_display()
{
  PassSimple &pass = display_ps_;

  const float2 viewport_size = inst_.draw_ctx->viewport_size_get();
  const DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS |
                         DRW_STATE_BLEND_ALPHA;
  pass.init();
  pass.state_set(state);
  pass.shader_set(inst_.shaders.static_shader_get(LOOKDEV_DISPLAY));
  pass.push_constant("viewportSize", viewport_size);
  pass.push_constant("invertedViewportSize", 1.0f / viewport_size);
  pass.push_constant("anchor", int2(visible_rect_.xmax, visible_rect_.ymin));
  pass.bind_texture("metallic_tx", &spheres_[0].color_tx_);
  pass.bind_texture("diffuse_tx", &spheres_[1].color_tx_);

  pass.draw_procedural(GPU_PRIM_TRIS, 2, 6);
}

void LookdevModule::draw(View &view)
{
  if (!use_reference_spheres_) {
    return;
  }

  inst_.volume_probes.set_view(view);
  inst_.sphere_probes.set_view(view);

  if (assign_if_different(inst_.pipelines.data.use_monochromatic_transmittance, bool32_t(true))) {
    inst_.uniform_data.push_update();
  }

  for (Sphere &sphere : spheres_) {
    sphere.framebuffer.bind();
    inst_.manager->submit(sphere.pass, view);
  }
}

void LookdevModule::rotate_world()
{
  if (!inst_.is_viewport()) {
    return;
  }

  AxisAngle axis_angle_rotation(AxisSigned::Z_NEG, studio_light_rotation_z_);
  float4x4 rotation = math::from_rotation<float4x4>(axis_angle_rotation);
  if (use_viewspace_lighting_) {
    CartesianBasis target(AxisSigned::X_POS, AxisSigned::Z_NEG, AxisSigned::Y_POS);
    rotation = inst_.camera.data_get().viewinv * math::from_rotation<float4x4>(target) * rotation;
  }

  if (assign_if_different(last_rotation_matrix_, rotation)) {
    rotate_world_probe_data(inst_.sphere_probes.octahedral_probes_texture(),
                            inst_.sphere_probes.world_sphere_probe().atlas_coord,
                            inst_.sphere_probes.spherical_harmonics_buf(),
                            inst_.world.sunlight,
                            rotation);
  }
}

void LookdevModule::display()
{
  if (!use_reference_spheres_) {
    return;
  }

  BLI_assert(inst_.is_viewport());

  DefaultFramebufferList *dfbl = inst_.draw_ctx->viewport_framebuffer_list_get();
  /* The viewport of the framebuffer can be modified when border rendering is enabled. */
  GPU_framebuffer_viewport_reset(dfbl->default_fb);
  GPU_framebuffer_bind(dfbl->default_fb);
  inst_.manager->submit(display_ps_);
}

void LookdevModule::store_world_probe_data(
    Texture &in_sphere_probe,
    const SphereProbeAtlasCoord &atlas_coord,
    StorageBuffer<SphereProbeHarmonic, true> &in_volume_probe,
    UniformArrayBuffer<LightData, 2> &in_sunlight)
{
  SphereProbeUvArea read_coord = atlas_coord.as_sampling_coord();
  SphereProbePixelArea write_coord_mip0 = atlas_coord.as_write_coord(0);
  SphereProbePixelArea write_coord_mip1 = atlas_coord.as_write_coord(1);
  SphereProbePixelArea write_coord_mip2 = atlas_coord.as_write_coord(2);
  SphereProbePixelArea write_coord_mip3 = atlas_coord.as_write_coord(3);
  SphereProbePixelArea write_coord_mip4 = atlas_coord.as_write_coord(4);

  if (world_sphere_probe_.ensure_2d_array(gpu::TextureFormat::SPHERE_PROBE_FORMAT,
                                          in_sphere_probe.size().xy(),
                                          1,
                                          GPU_TEXTURE_USAGE_GENERAL,
                                          nullptr,
                                          5))
  {
    GPU_texture_mipmap_mode(world_sphere_probe_, true, true);
    world_sphere_probe_.ensure_mip_views();
  }

  PassSimple pass = {__func__};
  pass.init();
  pass.shader_set(inst_.shaders.static_shader_get(LOOKDEV_COPY_WORLD));
  pass.push_constant("read_coord_packed", reinterpret_cast<int4 *>(&read_coord));
  pass.push_constant("write_coord_mip0_packed", reinterpret_cast<int4 *>(&write_coord_mip0));
  pass.push_constant("write_coord_mip1_packed", reinterpret_cast<int4 *>(&write_coord_mip1));
  pass.push_constant("write_coord_mip2_packed", reinterpret_cast<int4 *>(&write_coord_mip2));
  pass.push_constant("write_coord_mip3_packed", reinterpret_cast<int4 *>(&write_coord_mip3));
  pass.push_constant("write_coord_mip4_packed", reinterpret_cast<int4 *>(&write_coord_mip4));
  pass.push_constant("lookdev_rotation", float4x4::identity());
  pass.bind_texture("in_sphere_tx", in_sphere_probe);
  pass.bind_image("out_sphere_mip0", world_sphere_probe_.mip_view(0));
  pass.bind_image("out_sphere_mip1", world_sphere_probe_.mip_view(1));
  pass.bind_image("out_sphere_mip2", world_sphere_probe_.mip_view(2));
  pass.bind_image("out_sphere_mip3", world_sphere_probe_.mip_view(3));
  pass.bind_image("out_sphere_mip4", world_sphere_probe_.mip_view(4));
  pass.bind_ssbo("in_sh", in_volume_probe);
  pass.bind_ssbo("out_sh", world_volume_probe_);
  pass.bind_ssbo("in_sun", in_sunlight); /* Note only the 1st member of the array is read. */
  pass.bind_ssbo("out_sun", world_sunlight_);
  int3 dispatch_size = int3(
      int2(math::divide_ceil(int2(write_coord_mip0.extent), int2(SPHERE_PROBE_REMAP_GROUP_SIZE))),
      1);
  pass.dispatch(dispatch_size);

  inst_.manager->submit(pass);

  last_rotation_matrix_ = float4x4::identity();
}

/* This is called as soon as possible inside the frame drawing and tag world probe volume
 * to update. Volume probe update is the only thing that needs to be triggered to make sure the SH
 * are copied to all volume probes. Sphere probes are already updated by this function. */
void LookdevModule::rotate_world_probe_data(
    Texture &dst_sphere_probe,
    const SphereProbeAtlasCoord &atlas_coord,
    StorageBuffer<SphereProbeHarmonic, true> &dst_volume_probe,
    UniformArrayBuffer<LightData, 2> &dst_sunlight,
    float4x4 &rotation)
{
  SphereProbeUvArea read_coord = atlas_coord.as_sampling_coord();
  SphereProbePixelArea write_coord_mip0 = atlas_coord.as_write_coord(0);
  SphereProbePixelArea write_coord_mip1 = atlas_coord.as_write_coord(1);
  SphereProbePixelArea write_coord_mip2 = atlas_coord.as_write_coord(2);
  SphereProbePixelArea write_coord_mip3 = atlas_coord.as_write_coord(3);
  SphereProbePixelArea write_coord_mip4 = atlas_coord.as_write_coord(4);

  PassSimple pass = {__func__};
  pass.init();
  pass.shader_set(inst_.shaders.static_shader_get(LOOKDEV_COPY_WORLD));
  pass.push_constant("read_coord_packed", reinterpret_cast<int4 *>(&read_coord));
  pass.push_constant("write_coord_mip0_packed", reinterpret_cast<int4 *>(&write_coord_mip0));
  pass.push_constant("write_coord_mip1_packed", reinterpret_cast<int4 *>(&write_coord_mip1));
  pass.push_constant("write_coord_mip2_packed", reinterpret_cast<int4 *>(&write_coord_mip2));
  pass.push_constant("write_coord_mip3_packed", reinterpret_cast<int4 *>(&write_coord_mip3));
  pass.push_constant("write_coord_mip4_packed", reinterpret_cast<int4 *>(&write_coord_mip4));
  pass.push_constant("lookdev_rotation", rotation);
  pass.bind_texture("in_sphere_tx", &world_sphere_probe_);
  pass.bind_image("out_sphere_mip0", dst_sphere_probe.mip_view(0));
  pass.bind_image("out_sphere_mip1", dst_sphere_probe.mip_view(1));
  pass.bind_image("out_sphere_mip2", dst_sphere_probe.mip_view(2));
  pass.bind_image("out_sphere_mip3", dst_sphere_probe.mip_view(3));
  pass.bind_image("out_sphere_mip4", dst_sphere_probe.mip_view(4));
  pass.bind_ssbo("in_sh", world_volume_probe_);
  pass.bind_ssbo("out_sh", dst_volume_probe);
  pass.bind_ssbo("in_sun", world_sunlight_);
  pass.bind_ssbo("out_sun", dst_sunlight);
  int3 dispatch_size = int3(
      int2(math::divide_ceil(int2(write_coord_mip0.extent), int2(SPHERE_PROBE_REMAP_GROUP_SIZE))),
      1);
  pass.dispatch(dispatch_size);

  inst_.manager->submit(pass);
  /* Tag world to update the SH stored in the volume probe atlas.
   * If any volume probe is visible, this will reupload the baked data.
   * This is the costly part of this feature. */
  inst_.volume_probes.update_world_irradiance();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Parameters
 * \{ */

LookdevParameters::LookdevParameters() = default;

LookdevParameters::LookdevParameters(const blender::View3D *v3d)
{
  if (v3d == nullptr) {
    return;
  }

  const blender::View3DShading &shading = v3d->shading;
  show_scene_world = shading.type == OB_RENDER ? shading.flag & V3D_SHADING_SCENE_WORLD_RENDER :
                                                 shading.flag & V3D_SHADING_SCENE_WORLD;
  if (!show_scene_world) {
    rot_z = shading.studiolight_rot_z;
    background_opacity = shading.studiolight_background;
    blur = shading.studiolight_blur;
    intensity = shading.studiolight_intensity;
    hdri = StringRefNull(shading.lookdev_light);
    camera_space = (shading.flag & V3D_SHADING_STUDIOLIGHT_VIEW_ROTATION) != 0;
  }
}

bool LookdevParameters::operator==(const LookdevParameters &other) const
{
  return hdri == other.hdri && background_opacity == other.background_opacity &&
         blur == other.blur && intensity == other.intensity &&
         show_scene_world == other.show_scene_world && camera_space == other.camera_space;
}

bool LookdevParameters::operator!=(const LookdevParameters &other) const
{
  return !(*this == other);
}

/** \} */

}  // namespace blender::eevee
