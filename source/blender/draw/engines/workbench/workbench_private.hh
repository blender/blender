/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_context.hh"

#include "DNA_camera_types.h"
#include "DNA_material_types.h"
#include "DRW_render.hh"
#include "GPU_shader.hh"
#include "draw_manager.hh"
#include "draw_pass.hh"

#include "workbench_defines.hh"
#include "workbench_enums.hh"
#include "workbench_shader_shared.hh"

#include "GPU_capabilities.hh"

namespace blender::workbench {

using namespace draw;
using StaticShader = gpu::StaticShader;

class ShaderCache {
 private:
  StaticShader prepass_[geometry_type_len][pipeline_type_len][lighting_type_len][shader_type_len]
                       [2 /*clip*/];
  StaticShader resolve_[lighting_type_len][2 /*cavity*/][2 /*curvature*/][2 /*shadow*/];

  StaticShader shadow_[2 /*depth_pass*/][2 /*manifold*/][2 /*cap*/];

  StaticShader volume_[2 /*smoke*/][3 /*interpolation*/][2 /*coba*/][2 /*slice*/];

  static gpu::StaticShaderCache<ShaderCache> &get_static_cache()
  {
    static gpu::StaticShaderCache<ShaderCache> static_cache;
    return static_cache;
  }

 public:
  static ShaderCache &get()
  {
    return get_static_cache().get();
  }
  static void release()
  {
    get_static_cache().release();
  }

  ShaderCache();

  gpu::Shader *prepass_get(eGeometryType geometry_type,
                           ePipelineType pipeline_type,
                           eLightingType lighting_type,
                           eShaderType shader_type,
                           bool clip)
  {
    return prepass_[int(geometry_type)][int(pipeline_type)][int(lighting_type)][int(shader_type)]
                   [clip]
                       .get();
  }

  gpu::Shader *resolve_get(eLightingType lighting_type,
                           bool cavity = false,
                           bool curvature = false,
                           bool shadow = false)
  {
    return resolve_[int(lighting_type)][cavity][curvature][shadow].get();
  }

  gpu::Shader *shadow_get(bool depth_pass, bool manifold, bool cap = false)
  {
    return shadow_[depth_pass][manifold][cap].get();
  }

  gpu::Shader *volume_get(bool smoke, int interpolation, bool coba, bool slice)
  {
    return volume_[smoke][interpolation][coba][slice].get();
  }

  /* Transparency */
  StaticShader transparent_resolve = {"workbench_transparent_resolve"};
  StaticShader merge_depth = {"workbench_merge_depth"};

  /* ShadowView */
  StaticShader shadow_visibility_dynamic = {
      "workbench_shadow_visibility_compute_dynamic_pass_type"};
  StaticShader shadow_visibility_static = {"workbench_shadow_visibility_compute_static_pass_type"};

  /* Outline */
  StaticShader outline = {"workbench_effect_outline"};

  /* Dof */
  StaticShader dof_prepare = {"workbench_effect_dof_prepare"};
  StaticShader dof_downsample = {"workbench_effect_dof_downsample"};
  StaticShader dof_blur1 = {"workbench_effect_dof_blur1"};
  StaticShader dof_blur2 = {"workbench_effect_dof_blur2"};
  StaticShader dof_resolve = {"workbench_effect_dof_resolve"};

  /* AA */
  StaticShader taa_accumulation = {"workbench_taa"};
  StaticShader smaa_edge_detect = {"workbench_smaa_stage_0"};
  StaticShader smaa_aa_weight = {"workbench_smaa_stage_1"};
  StaticShader smaa_resolve = {"workbench_smaa_stage_2"};
  StaticShader overlay_depth = {"workbench_overlay_depth"};
};

struct Material {
  float3 base_color = float3(0);
  /* Packed data into a int. Decoded in the shader. */
  uint packed_data = 0;

  Material() = default;
  Material(float3 color) : base_color(color), packed_data(Material::pack_data(0.0f, 0.4f, 1.0f)) {}

  Material(::Object &ob, bool random = false);
  Material(::Material &mat)
      : base_color(&mat.r), packed_data(Material::pack_data(mat.metallic, mat.roughness, mat.a))
  {
  }

  static uint32_t pack_data(float metallic, float roughness, float alpha);

  bool is_transparent();
};

inline bool Material::is_transparent()
{
  uint32_t full_alpha_ref = 0x00ff0000;
  return (packed_data & full_alpha_ref) != full_alpha_ref;
}

inline uint32_t Material::pack_data(float metallic, float roughness, float alpha)
{
  /* Remap to Disney roughness. */
  roughness = sqrtf(roughness);
  uint32_t packed_roughness = unit_float_to_uchar_clamp(roughness);
  uint32_t packed_metallic = unit_float_to_uchar_clamp(metallic);
  uint32_t packed_alpha = unit_float_to_uchar_clamp(alpha);
  return (packed_alpha << 16u) | (packed_roughness << 8u) | packed_metallic;
}

ImageGPUTextures get_material_texture(GPUSamplerState &sampler_state);

struct SceneState {
  Scene *scene = nullptr;

  Object *camera_object = nullptr;
  Camera *camera = nullptr;
  float4x4 view_projection_matrix = float4x4::identity();
  int2 resolution = int2(0);

  eContextObjectMode object_mode = CTX_MODE_OBJECT;

  View3DShading shading = {};
  eLightingType lighting_type = eLightingType::STUDIO;
  bool xray_mode = false;

  DRWState cull_state = DRW_STATE_NO_DRAW;
  Vector<float4> clip_planes;

  float4 background_color = float4(0);

  bool draw_cavity = false;
  bool draw_curvature = false;
  bool draw_shadows = false;
  bool draw_outline = false;
  bool draw_dof = false;
  bool draw_aa = false;

  bool draw_object_id = false;

  int sample = 0;
  int samples_len = 0;
  bool reset_taa_next_sample = false;
  bool render_finished = false;

  /* Used when material_type == eMaterialType::SINGLE */
  Material material_override = Material(float3(1.0f));
  /* When r == -1.0 the shader uses the vertex color */
  Material material_attribute_color = Material(float3(-1.0f));

  void init(const DRWContext *context, bool scene_updated, Object *camera_ob = nullptr);
};

struct MaterialTexture {
  const char *name = nullptr;
  ImageGPUTextures gpu = {};
  GPUSamplerState sampler_state = GPUSamplerState::default_sampler();
  bool premultiplied = false;
  bool alpha_cutoff = false;

  MaterialTexture() = default;
  MaterialTexture(Object *ob, int material_index);
  MaterialTexture(::Image *image, ImageUser *user = nullptr);
};

struct SceneResources;

struct ObjectState {
  eV3DShadingColorType color_type = V3D_SHADING_SINGLE_COLOR;
  MaterialTexture image_paint_override = {};
  bool show_missing_texture = false;
  bool draw_shadow = false;
  bool use_per_material_batches = false;
  bool sculpt_pbvh = false;

  ObjectState(const DRWContext *draw_ctx,
              const SceneState &scene_state,
              const SceneResources &resources,
              Object *ob);
};

class CavityEffect {
 private:
  /* This value must be kept in sync with the one declared at
   * workbench_composite_info.hh (cavity_samples) */
  static const int max_samples_ = 512;

  UniformArrayBuffer<float4, max_samples_> samples_buf = {};

  int sample_ = 0;
  int sample_count_ = 0;
  bool curvature_enabled_ = false;
  bool cavity_enabled_ = false;

 public:
  void init(const SceneState &scene_state, SceneResources &resources);
  void setup_resolve_pass(PassSimple &pass, SceneResources &resources);

 private:
  void load_samples_buf(int ssao_samples);
};

struct SceneResources {
  static const int jitter_tx_size = 64;

  StringRefNull current_matcap = {};
  Texture matcap_tx = "matcap_tx";

  TextureFromPool object_id_tx = "wb_object_id_tx";

  TextureRef color_tx;
  TextureRef depth_tx;
  TextureRef depth_in_front_tx;

  Framebuffer clear_fb = {"Clear Main"};
  Framebuffer clear_depth_only_fb = {"Clear Depth"};
  Framebuffer clear_in_front_fb = {"Clear In Front"};

  StorageVectorBuffer<Material> material_buf = {"material_buf"};
  UniformBuffer<WorldData> world_buf = {};
  UniformArrayBuffer<float4, 6> clip_planes_buf;

  Texture jitter_tx = "wb_jitter_tx";

  CavityEffect cavity = {};

  Texture missing_tx = "missing_tx";
  MaterialTexture missing_texture;

  Texture dummy_texture_tx = {"dummy_texture"};
  Texture dummy_tile_data_tx = {"dummy_tile_data"};
  Texture dummy_tile_array_tx = {"dummy_tile_array"};

  gpu::Batch *volume_cube_batch = nullptr;

  ~SceneResources()
  {
    /* TODO(fclem): Auto destruction. */
    GPU_BATCH_DISCARD_SAFE(volume_cube_batch);
  }

  void init(const SceneState &scene_state, const DRWContext *ctx);
  void load_jitter_tx(int total_samples);
};

class MeshPass : public PassMain {
 private:
  struct TextureSubPassKey {
    gpu::Texture *texture;
    GPUSamplerState sampler_state;
    eGeometryType geom_type;

    uint64_t hash() const
    {
      return get_default_hash(texture, sampler_state.as_uint(), geom_type);
    }

    bool operator==(TextureSubPassKey const &rhs) const
    {
      return this->texture == rhs.texture && this->sampler_state == rhs.sampler_state &&
             this->geom_type == rhs.geom_type;
    }
  };

  Map<TextureSubPassKey, PassMain::Sub *> texture_subpass_map_;

  PassMain::Sub *passes_[geometry_type_len][shader_type_len] = {{nullptr}};

  ePipelineType pipeline_;
  eLightingType lighting_;
  bool clip_;

  bool is_empty_ = false;

  PassMain::Sub &get_subpass(eGeometryType geometry_type, eShaderType shader_type);

 public:
  MeshPass(const char *name);

  /* TODO: Move to draw::Pass */
  bool is_empty() const;

  void init_pass(SceneResources &resources, DRWState state, int clip_planes);
  void init_subpasses(ePipelineType pipeline, eLightingType lighting, bool clip);

  PassMain::Sub &get_subpass(eGeometryType geometry_type,
                             const MaterialTexture *texture = nullptr);
};

enum class StencilBits : uint8_t {
  BACKGROUND = 0,
  OBJECT = 1u << 0,
  OBJECT_IN_FRONT = 1u << 1,
};

class OpaquePass {
 public:
  TextureFromPool gbuffer_normal_tx = {"gbuffer_normal_tx"};
  TextureFromPool gbuffer_material_tx = {"gbuffer_material_tx"};

  Texture shadow_depth_stencil_tx = {"shadow_depth_stencil_tx"};
  gpu::Texture *deferred_ps_stencil_tx = nullptr;

  MeshPass gbuffer_ps_ = {"Opaque.Gbuffer"};
  MeshPass gbuffer_in_front_ps_ = {"Opaque.GbufferInFront"};
  PassSimple deferred_ps_ = {"Opaque.Deferred"};

  Framebuffer gbuffer_fb = {"Opaque.Gbuffer"};
  Framebuffer gbuffer_in_front_fb = {"Opaque.GbufferInFront"};
  Framebuffer deferred_fb = {"Opaque.Deferred"};
  Framebuffer clear_fb = {"Opaque.Clear"};

  void sync(const SceneState &scene_state, SceneResources &resources);
  void draw(Manager &manager,
            View &view,
            SceneResources &resources,
            int2 resolution,
            class ShadowPass *shadow_pass);
  bool is_empty() const;
};

class TransparentPass {
 public:
  TextureFromPool accumulation_tx = {"accumulation_accumulation_tx"};
  TextureFromPool reveal_tx = {"accumulation_reveal_tx"};
  Framebuffer transparent_fb = {};

  MeshPass accumulation_ps_ = {"Transparent.Accumulation"};
  MeshPass accumulation_in_front_ps_ = {"Transparent.AccumulationInFront"};
  PassSimple resolve_ps_ = {"Transparent.Resolve"};
  Framebuffer resolve_fb = {};

  void sync(const SceneState &scene_state, SceneResources &resources);
  void draw(Manager &manager, View &view, SceneResources &resources, int2 resolution);
  bool is_empty() const;
};

class TransparentDepthPass {
 public:
  MeshPass main_ps_ = {"TransparentDepth.Main"};
  Framebuffer main_fb = {"TransparentDepth.Main"};
  MeshPass in_front_ps_ = {"TransparentDepth.InFront"};
  Framebuffer in_front_fb = {"TransparentDepth.InFront"};
  PassSimple merge_ps_ = {"TransparentDepth.Merge"};
  Framebuffer merge_fb = {"TransparentDepth.Merge"};

  void sync(const SceneState &scene_state, SceneResources &resources);
  void draw(Manager &manager, View &view, SceneResources &resources);
  bool is_empty() const;
};

#define DEBUG_SHADOW_VOLUME 0

class ShadowPass {
 private:
  enum PassType { PASS = 0, FAIL, FORCED_FAIL, MAX };

  class ShadowView : public View {
    bool force_fail_method_ = false;
    float3 light_direction_ = float3(0);
    UniformBuffer<ExtrudedFrustum> extruded_frustum_ = {};
    ShadowPass::PassType current_pass_type_ = PASS;

    VisibilityBuf pass_visibility_buf_ = {};
    VisibilityBuf fail_visibility_buf_ = {};

   public:
    ShadowView() : View("ShadowPass.View") {};

    void setup(View &view, float3 light_direction, bool force_fail_method);
    bool debug_object_culling(Object *ob);
    void set_mode(PassType type);

   protected:
    virtual void compute_visibility(ObjectBoundsBuf &bounds,
                                    ObjectInfosBuf &infos,
                                    uint resource_len,
                                    bool debug_freeze) override;
    virtual VisibilityBuf &get_visibility_buffer() override;
  } view_ = {};

  bool enabled_;

  UniformBuffer<ShadowPassData> pass_data_ = {};

  /* Draws are added to both passes and the visibly compute shader selects one of them */
  PassMain pass_ps_ = {"Shadow.Pass"};
  PassMain fail_ps_ = {"Shadow.Fail"};

  /* In some cases, we know beforehand that we need to use the fail technique */
  PassMain forced_fail_ps_ = {"Shadow.ForcedFail"};

  /* [PassType][Is Manifold][Is Cap] */
  PassMain::Sub *passes_[PassType::MAX][2][2] = {{{nullptr}}};
  PassMain::Sub *&get_pass_ptr(PassType type, bool manifold, bool cap = false);

  TextureFromPool depth_tx_ = {};
  Framebuffer fb_ = {};

 public:
  void init(const SceneState &scene_state, SceneResources &resources);
  void update();
  void sync();
  void object_sync(SceneState &scene_state,
                   ObjectRef &ob_ref,
                   ResourceHandleRange handle,
                   const bool has_transp_mat);
  void draw(Manager &manager,
            View &view,
            SceneResources &resources,
            gpu::Texture &depth_stencil_tx,
            /* Needed when there are opaque "In Front" objects in the scene */
            bool force_fail_method);

  bool is_debug();
};

class VolumePass {
  bool active_ = true;

  PassMain ps_ = {"Volume"};
  Framebuffer fb_ = {"Volume"};

  Texture dummy_shadow_tx_ = {"Volume.Dummy Shadow Tx"};
  Texture dummy_volume_tx_ = {"Volume.Dummy Volume Tx"};
  Texture dummy_coba_tx_ = {"Volume.Dummy Coba Tx"};

  gpu::Texture *stencil_tx_ = nullptr;

 public:
  void sync(SceneResources &resources);

  void object_sync_volume(Manager &manager,
                          SceneResources &resources,
                          const SceneState &scene_state,
                          ObjectRef &ob_ref,
                          float3 color);

  void object_sync_modifier(Manager &manager,
                            SceneResources &resources,
                            const SceneState &scene_state,
                            ObjectRef &ob_ref,
                            ModifierData *md);

  void draw(Manager &manager, View &view, SceneResources &resources);

 private:
  void draw_slice_ps(Manager &manager,
                     SceneResources &resources,
                     PassMain::Sub &ps,
                     ObjectRef &ob_ref,
                     int slice_axis_enum,
                     float slice_depth);

  void draw_volume_ps(Manager &manager,
                      SceneResources &resources,
                      PassMain::Sub &ps,
                      ObjectRef &ob_ref,
                      int taa_sample,
                      float3 slice_count,
                      float3 world_size);
};

class OutlinePass {
 private:
  bool enabled_ = false;

  PassSimple ps_ = PassSimple("Workbench.Outline");
  Framebuffer fb_ = Framebuffer("Workbench.Outline");

 public:
  void init(const SceneState &scene_state);
  void sync(SceneResources &resources);
  void draw(Manager &manager, SceneResources &resources);
};

class DofPass {
 private:
  static const int kernel_radius_ = 3;
  static const int samples_len_ = (kernel_radius_ * 2 + 1) * (kernel_radius_ * 2 + 1);

  bool enabled_ = false;

  float offset_ = 0;

  UniformArrayBuffer<float4, samples_len_> samples_buf_ = {};

  Texture source_tx_ = {};
  Texture coc_halfres_tx_ = {};
  TextureFromPool blur_tx_ = {};

  Framebuffer downsample_fb_ = {};
  Framebuffer blur1_fb_ = {};
  Framebuffer blur2_fb_ = {};
  Framebuffer resolve_fb_ = {};

  PassSimple down_ps_ = {"Workbench.DoF.DownSample"};
  PassSimple down2_ps_ = {"Workbench.DoF.DownSample2"};
  PassSimple down3_ps_ = {"Workbench.DoF.DownSample3"};
  PassSimple blur_ps_ = {"Workbench.DoF.Blur"};
  PassSimple blur2_ps_ = {"Workbench.DoF.Blur2"};
  PassSimple resolve_ps_ = {"Workbench.DoF.Resolve"};

  float aperture_size_ = 0;
  float distance_ = 0;
  float invsensor_size_ = 0;
  float near_ = 0;
  float far_ = 0;
  float blades_ = 0;
  float rotation_ = 0;
  float ratio_ = 0;

 public:
  void init(const SceneState &scene_state, const DRWContext *draw_ctx);
  void sync(SceneResources &resources, const DRWContext *draw_ctx);
  void draw(Manager &manager, View &view, SceneResources &resources, int2 resolution);
  bool is_enabled();

 private:
  void setup_samples();
};

class AntiAliasingPass {
 private:
  bool enabled_ = false;
  /* Weight accumulated. */
  float weight_accum_ = 0;
  /* Samples weight for this iteration. */
  float weights_[9] = {0};
  /* Sum of weights. */
  float weights_sum_ = 0;

  Texture sample0_depth_tx_ = {"sample0_depth_tx"};
  Texture sample0_depth_in_front_tx_ = {"sample0_depth_in_front_tx"};

  Texture taa_accumulation_tx_ = {"taa_accumulation_tx"};
  Texture smaa_search_tx_ = {"smaa_search_tx"};
  Texture smaa_area_tx_ = {"smaa_area_tx"};
  TextureFromPool smaa_edge_tx_ = {"smaa_edge_tx"};
  TextureFromPool smaa_weight_tx_ = {"smaa_weight_tx"};

  Framebuffer taa_accumulation_fb_ = {"taa_accumulation_fb"};
  Framebuffer smaa_edge_fb_ = {"smaa_edge_fb"};
  Framebuffer smaa_weight_fb_ = {"smaa_weight_fb"};
  Framebuffer smaa_resolve_fb_ = {"smaa_resolve_fb"};
  Framebuffer overlay_depth_fb_ = {"overlay_depth_fb"};

  float4 smaa_viewport_metrics_ = float4(0);
  float smaa_mix_factor_ = 0;

  PassSimple taa_accumulation_ps_ = {"TAA.Accumulation"};
  PassSimple smaa_edge_detect_ps_ = {"SMAA.EdgeDetect"};
  PassSimple smaa_aa_weight_ps_ = {"SMAA.BlendWeights"};
  PassSimple smaa_resolve_ps_ = {"SMAA.Resolve"};
  PassSimple overlay_depth_ps_ = {"Overlay Depth"};

 public:
  AntiAliasingPass();

  void init(const SceneState &scene_state);
  void sync(const SceneState &scene_state, SceneResources &resources);
  void setup_view(View &view, const SceneState &scene_state);
  void draw(
      const DRWContext *draw_ctx,
      Manager &manager,
      View &view,
      const SceneState &scene_state,
      SceneResources &resources,
      /** Passed directly since we may need to copy back the results from the first sample,
       * and resources.depth_in_front_tx is only valid when mesh passes have to draw to it. */
      gpu::Texture *depth_in_front_tx);
};

}  // namespace blender::workbench
