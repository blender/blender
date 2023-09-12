/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_editmesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_paint.hh"
#include "BKE_particle.h"
#include "BKE_pbvh_api.hh"
#include "BKE_report.h"
#include "DEG_depsgraph_query.h"
#include "DNA_fluid_types.h"
#include "ED_paint.hh"
#include "ED_view3d.hh"
#include "GPU_capabilities.h"
#include "IMB_imbuf_types.h"

#include "draw_common.hh"
#include "draw_sculpt.hh"

#include "workbench_private.hh"

#include "workbench_engine.h" /* Own include. */

namespace blender::workbench {

using namespace draw;

class Instance {
 public:
  View view = {"DefaultView"};

  SceneState scene_state;

  SceneResources resources;

  OpaquePass opaque_ps;
  TransparentPass transparent_ps;
  TransparentDepthPass transparent_depth_ps;

  ShadowPass shadow_ps;
  VolumePass volume_ps;
  OutlinePass outline_ps;
  DofPass dof_ps;
  AntiAliasingPass anti_aliasing_ps;

  /* An array of nullptr GPUMaterial pointers so we can call DRW_cache_object_surface_material_get.
   * They never get actually used. */
  Vector<GPUMaterial *> dummy_gpu_materials = {1, nullptr, {}};
  GPUMaterial **get_dummy_gpu_materials(int material_count)
  {
    if (material_count > dummy_gpu_materials.size()) {
      dummy_gpu_materials.resize(material_count, nullptr);
    }
    return dummy_gpu_materials.begin();
  };

  void init(Object *camera_ob = nullptr)
  {
    scene_state.init(camera_ob);
    shadow_ps.init(scene_state, resources);
    resources.init(scene_state);

    outline_ps.init(scene_state);
    dof_ps.init(scene_state);
    anti_aliasing_ps.init(scene_state);
  }

  void begin_sync()
  {
    const float2 viewport_size = DRW_viewport_size_get();
    const int2 resolution = {int(viewport_size.x), int(viewport_size.y)};
    resources.depth_tx.ensure_2d(GPU_DEPTH24_STENCIL8,
                                 resolution,
                                 GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT |
                                     GPU_TEXTURE_USAGE_MIP_SWIZZLE_VIEW);
    resources.material_buf.clear();

    opaque_ps.sync(scene_state, resources);
    transparent_ps.sync(scene_state, resources);
    transparent_depth_ps.sync(scene_state, resources);

    shadow_ps.sync();
    volume_ps.sync(resources);
    outline_ps.sync(resources);
    dof_ps.sync(resources);
    anti_aliasing_ps.sync(resources, scene_state.resolution);
  }

  void end_sync()
  {
    resources.material_buf.push_update();
  }

  Material get_material(ObjectRef ob_ref, eV3DShadingColorType color_type, int slot = 0)
  {
    switch (color_type) {
      case V3D_SHADING_OBJECT_COLOR:
        return Material(*ob_ref.object);
      case V3D_SHADING_RANDOM_COLOR:
        return Material(*ob_ref.object, true);
      case V3D_SHADING_SINGLE_COLOR:
        return scene_state.material_override;
      case V3D_SHADING_VERTEX_COLOR:
        return scene_state.material_attribute_color;
      case V3D_SHADING_TEXTURE_COLOR:
        ATTR_FALLTHROUGH;
      case V3D_SHADING_MATERIAL_COLOR:
        if (::Material *_mat = BKE_object_material_get_eval(ob_ref.object, slot)) {
          return Material(*_mat);
        }
        ATTR_FALLTHROUGH;
      default:
        return Material(*BKE_material_default_empty());
    }
  }

  void object_sync(Manager &manager, ObjectRef &ob_ref)
  {
    if (scene_state.render_finished) {
      return;
    }

    Object *ob = ob_ref.object;
    if (!DRW_object_is_renderable(ob)) {
      return;
    }

    const ObjectState object_state = ObjectState(scene_state, ob);

    /* Needed for mesh cache validation, to prevent two copies of
     * of vertex color arrays from being sent to the GPU (e.g.
     * when switching from eevee to workbench).
     */
    if (ob_ref.object->sculpt && ob_ref.object->sculpt->pbvh) {
      /* TODO(Miguel Pozo): Could this me moved to sculpt_batches_get()? */
      BKE_pbvh_is_drawing_set(ob_ref.object->sculpt->pbvh, object_state.sculpt_pbvh);
    }

    bool is_object_data_visible = (DRW_object_visibility_in_active_context(ob) &
                                   OB_VISIBLE_SELF) &&
                                  (ob->dt >= OB_SOLID || DRW_state_is_scene_render());

    if (!(ob->base_flag & BASE_FROM_DUPLI)) {
      ModifierData *md = BKE_modifiers_findby_type(ob, eModifierType_Fluid);
      if (md && BKE_modifier_is_enabled(scene_state.scene, md, eModifierMode_Realtime)) {
        FluidModifierData *fmd = (FluidModifierData *)md;
        if (fmd->domain) {
          volume_ps.object_sync_modifier(manager, resources, scene_state, ob_ref, md);

          if (fmd->domain->type == FLUID_DOMAIN_TYPE_GAS) {
            /* Do not draw solid in this case. */
            is_object_data_visible = false;
          }
        }
      }
    }

    ResourceHandle emitter_handle(0);

    if (is_object_data_visible) {
      if (object_state.sculpt_pbvh) {
        /* Disable frustum culling for sculpt meshes. */
        ResourceHandle handle = manager.resource_handle(float4x4(ob_ref.object->object_to_world));
        sculpt_sync(ob_ref, handle, object_state);
        emitter_handle = handle;
      }
      else if (ob->type == OB_MESH) {
        ResourceHandle handle = manager.resource_handle(ob_ref);
        mesh_sync(ob_ref, handle, object_state);
        emitter_handle = handle;
      }
      else if (ob->type == OB_POINTCLOUD) {
        point_cloud_sync(manager, ob_ref, object_state);
      }
      else if (ob->type == OB_CURVES) {
        curves_sync(manager, ob_ref, object_state);
      }
      else if (ob->type == OB_VOLUME) {
        if (scene_state.shading.type != OB_WIRE) {
          volume_ps.object_sync_volume(manager,
                                       resources,
                                       scene_state,
                                       ob_ref,
                                       get_material(ob_ref, object_state.color_type).base_color);
        }
      }
    }

    if (ob->type == OB_MESH && ob->modifiers.first != nullptr) {
      LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
        if (md->type != eModifierType_ParticleSystem) {
          continue;
        }
        ParticleSystem *psys = ((ParticleSystemModifierData *)md)->psys;
        if (!DRW_object_is_visible_psys_in_active_context(ob, psys)) {
          continue;
        }
        ParticleSettings *part = psys->part;
        const int draw_as = (part->draw_as == PART_DRAW_REND) ? part->ren_as : part->draw_as;

        if (draw_as == PART_DRAW_PATH) {
          hair_sync(manager, ob_ref, emitter_handle, object_state, psys, md);
        }
      }
    }
  }

  template<typename F>
  void draw_to_mesh_pass(ObjectRef &ob_ref, bool is_transparent, F draw_callback)
  {
    const bool in_front = (ob_ref.object->dtx & OB_DRAW_IN_FRONT) != 0;

    if (scene_state.xray_mode || is_transparent) {
      if (in_front) {
        draw_callback(transparent_ps.accumulation_in_front_ps_);
        draw_callback(transparent_depth_ps.in_front_ps_);
      }
      else {
        draw_callback(transparent_ps.accumulation_ps_);
        draw_callback(transparent_depth_ps.main_ps_);
      }
    }
    else {
      if (in_front) {
        draw_callback(opaque_ps.gbuffer_in_front_ps_);
      }
      else {
        draw_callback(opaque_ps.gbuffer_ps_);
      }
    }
  }

  void draw_mesh(ObjectRef &ob_ref,
                 Material &material,
                 GPUBatch *batch,
                 ResourceHandle handle,
                 ::Image *image = nullptr,
                 GPUSamplerState sampler_state = GPUSamplerState::default_sampler(),
                 ImageUser *iuser = nullptr)
  {
    resources.material_buf.append(material);
    int material_index = resources.material_buf.size() - 1;

    draw_to_mesh_pass(ob_ref, material.is_transparent(), [&](MeshPass &mesh_pass) {
      mesh_pass.get_subpass(eGeometryType::MESH, image, sampler_state, iuser)
          .draw(batch, handle, material_index);
    });
  }

  void mesh_sync(ObjectRef &ob_ref, ResourceHandle handle, const ObjectState &object_state)
  {
    bool has_transparent_material = false;

    if (object_state.use_per_material_batches) {
      const int material_count = DRW_cache_object_material_count_get(ob_ref.object);

      GPUBatch **batches;
      if (object_state.color_type == V3D_SHADING_TEXTURE_COLOR) {
        batches = DRW_cache_mesh_surface_texpaint_get(ob_ref.object);
      }
      else {
        batches = DRW_cache_object_surface_material_get(
            ob_ref.object, get_dummy_gpu_materials(material_count), material_count);
      }

      if (batches) {
        for (auto i : IndexRange(material_count)) {
          if (batches[i] == nullptr) {
            continue;
          }

          /* Material slots start from 1. */
          int material_slot = i + 1;
          Material mat = get_material(ob_ref, object_state.color_type, material_slot);
          has_transparent_material = has_transparent_material || mat.is_transparent();

          ::Image *image = nullptr;
          ImageUser *iuser = nullptr;
          GPUSamplerState sampler_state = GPUSamplerState::default_sampler();
          if (object_state.color_type == V3D_SHADING_TEXTURE_COLOR) {
            get_material_image(ob_ref.object, material_slot, image, iuser, sampler_state);
          }

          draw_mesh(ob_ref, mat, batches[i], handle, image, sampler_state, iuser);
        }
      }
    }
    else {
      GPUBatch *batch;
      if (object_state.color_type == V3D_SHADING_TEXTURE_COLOR) {
        batch = DRW_cache_mesh_surface_texpaint_single_get(ob_ref.object);
      }
      else if (object_state.color_type == V3D_SHADING_VERTEX_COLOR) {
        if (ob_ref.object->mode & OB_MODE_VERTEX_PAINT) {
          batch = DRW_cache_mesh_surface_vertpaint_get(ob_ref.object);
        }
        else {
          batch = DRW_cache_mesh_surface_sculptcolors_get(ob_ref.object);
        }
      }
      else {
        batch = DRW_cache_object_surface_get(ob_ref.object);
      }

      if (batch) {
        Material mat = get_material(ob_ref, object_state.color_type);
        has_transparent_material = has_transparent_material || mat.is_transparent();

        draw_mesh(ob_ref,
                  mat,
                  batch,
                  handle,
                  object_state.image_paint_override,
                  object_state.override_sampler_state);
      }
    }

    if (object_state.draw_shadow) {
      shadow_ps.object_sync(scene_state, ob_ref, handle, has_transparent_material);
    }
  }

  void sculpt_sync(ObjectRef &ob_ref, ResourceHandle handle, const ObjectState &object_state)
  {
    if (object_state.use_per_material_batches) {
      const int material_count = DRW_cache_object_material_count_get(ob_ref.object);
      for (SculptBatch &batch : sculpt_batches_per_material_get(
               ob_ref.object, {get_dummy_gpu_materials(material_count), material_count}))
      {
        Material mat = get_material(ob_ref, object_state.color_type, batch.material_slot);
        if (SCULPT_DEBUG_DRAW) {
          mat.base_color = batch.debug_color();
        }

        ::Image *image = nullptr;
        ImageUser *iuser = nullptr;
        GPUSamplerState sampler_state = GPUSamplerState::default_sampler();
        if (object_state.color_type == V3D_SHADING_TEXTURE_COLOR) {
          get_material_image(ob_ref.object, batch.material_slot, image, iuser, sampler_state);
        }

        draw_mesh(ob_ref, mat, batch.batch, handle, image, sampler_state);
      }
    }
    else {
      Material mat = get_material(ob_ref, object_state.color_type);
      SculptBatchFeature features = SCULPT_BATCH_DEFAULT;
      if (object_state.color_type == V3D_SHADING_VERTEX_COLOR) {
        features = SCULPT_BATCH_VERTEX_COLOR;
      }
      else if (object_state.color_type == V3D_SHADING_TEXTURE_COLOR) {
        features = SCULPT_BATCH_UV;
      }

      for (SculptBatch &batch : sculpt_batches_get(ob_ref.object, features)) {
        if (SCULPT_DEBUG_DRAW) {
          mat.base_color = batch.debug_color();
        }

        draw_mesh(ob_ref,
                  mat,
                  batch.batch,
                  handle,
                  object_state.image_paint_override,
                  object_state.override_sampler_state);
      }
    }
  }

  void point_cloud_sync(Manager &manager, ObjectRef &ob_ref, const ObjectState &object_state)
  {
    ResourceHandle handle = manager.resource_handle(ob_ref);

    Material mat = get_material(ob_ref, object_state.color_type);
    resources.material_buf.append(mat);
    int material_index = resources.material_buf.size() - 1;

    draw_to_mesh_pass(ob_ref, mat.is_transparent(), [&](MeshPass &mesh_pass) {
      PassMain::Sub &pass =
          mesh_pass.get_subpass(eGeometryType::POINTCLOUD).sub("Point Cloud SubPass");
      GPUBatch *batch = point_cloud_sub_pass_setup(pass, ob_ref.object);
      pass.draw(batch, handle, material_index);
    });
  }

  void hair_sync(Manager &manager,
                 ObjectRef &ob_ref,
                 ResourceHandle emitter_handle,
                 const ObjectState &object_state,
                 ParticleSystem *psys,
                 ModifierData *md)
  {
    /* Skip frustum culling. */
    ResourceHandle handle = manager.resource_handle(float4x4(ob_ref.object->object_to_world));

    Material mat = get_material(ob_ref, object_state.color_type, psys->part->omat);
    ::Image *image = nullptr;
    ImageUser *iuser = nullptr;
    GPUSamplerState sampler_state = GPUSamplerState::default_sampler();
    if (object_state.color_type == V3D_SHADING_TEXTURE_COLOR) {
      get_material_image(ob_ref.object, psys->part->omat, image, iuser, sampler_state);
    }
    resources.material_buf.append(mat);
    int material_index = resources.material_buf.size() - 1;

    draw_to_mesh_pass(ob_ref, mat.is_transparent(), [&](MeshPass &mesh_pass) {
      PassMain::Sub &pass = mesh_pass
                                .get_subpass(eGeometryType::CURVES, image, sampler_state, iuser)
                                .sub("Hair SubPass");
      pass.push_constant("emitter_object_id", int(emitter_handle.raw));
      GPUBatch *batch = hair_sub_pass_setup(pass, scene_state.scene, ob_ref.object, psys, md);
      pass.draw(batch, handle, material_index);
    });
  }

  void curves_sync(Manager &manager, ObjectRef &ob_ref, const ObjectState &object_state)
  {
    /* Skip frustum culling. */
    ResourceHandle handle = manager.resource_handle(float4x4(ob_ref.object->object_to_world));

    Material mat = get_material(ob_ref, object_state.color_type);
    resources.material_buf.append(mat);
    int material_index = resources.material_buf.size() - 1;

    draw_to_mesh_pass(ob_ref, mat.is_transparent(), [&](MeshPass &mesh_pass) {
      PassMain::Sub &pass = mesh_pass.get_subpass(eGeometryType::CURVES).sub("Curves SubPass");
      GPUBatch *batch = curves_sub_pass_setup(pass, scene_state.scene, ob_ref.object);
      pass.draw(batch, handle, material_index);
    });
  }

  void draw(Manager &manager,
            GPUTexture *depth_tx,
            GPUTexture *depth_in_front_tx,
            GPUTexture *color_tx)
  {
    view.sync(DRW_view_default_get());

    int2 resolution = scene_state.resolution;

    if (scene_state.render_finished) {
      /* Just copy back the already rendered result */
      anti_aliasing_ps.draw(
          manager, view, resources, resolution, depth_tx, depth_in_front_tx, color_tx);
      return;
    }

    anti_aliasing_ps.setup_view(view, resolution);

    resources.color_tx.acquire(
        resolution, GPU_RGBA16F, GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT);
    resources.color_tx.clear(resources.world_buf.background_color);
    if (scene_state.draw_object_id) {
      resources.object_id_tx.acquire(
          resolution, GPU_R16UI, GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT);
      resources.object_id_tx.clear(uint4(0));
    }

    Framebuffer fb = Framebuffer("Workbench.Clear");
    fb.ensure(GPU_ATTACHMENT_TEXTURE(resources.depth_tx));
    fb.bind();
    GPU_framebuffer_clear_depth_stencil(fb, 1.0f, 0x00);

    bool needs_depth_in_front = !transparent_ps.accumulation_in_front_ps_.is_empty() ||
                                scene_state.sample == 0;
    if (needs_depth_in_front) {
      resources.depth_in_front_tx.acquire(resolution,
                                          GPU_DEPTH24_STENCIL8,
                                          GPU_TEXTURE_USAGE_SHADER_READ |
                                              GPU_TEXTURE_USAGE_ATTACHMENT);
    }

    opaque_ps.draw(
        manager, view, resources, resolution, scene_state.draw_shadows ? &shadow_ps : nullptr);
    transparent_ps.draw(manager, view, resources, resolution);
    transparent_depth_ps.draw(manager, view, resources);

    volume_ps.draw(manager, view, resources);
    outline_ps.draw(manager, resources);
    dof_ps.draw(manager, view, resources, resolution);
    anti_aliasing_ps.draw(
        manager, view, resources, resolution, depth_tx, depth_in_front_tx, color_tx);

    resources.color_tx.release();
    resources.object_id_tx.release();
    resources.depth_in_front_tx.release();
  }

  void draw_viewport(Manager &manager,
                     GPUTexture *depth_tx,
                     GPUTexture *depth_in_front_tx,
                     GPUTexture *color_tx)
  {
    this->draw(manager, depth_tx, depth_in_front_tx, color_tx);

    if (scene_state.sample + 1 < scene_state.samples_len) {
      DRW_viewport_request_redraw();
    }
  }

  void draw_viewport_image_render(Manager &manager,
                                  GPUTexture *depth_tx,
                                  GPUTexture *depth_in_front_tx,
                                  GPUTexture *color_tx)
  {
    BLI_assert(scene_state.sample == 0);
    for (auto i : IndexRange(scene_state.samples_len)) {
      if (i != 0) {
        scene_state.sample = i;
        /* Re-sync anything dependent on scene_state.sample. */
        resources.init(scene_state);
        dof_ps.init(scene_state);
        anti_aliasing_ps.init(scene_state);
        anti_aliasing_ps.sync(resources, scene_state.resolution);
      }
      this->draw(manager, depth_tx, depth_in_front_tx, color_tx);
    }
  }
};

}  // namespace blender::workbench

/* -------------------------------------------------------------------- */
/** \name Interface with legacy C DRW manager
 * \{ */

using namespace blender;

struct WORKBENCH_Data {
  DrawEngineType *engine_type;
  DRWViewportEmptyList *fbl;
  DRWViewportEmptyList *txl;
  DRWViewportEmptyList *psl;
  DRWViewportEmptyList *stl;
  workbench::Instance *instance;

  char info[GPU_INFO_SIZE];
};

static void workbench_engine_init(void *vedata)
{
  /* TODO(fclem): Remove once it is minimum required. */
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }

  WORKBENCH_Data *ved = reinterpret_cast<WORKBENCH_Data *>(vedata);
  if (ved->instance == nullptr) {
    ved->instance = new workbench::Instance();
  }

  ved->instance->init();
}

static void workbench_cache_init(void *vedata)
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }
  reinterpret_cast<WORKBENCH_Data *>(vedata)->instance->begin_sync();
}

static void workbench_cache_populate(void *vedata, Object *object)
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }
  draw::Manager *manager = DRW_manager_get();

  draw::ObjectRef ref;
  ref.object = object;
  ref.dupli_object = DRW_object_get_dupli(object);
  ref.dupli_parent = DRW_object_get_dupli_parent(object);

  reinterpret_cast<WORKBENCH_Data *>(vedata)->instance->object_sync(*manager, ref);
}

static void workbench_cache_finish(void *vedata)
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }
  reinterpret_cast<WORKBENCH_Data *>(vedata)->instance->end_sync();
}

static void workbench_draw_scene(void *vedata)
{
  WORKBENCH_Data *ved = reinterpret_cast<WORKBENCH_Data *>(vedata);
  if (!GPU_shader_storage_buffer_objects_support()) {
    STRNCPY(ved->info, "Error: No shader storage buffer support");
    return;
  }
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  draw::Manager *manager = DRW_manager_get();
  if (DRW_state_is_viewport_image_render()) {
    ved->instance->draw_viewport_image_render(
        *manager, dtxl->depth, dtxl->depth_in_front, dtxl->color);
  }
  else {
    ved->instance->draw_viewport(*manager, dtxl->depth, dtxl->depth_in_front, dtxl->color);
  }
}

static void workbench_instance_free(void *instance)
{
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }
  delete reinterpret_cast<workbench::Instance *>(instance);
}

static void workbench_view_update(void *vedata)
{
  WORKBENCH_Data *ved = reinterpret_cast<WORKBENCH_Data *>(vedata);
  if (ved->instance) {
    ved->instance->scene_state.reset_taa_next_sample = true;
  }
}

static void workbench_id_update(void *vedata, ID *id)
{
  UNUSED_VARS(vedata, id);
}

/* RENDER */

static bool workbench_render_framebuffers_init()
{
  /* For image render, allocate own buffers because we don't have a viewport. */
  const float2 viewport_size = DRW_viewport_size_get();
  const int2 size = {int(viewport_size.x), int(viewport_size.y)};

  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  /* When doing a multi view rendering the first view will allocate the buffers
   * the other views will reuse these buffers */
  if (dtxl->color == nullptr) {
    BLI_assert(dtxl->depth == nullptr);
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_GENERAL;
    dtxl->color = GPU_texture_create_2d(
        "txl.color", size.x, size.y, 1, GPU_RGBA16F, usage, nullptr);
    dtxl->depth = GPU_texture_create_2d(
        "txl.depth", size.x, size.y, 1, GPU_DEPTH24_STENCIL8, usage, nullptr);
  }

  if (!(dtxl->depth && dtxl->color)) {
    return false;
  }

  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

  GPU_framebuffer_ensure_config(
      &dfbl->default_fb,
      {GPU_ATTACHMENT_TEXTURE(dtxl->depth), GPU_ATTACHMENT_TEXTURE(dtxl->color)});

  GPU_framebuffer_ensure_config(&dfbl->depth_only_fb,
                                {GPU_ATTACHMENT_TEXTURE(dtxl->depth), GPU_ATTACHMENT_NONE});

  GPU_framebuffer_ensure_config(&dfbl->color_only_fb,
                                {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(dtxl->color)});

  return GPU_framebuffer_check_valid(dfbl->default_fb, nullptr) &&
         GPU_framebuffer_check_valid(dfbl->color_only_fb, nullptr) &&
         GPU_framebuffer_check_valid(dfbl->depth_only_fb, nullptr);
}

#ifdef DEBUG
/* This is just to ease GPU debugging when the frame delimiter is set to Finish */
#  define GPU_FINISH_DELIMITER() GPU_finish()
#else
#  define GPU_FINISH_DELIMITER()
#endif

static void write_render_color_output(RenderLayer *layer,
                                      const char *viewname,
                                      GPUFrameBuffer *fb,
                                      const rcti *rect)
{
  RenderPass *rp = RE_pass_find_by_name(layer, RE_PASSNAME_COMBINED, viewname);
  if (rp) {
    GPU_framebuffer_bind(fb);
    GPU_framebuffer_read_color(fb,
                               rect->xmin,
                               rect->ymin,
                               BLI_rcti_size_x(rect),
                               BLI_rcti_size_y(rect),
                               4,
                               0,
                               GPU_DATA_FLOAT,
                               rp->ibuf->float_buffer.data);
  }
}

static void write_render_z_output(RenderLayer *layer,
                                  const char *viewname,
                                  GPUFrameBuffer *fb,
                                  const rcti *rect,
                                  float4x4 winmat)
{
  RenderPass *rp = RE_pass_find_by_name(layer, RE_PASSNAME_Z, viewname);
  if (rp) {
    GPU_framebuffer_bind(fb);
    GPU_framebuffer_read_depth(fb,
                               rect->xmin,
                               rect->ymin,
                               BLI_rcti_size_x(rect),
                               BLI_rcti_size_y(rect),
                               GPU_DATA_FLOAT,
                               rp->ibuf->float_buffer.data);

    int pix_num = BLI_rcti_size_x(rect) * BLI_rcti_size_y(rect);

    /* Convert GPU depth [0..1] to view Z [near..far] */
    if (DRW_view_is_persp_get(nullptr)) {
      for (float &z : MutableSpan(rp->ibuf->float_buffer.data, pix_num)) {
        if (z == 1.0f) {
          z = 1e10f; /* Background */
        }
        else {
          z = z * 2.0f - 1.0f;
          z = winmat[3][2] / (z + winmat[2][2]);
        }
      }
    }
    else {
      /* Keep in mind, near and far distance are negatives. */
      float near = DRW_view_near_distance_get(nullptr);
      float far = DRW_view_far_distance_get(nullptr);
      float range = fabsf(far - near);

      for (float &z : MutableSpan(rp->ibuf->float_buffer.data, pix_num)) {
        if (z == 1.0f) {
          z = 1e10f; /* Background */
        }
        else {
          z = z * range - near;
        }
      }
    }
  }
}

static void workbench_render_to_image(void *vedata,
                                      RenderEngine *engine,
                                      RenderLayer *layer,
                                      const rcti *rect)
{
  /* TODO(fclem): Remove once it is minimum required. */
  if (!GPU_shader_storage_buffer_objects_support()) {
    return;
  }

  if (!workbench_render_framebuffers_init()) {
    RE_engine_report(engine, RPT_ERROR, "Failed to allocate GPU buffers");
    return;
  }

  GPU_FINISH_DELIMITER();

  /* Setup */

  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Depsgraph *depsgraph = draw_ctx->depsgraph;

  WORKBENCH_Data *ved = reinterpret_cast<WORKBENCH_Data *>(vedata);
  if (ved->instance == nullptr) {
    ved->instance = new workbench::Instance();
  }

  /* TODO(sergey): Shall render hold pointer to an evaluated camera instead? */
  Object *camera_ob = DEG_get_evaluated_object(depsgraph, RE_GetCamera(engine->re));

  /* Set the perspective, view and window matrix. */
  float4x4 winmat, viewmat, viewinv;
  RE_GetCameraWindow(engine->re, camera_ob, winmat.ptr());
  RE_GetCameraModelMatrix(engine->re, camera_ob, viewinv.ptr());
  viewmat = math::invert(viewinv);

  DRWView *view = DRW_view_create(viewmat.ptr(), winmat.ptr(), nullptr, nullptr, nullptr);
  DRW_view_default_set(view);
  DRW_view_set_active(view);

  /* Render */
  do {
    if (RE_engine_test_break(engine)) {
      break;
    }

    ved->instance->init(camera_ob);

    DRW_manager_get()->begin_sync();

    workbench_cache_init(vedata);
    auto workbench_render_cache =
        [](void *vedata, Object *ob, RenderEngine * /*engine*/, Depsgraph * /*depsgraph*/) {
          workbench_cache_populate(vedata, ob);
        };
    DRW_render_object_iter(vedata, engine, depsgraph, workbench_render_cache);
    workbench_cache_finish(vedata);

    DRW_manager_get()->end_sync();

    /* Also we weed to have a correct FBO bound for #DRW_curves_update */
    // GPU_framebuffer_bind(dfbl->default_fb);
    // DRW_curves_update(); /* TODO(@pragma37): Check this once curves are implemented */

    workbench_draw_scene(vedata);

    /* Perform render step between samples to allow
     * flushing of freed GPUBackend resources. */
    GPU_render_step();
    GPU_FINISH_DELIMITER();
  } while (ved->instance->scene_state.sample + 1 < ved->instance->scene_state.samples_len);

  const char *viewname = RE_GetActiveRenderView(engine->re);
  write_render_color_output(layer, viewname, dfbl->default_fb, rect);
  write_render_z_output(layer, viewname, dfbl->default_fb, rect, winmat);
}

static void workbench_render_update_passes(RenderEngine *engine,
                                           Scene *scene,
                                           ViewLayer *view_layer)
{
  if (view_layer->passflag & SCE_PASS_COMBINED) {
    RE_engine_register_pass(engine, scene, view_layer, RE_PASSNAME_COMBINED, 4, "RGBA", SOCK_RGBA);
  }
  if (view_layer->passflag & SCE_PASS_Z) {
    RE_engine_register_pass(engine, scene, view_layer, RE_PASSNAME_Z, 1, "Z", SOCK_FLOAT);
  }
}

extern "C" {

static const DrawEngineDataSize workbench_data_size = DRW_VIEWPORT_DATA_SIZE(WORKBENCH_Data);

DrawEngineType draw_engine_workbench = {
    /*next*/ nullptr,
    /*prev*/ nullptr,
    /*idname*/ N_("Workbench"),
    /*vedata_size*/ &workbench_data_size,
    /*engine_init*/ &workbench_engine_init,
    /*engine_free*/ nullptr,
    /*instance_free*/ &workbench_instance_free,
    /*cache_init*/ &workbench_cache_init,
    /*cache_populate*/ &workbench_cache_populate,
    /*cache_finish*/ &workbench_cache_finish,
    /*draw_scene*/ &workbench_draw_scene,
    /*view_update*/ &workbench_view_update,
    /*id_update*/ &workbench_id_update,
    /*render_to_image*/ &workbench_render_to_image,
    /*store_metadata*/ nullptr,
};

RenderEngineType DRW_engine_viewport_workbench_type = {
    /*next*/ nullptr,
    /*prev*/ nullptr,
    /*idname*/ "BLENDER_WORKBENCH",
    /*name*/ N_("Workbench"),
    /*flag*/ RE_INTERNAL | RE_USE_STEREO_VIEWPORT | RE_USE_GPU_CONTEXT,
    /*update*/ nullptr,
    /*render*/ &DRW_render_to_image,
    /*render_frame_finish*/ nullptr,
    /*draw*/ nullptr,
    /*bake*/ nullptr,
    /*view_update*/ nullptr,
    /*view_draw*/ nullptr,
    /*update_script_node*/ nullptr,
    /*update_render_passes*/ &workbench_render_update_passes,
    /*draw_engine*/ &draw_engine_workbench,
    /*rna_ext*/
    {
        /*data*/ nullptr,
        /*srna*/ nullptr,
        /*call*/ nullptr,
    },
};
}

/** \} */
