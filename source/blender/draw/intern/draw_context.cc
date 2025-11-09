/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include <cstdio>

#include "CLG_log.h"

#include "BLI_enum_flags.hh"
#include "BLI_function_ref.hh"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_sys_types.h"
#include "BLI_task.h"
#include "BLI_threads.h"

#include "BLF_api.hh"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_curve.hh"
#include "BKE_curves.h"
#include "BKE_duplilist.hh"
#include "BKE_editmesh.hh"
#include "BKE_global.hh"
#include "BKE_grease_pencil.h"
#include "BKE_idprop.hh"
#include "BKE_lattice.hh"
#include "BKE_layer.hh"
#include "BKE_main.hh"
#include "BKE_mball.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_modifier.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"
#include "BKE_paint.hh"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_pointcloud.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"
#include "BKE_subdiv_modifier.hh"
#include "BKE_volume.hh"

#include "DNA_camera_types.h"
#include "DNA_mesh_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "ED_gpencil_legacy.hh"
#include "ED_screen.hh"
#include "ED_space_api.hh"
#include "ED_view3d.hh"

#include "GPU_capabilities.hh"
#include "GPU_framebuffer.hh"
#include "GPU_matrix.hh"
#include "GPU_platform.hh"
#include "GPU_shader_shared.hh"
#include "GPU_state.hh"
#include "GPU_uniform_buffer.hh"
#include "GPU_viewport.hh"

#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "WM_api.hh"

#include "DRW_render.hh"
#include "draw_cache.hh"
#include "draw_color_management.hh"
#include "draw_common_c.hh"
#include "draw_context_private.hh"
#include "draw_handle.hh"
#include "draw_manager_text.hh"
#include "draw_shader.hh"
#include "draw_subdivision.hh"
#include "draw_view_c.hh"
#include "draw_view_data.hh"

/* only for callbacks */
#include "draw_cache_impl.hh"

#include "engines/compositor/compositor_engine.h"
#include "engines/eevee/eevee_engine.h"
#include "engines/external/external_engine.h"
#include "engines/gpencil/gpencil_engine.hh"
#include "engines/image/image_engine.h"
#include "engines/overlay/overlay_engine.h"
#include "engines/select/select_engine.hh"
#include "engines/workbench/workbench_engine.h"

#include "GPU_context.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "BLI_time.h"

#include "DRW_select_buffer.hh"

thread_local DRWContext *DRWContext::g_context = nullptr;

DRWContext::DRWContext(Mode mode_,
                       Depsgraph *depsgraph,
                       const int2 size,
                       const bContext *C,
                       ARegion *region,
                       View3D *v3d)
    : mode(mode_)
{
  BLI_assert(size.x > 0 && size.y > 0);

  this->size = float2(size);
  this->inv_size = 1.0f / this->size;

  this->depsgraph = depsgraph;
  this->scene = DEG_get_evaluated_scene(depsgraph);
  this->view_layer = DEG_get_evaluated_view_layer(depsgraph);

  this->evil_C = C;

  this->region = (region) ? region : ((C) ? CTX_wm_region(C) : nullptr);
  this->space_data = (C) ? CTX_wm_space_data(C) : nullptr;
  this->v3d = (v3d) ? v3d : ((C) ? CTX_wm_view3d(C) : nullptr);
  if (this->v3d != nullptr && this->region != nullptr) {
    this->rv3d = static_cast<RegionView3D *>(this->region->regiondata);
  }
  /* Active object. Set to nullptr for render (when region is nullptr). */
  this->obact = (this->region) ? BKE_view_layer_active_object_get(this->view_layer) : nullptr;
  /* Object mode. */
  this->object_mode = (this->obact) ? eObjectMode(this->obact->mode) : OB_MODE_OBJECT;
  /* Edit object. */
  this->object_edit = (this->object_mode & OB_MODE_EDIT) ? this->obact : nullptr;
  /* Pose object. */
  if (this->object_mode & OB_MODE_POSE) {
    this->object_pose = this->obact;
  }
  else if (this->object_mode & OB_MODE_ALL_WEIGHT_PAINT) {
    this->object_pose = BKE_object_pose_armature_get(this->obact);
  }
  else {
    this->object_pose = nullptr;
  }

  /* View layer can be lazily synced. */
  BKE_view_layer_synced_ensure(this->scene, this->view_layer);

  /* fclem: Is this still needed ? */
  if (this->object_edit && rv3d) {
    ED_view3d_init_mats_rv3d(this->object_edit, rv3d);
  }

  BLI_assert(g_context == nullptr);
  g_context = this;
}

DRWContext::DRWContext(Mode mode_,
                       Depsgraph *depsgraph,
                       GPUViewport *viewport,
                       const bContext *C,
                       ARegion *region,
                       View3D *v3d)
    : DRWContext(mode_,
                 depsgraph,
                 int2(GPU_texture_width(GPU_viewport_color_texture(viewport, 0)),
                      GPU_texture_height(GPU_viewport_color_texture(viewport, 0))),
                 C,
                 region,
                 v3d)
{
  this->viewport = viewport;

  blender::draw::color_management::viewport_color_management_set(*viewport, *this);
}

DRWContext::~DRWContext()
{
  BLI_assert(g_context == this);
  g_context = nullptr;
}

blender::gpu::FrameBuffer *DRWContext::default_framebuffer()
{
  return view_data_active->dfbl.default_fb;
}

DefaultFramebufferList *DRWContext::viewport_framebuffer_list_get() const
{
  return const_cast<DefaultFramebufferList *>(&view_data_active->dfbl);
}

DefaultTextureList *DRWContext::viewport_texture_list_get() const
{
  return const_cast<DefaultTextureList *>(&view_data_active->dtxl);
}

static bool draw_show_annotation()
{
  DRWContext &draw_ctx = drw_get();
  SpaceLink *space_data = draw_ctx.space_data;
  View3D *v3d = draw_ctx.v3d;

  if (space_data != nullptr) {
    switch (space_data->spacetype) {
      case SPACE_IMAGE: {
        SpaceImage *sima = (SpaceImage *)space_data;
        return (sima->flag & SI_SHOW_GPENCIL) != 0;
      }
      case SPACE_NODE:
        /* Don't draw the annotation for the node editor. Annotations are handled by space_image as
         * the draw manager is only used to draw the background. */
        return false;
      default:
        break;
    }
  }
  return (v3d && ((v3d->flag2 & V3D_SHOW_ANNOTATION) != 0) &&
          ((v3d->flag2 & V3D_HIDE_OVERLAYS) == 0));
}

/* -------------------------------------------------------------------- */
/** \name Threaded Extraction
 * \{ */

struct ExtractionGraph {
 public:
  TaskGraph *graph = BLI_task_graph_create();

 private:
  /* WORKAROUND: BLI_gset_free is not allowing to pass a data pointer to the free function. */
  static thread_local TaskGraph *task_graph_ptr_;

 public:
  ~ExtractionGraph()
  {
    BLI_assert_msg(graph == nullptr, "Missing call to work_and_wait");
  }

  /* `delayed_extraction` is a set of object to add to the graph before running.
   * The non-null, the set is consumed and freed after use. */
  void work_and_wait(GSet *&delayed_extraction)
  {
    BLI_assert_msg(graph, "Trying to submit more than once");

    if (delayed_extraction) {
      task_graph_ptr_ = graph;
      BLI_gset_free(delayed_extraction, delayed_extraction_free_callback);
      task_graph_ptr_ = nullptr;
      delayed_extraction = nullptr;
    }

    BLI_task_graph_work_and_wait(graph);
    BLI_task_graph_free(graph);
    graph = nullptr;
  }

 private:
  static void delayed_extraction_free_callback(void *object)
  {
    blender::draw::drw_batch_cache_generate_requested_evaluated_mesh_or_curve(
        reinterpret_cast<Object *>(object), *task_graph_ptr_);
  }
};

thread_local TaskGraph *ExtractionGraph::task_graph_ptr_ = nullptr;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Settings
 * \{ */

bool DRW_object_is_renderable(const Object *ob)
{
  BLI_assert((ob->base_flag & BASE_ENABLED_AND_MAYBE_VISIBLE_IN_VIEWPORT) != 0);

  if (ob->type == OB_MESH) {
    DRWContext &draw_ctx = drw_get();
    /* The evaluated object might be a mesh even though the original object has a different type.
     * Also make sure the original object is a mesh (see #140762). */
    if (draw_ctx.object_edit && draw_ctx.object_edit->type != OB_MESH) {
      /* Noop. */
    }
    else if ((ob == draw_ctx.object_edit) || ob->mode == OB_MODE_EDIT) {
      View3D *v3d = draw_ctx.v3d;
      if (v3d && ((v3d->flag2 & V3D_HIDE_OVERLAYS) == 0) && RETOPOLOGY_ENABLED(v3d)) {
        return false;
      }
    }
  }

  return true;
}

bool DRW_object_is_in_edit_mode(const Object *ob)
{
  if (BKE_object_is_in_editmode(ob)) {
    if (ELEM(ob->type, OB_MESH, OB_CURVES)) {
      if ((ob->mode & OB_MODE_EDIT) == 0) {
        return false;
      }
    }
    return true;
  }
  return false;
}

int DRW_object_visibility_in_active_context(const Object *ob)
{
  const eEvaluationMode mode = DRW_context_get()->is_scene_render() ? DAG_EVAL_RENDER :
                                                                      DAG_EVAL_VIEWPORT;
  return BKE_object_visibility(ob, mode);
}

bool DRW_object_use_hide_faces(const Object *ob)
{
  if (ob->type == OB_MESH) {
    switch (ob->mode) {
      case OB_MODE_SCULPT:
      case OB_MODE_TEXTURE_PAINT:
      case OB_MODE_VERTEX_PAINT:
      case OB_MODE_WEIGHT_PAINT:
        return true;
    }
  }

  return false;
}

bool DRW_object_is_visible_psys_in_active_context(const Object *object, const ParticleSystem *psys)
{
  const bool for_render = DRW_context_get()->is_image_render();
  /* NOTE: psys_check_enabled is using object and particle system for only
   * reading, but is using some other functions which are more generic and
   * which are hard to make const-pointer. */
  if (!psys_check_enabled((Object *)object, (ParticleSystem *)psys, for_render)) {
    return false;
  }
  const DRWContext *draw_ctx = DRW_context_get();
  const Scene *scene = draw_ctx->scene;
  if (object == draw_ctx->object_edit) {
    return false;
  }
  const ParticleSettings *part = psys->part;
  const ParticleEditSettings *pset = &scene->toolsettings->particle;
  if (object->mode == OB_MODE_PARTICLE_EDIT) {
    if (psys_in_edit_mode(draw_ctx->depsgraph, psys)) {
      if ((pset->flag & PE_DRAW_PART) == 0) {
        return false;
      }
      if ((part->childtype == 0) &&
          (psys->flag & PSYS_HAIR_DYNAMICS && psys->pointcache->flag & PTCACHE_BAKED) == 0)
      {
        return false;
      }
    }
  }
  return true;
}

const Mesh *DRW_object_get_editmesh_cage_for_drawing(const Object &object)
{
  /* Same as DRW_object_get_data_for_drawing, but for the cage mesh. */
  BLI_assert(object.type == OB_MESH);
  const Mesh *cage_mesh = BKE_object_get_editmesh_eval_cage(&object);
  if (cage_mesh == nullptr) {
    return nullptr;
  }

  if (BKE_subsurf_modifier_has_gpu_subdiv(cage_mesh)) {
    return cage_mesh;
  }
  return BKE_mesh_wrapper_ensure_subdivision(cage_mesh);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Viewport (DRW_viewport)
 * \{ */

DRWData *DRW_viewport_data_create()
{
  DRWData *drw_data = MEM_callocN<DRWData>("DRWData");

  drw_data->default_view = new blender::draw::View("DrawDefaultView");

  for (int i = 0; i < 2; i++) {
    drw_data->view_data[i] = new DRWViewData();
  }
  return drw_data;
}

void DRWData::modules_init()
{
  using namespace blender::draw;
  DRW_pointcloud_init(this);
  DRW_curves_init(this);
  DRW_volume_init(this);
}

void DRWData::modules_begin_sync()
{
  using namespace blender::draw;
  DRW_curves_begin_sync(this);
  DRW_smoke_begin_sync(this);
}

void DRWData::modules_exit()
{
  DRW_smoke_exit(this);
}

void DRW_viewport_data_free(DRWData *drw_data)
{
  for (int i = 0; i < 2; i++) {
    delete drw_data->view_data[i];
  }
  DRW_volume_module_free(drw_data->volume_module);
  DRW_pointcloud_module_free(drw_data->pointcloud_module);
  DRW_curves_module_free(drw_data->curves_module);
  delete drw_data->default_view;
  MEM_freeN(drw_data);
}

static DRWData *drw_viewport_data_ensure(GPUViewport *viewport)
{
  DRWData **data_p = GPU_viewport_data_get(viewport);
  DRWData *data = *data_p;

  if (data == nullptr) {
    *data_p = data = DRW_viewport_data_create();
  }
  return data;
}

void DRWContext::acquire_data()
{
  BLI_assert(GPU_context_active_get() != nullptr);

  blender::gpu::TexturePool::get().reset();

  {
    /* Acquire DRWData. */
    if (!this->viewport && this->data) {
      /* Manager was init first without a viewport, created DRWData, but is being re-init.
       * In this case, keep the old data. */
    }
    else if (this->viewport) {
      /* Use viewport's persistent DRWData. */
      this->data = drw_viewport_data_ensure(this->viewport);
    }
    else {
      /* Create temporary DRWData. Freed in drw_manager_exit(). */
      this->data = DRW_viewport_data_create();
    }
    int view = (this->viewport) ? GPU_viewport_active_view_get(this->viewport) : 0;
    this->view_data_active = this->data->view_data[view];

    this->view_data_active->texture_list_size_validate(int2(this->size));

    if (this->viewport) {
      DRW_view_data_default_lists_from_viewport(this->view_data_active, this->viewport);
    }
  }
  {
    /* Create the default view. */
    if (this->rv3d != nullptr) {
      blender::draw::View::default_set(float4x4(this->rv3d->viewmat),
                                       float4x4(this->rv3d->winmat));
    }
    else if (this->region) {
      /* Assume that if rv3d is nullptr, we are drawing for a 2D area. */
      View2D *v2d = &this->region->v2d;
      rctf region_space = {0.0f, 1.0f, 0.0f, 1.0f};

      float4x4 viewmat;
      BLI_rctf_transform_calc_m4_pivot_min(&v2d->cur, &region_space, viewmat.ptr());

      float4x4 winmat = float4x4::identity();
      winmat[0][0] = winmat[1][1] = 2.0f;
      winmat[3][0] = winmat[3][1] = -1.0f;

      blender::draw::View::default_set(viewmat, winmat);
    }
    else {
      /* Assume that this is the render mode or custom mode and
       * that the default view will be set appropriately or not used. */
      BLI_assert(this->is_image_render() || this->mode == DRWContext::CUSTOM);
    }
  }

  /* Init modules ahead of time because the begin_sync happens before DRW_render_object_iter. */
  this->data->modules_init();
}

void DRWContext::release_data()
{
  BLI_assert(GPU_context_active_get() != nullptr);

  this->data->modules_exit();

  /* Reset drawing state to avoid to side-effects. */
  blender::draw::command::StateSet::set();

  DRW_view_data_reset(this->view_data_active);

  if (this->data != nullptr && this->viewport == nullptr) {
    DRW_viewport_data_free(this->data);
  }
  this->data = nullptr;
  this->viewport = nullptr;
}

blender::draw::TextureFromPool &DRW_viewport_pass_texture_get(const char *pass_name)
{
  return *drw_get().view_data_active->viewport_compositor_passes.lookup_or_add_cb(
      pass_name, [&]() { return std::make_unique<blender::draw::TextureFromPool>(pass_name); });
}

void DRW_viewport_request_redraw()
{
  if (drw_get().viewport) {
    GPU_viewport_tag_update(drw_get().viewport);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplis
 * \{ */

/* The Dupli systems generate a lot of transient objects that share the batch caches.
 * So we ensure to only clear and generate the cache once per source instance type using this
 * set. */
/* TODO(fclem): This should be reconsidered as this has some unneeded overhead and complexity.
 * Maybe it isn't needed at all. */
struct DupliCacheManager {
 private:
  /* Key identifying a single instance source. */
  struct DupliKey {
    Object *ob = nullptr;
    ID *ob_data = nullptr;

    DupliKey() = default;
    DupliKey(const DupliObject *ob_dupli) : ob(ob_dupli->ob), ob_data(ob_dupli->ob_data) {}

    uint64_t hash() const
    {
      return blender::get_default_hash(this->ob, this->ob_data);
    }

    friend bool operator==(const DupliKey &a, const DupliKey &b)
    {
      return a.ob == b.ob && a.ob_data == b.ob_data;
    }
  };

  /* Last key used. Allows to avoid the overhead of polling the `dupli_set` for each instance.
   * This helps when a Dupli system generates a lot of similar geometry consecutively. */
  DupliKey last_key_ = {};

  /* Set containing all visited Dupli source object. */
  blender::Set<DupliKey> *dupli_set_ = nullptr;

 public:
  void try_add(blender::draw::ObjectRef &ob_ref);
  void extract_all(ExtractionGraph &extraction);
};

void DupliCacheManager::try_add(blender::draw::ObjectRef &ob_ref)
{
  if (ob_ref.is_dupli() == false) {
    return;
  }
  if (last_key_ == ob_ref.dupli_object_) {
    /* Same data as previous iteration. No need to perform the check again. */
    return;
  }

  last_key_ = ob_ref.dupli_object_;

  if (dupli_set_ == nullptr) {
    dupli_set_ = MEM_new<blender::Set<DupliKey>>("DupliCacheManager::dupli_set_");
  }

  if (dupli_set_->add(last_key_)) {
    /* Key is newly added. It is the first time we sync this object. */
    /* TODO: Meh a bit out of place but this is nice as it is
     * only done once per instance type. */
    /* Note that this can happen for geometry data whose type is different from the original
     * object (e.g. Text evaluated as Mesh, Geometry node instance etc...).
     * In this case, key.ob is not going to have the same data type as ob_ref.object nor the same
     * data at all. */
    blender::draw::drw_batch_cache_validate(ob_ref.object);
  }
}

void DupliCacheManager::extract_all(ExtractionGraph &extraction)
{
  /* Reset for next iter. */
  last_key_ = {};

  if (dupli_set_ == nullptr) {
    return;
  }

  /* Note these can referenced by the temporary object pointer `Object *ob` and needs to have at
   * least the same lifetime. */
  blender::bke::ObjectRuntime tmp_runtime;
  Object tmp_object;

  using Iter = blender::Set<DupliKey>::Iterator;
  Iter begin = dupli_set_->begin();
  Iter end = dupli_set_->end();
  for (Iter iter = begin; iter != end; ++iter) {
    const DupliKey &key = *iter;
    Object *ob = iter->ob;

    if (key.ob_data != ob->data) {
      /* Copy both object data and runtime. */
      tmp_runtime = *ob->runtime;
      tmp_object = blender::dna::shallow_copy(*ob);
      tmp_object.runtime = &tmp_runtime;
      /* Geometry instances shouldn't be rendered with edit mode overlays. */
      tmp_object.mode = OB_MODE_OBJECT;
      /* Do not modify the original bound-box. */
      BKE_object_replace_data_on_shallow_copy(&tmp_object, key.ob_data);

      ob = &tmp_object;
    }

    blender::draw::drw_batch_cache_generate_requested(ob, *extraction.graph);
  }

  /* TODO(fclem): Could eventually keep the set allocated. */
  MEM_SAFE_DELETE(dupli_set_);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ObjectRef
 * \{ */

namespace blender::draw {

ObjectRef::ObjectRef(Object *ob, Object *dupli_parent, DupliObject *dupli_object)
    : dupli_object_(dupli_object), dupli_parent_(dupli_parent), object(ob)
{
}

ObjectRef::ObjectRef(Object &ob, Object *dupli_parent, const VectorList<DupliObject *> &duplis)
    : dupli_object_(duplis[0]), dupli_parent_(dupli_parent), duplis_(&duplis), object(&ob)
{
}

}  // namespace blender::draw

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scene Iteration
 * \{ */

namespace blender::draw {

static bool supports_handle_ranges(DupliObject *dupli, Object *parent)
{
  int ob_type = dupli->ob_data ? BKE_object_obdata_to_type(dupli->ob_data) : OB_EMPTY;
  if (!ELEM(ob_type, OB_MESH, OB_CURVES_LEGACY, OB_SURF, OB_FONT, OB_POINTCLOUD, OB_GREASE_PENCIL))
  {
    return false;
  }

  Object *ob = dupli->ob;
  if (min(ob->dt, parent->dt) == OB_BOUNDBOX) {
    return false;
  }

  if (ob_type == OB_MESH) {
    /* Hair drawing doesn't support handle ranges. */
    LISTBASE_FOREACH (ParticleSystem *, psys, &ob->particlesystem) {
      const int draw_as = (psys->part->draw_as == PART_DRAW_REND) ? psys->part->ren_as :
                                                                    psys->part->draw_as;
      if (draw_as == PART_DRAW_PATH && DRW_object_is_visible_psys_in_active_context(ob, psys)) {
        return false;
      }
    }
    /* Smoke drawing doesn't support handle ranges. */
    return !BKE_modifiers_findby_type(ob, eModifierType_Fluid);
  }

  if (ob_type == OB_GREASE_PENCIL) {
    GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(dupli->ob_data);
    return grease_pencil->flag & GREASE_PENCIL_STROKE_ORDER_3D;
  }

  return true;
}

enum class InstancesFlags : uint8_t {
  IsNegativeScale = 1 << 0,
};
ENUM_OPERATORS(InstancesFlags);

struct InstancesKey {
  uint64_t hash_value;

  Object *object;
  ID *ob_data;
  const blender::bke::GeometrySet *preview_base_geometry;
  int preview_instance_index;
  InstancesFlags flags;

  InstancesKey(Object *object,
               ID *ob_data,
               InstancesFlags flags,
               const blender::bke::GeometrySet *preview_base_geometry,
               int preview_instance_index)
      : object(object),
        ob_data(ob_data),
        preview_base_geometry(preview_base_geometry),
        preview_instance_index(preview_instance_index),
        flags(flags)
  {
    hash_value = get_default_hash(object);
    hash_value = get_default_hash(hash_value, ob_data);
    hash_value = get_default_hash(hash_value, preview_base_geometry);
    hash_value = get_default_hash(hash_value, preview_instance_index);
    hash_value = get_default_hash(hash_value, uint8_t(flags));
  }

  uint64_t hash() const
  {
    return hash_value;
  }

  bool operator==(const InstancesKey &k) const
  {
    if (hash_value != k.hash_value) {
      return false;
    }
    if (object != k.object) {
      return false;
    }
    if (ob_data != k.ob_data) {
      return false;
    }
    if (flags != k.flags) {
      return false;
    }
    if (preview_base_geometry != k.preview_base_geometry) {
      return false;
    }
    if (preview_instance_index != k.preview_instance_index) {
      return false;
    }
    return true;
  }
};

static void foreach_obref_in_scene(DRWContext &draw_ctx,
                                   FunctionRef<bool(Object &)> should_draw_object_cb,
                                   FunctionRef<void(ObjectRef &)> draw_object_cb)
{
  DupliList duplilist;
  Map<InstancesKey, VectorList<DupliObject *>> dupli_map;

  Object tmp_object;
  ObjectRuntimeHandle tmp_runtime;

  Depsgraph *depsgraph = draw_ctx.depsgraph;
  eEvaluationMode eval_mode = DEG_get_mode(depsgraph);
  View3D *v3d = draw_ctx.v3d;

  /* EEVEE is not supported for now. */
  const bool engines_support_handle_ranges = (v3d && v3d->shading.type <= OB_SOLID) ||
                                             BKE_scene_uses_blender_workbench(draw_ctx.scene);

  DEGObjectIterSettings deg_iter_settings = {nullptr};
  deg_iter_settings.depsgraph = depsgraph;
  deg_iter_settings.flags = DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY |
                            DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET;
  if (v3d && v3d->flag2 & V3D_SHOW_VIEWER) {
    deg_iter_settings.viewer_path = &v3d->viewer_path;
  }

  DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, ob) {

    if (!DEG_iterator_object_is_visible(eval_mode, ob)) {
      continue;
    }

    int visibility = BKE_object_visibility(ob, eval_mode);
    bool ob_visible = visibility & (OB_VISIBLE_SELF | OB_VISIBLE_PARTICLES);

    if (ob_visible && should_draw_object_cb(*ob)) {
      /* NOTE: object_duplilist_preview is still handled by DEG_OBJECT_ITER,
       * dupli_parent and dupli_object_current won't be null for these. */
      ObjectRef ob_ref(ob, data_.dupli_parent, data_.dupli_object_current);
      draw_object_cb(ob_ref);
    }

    bool is_preview_dupli = data_.dupli_parent && data_.dupli_object_current;
    if (is_preview_dupli) {
      /* Don't create duplis from temporary preview objects, object_duplilist_preview already takes
       * care of everything. (See #146194, #146211) */
      continue;
    }

    bool instances_visible = (visibility & OB_VISIBLE_INSTANCES) &&
                             ((ob->transflag & OB_DUPLI) ||
                              ob->runtime->geometry_set_eval != nullptr);

    if (!instances_visible) {
      continue;
    }

    duplilist.clear();
    object_duplilist(
        draw_ctx.depsgraph, draw_ctx.scene, ob, deg_iter_settings.included_objects, duplilist);

    if (duplilist.is_empty()) {
      continue;
    }

    dupli_map.clear();
    for (DupliObject &dupli : duplilist) {

      if (!DEG_iterator_dupli_is_visible(&dupli, eval_mode)) {
        continue;
      }

      /* TODO: Optimize.
       * We can't check the dupli.ob since visibility may be different than the dupli itself.
       * But we should be able to check the dupli visibility without creating a temp object. */
#if 0
      if (!should_draw_object_cb(*dupli.ob)) {
        continue;
      }
#endif

      if (!engines_support_handle_ranges || !supports_handle_ranges(&dupli, ob)) {
        /* Sync the dupli as a single object. */
        if (!evil::DEG_iterator_temp_object_from_dupli(
                ob, &dupli, eval_mode, false, &tmp_object, &tmp_runtime) ||
            !should_draw_object_cb(tmp_object))
        {
          evil::DEG_iterator_temp_object_free_properties(&dupli, &tmp_object);
          continue;
        }

        tmp_object.light_linking = ob->light_linking;
        SET_FLAG_FROM_TEST(tmp_object.transflag, is_negative_m4(dupli.mat), OB_NEG_SCALE);
        tmp_object.runtime->object_to_world = float4x4(dupli.mat);
        tmp_object.runtime->world_to_object = invert(tmp_object.runtime->object_to_world);

        blender::draw::ObjectRef ob_ref(&tmp_object, ob, &dupli);
        draw_object_cb(ob_ref);

        evil::DEG_iterator_temp_object_free_properties(&dupli, &tmp_object);
        continue;
      }

      InstancesFlags flags = InstancesFlags(0);
      {
        SET_FLAG_FROM_TEST(flags, is_negative_m4(dupli.mat), InstancesFlags::IsNegativeScale);
      }
      InstancesKey key(dupli.ob,
                       dupli.ob_data,
                       flags,
                       dupli.preview_base_geometry,
                       dupli.preview_instance_index);

      dupli_map.lookup_or_add_default(key).append(&dupli);
    }

    for (const auto &[key, instances] : dupli_map.items()) {
      DupliObject *first_dupli = instances.first();
      if (!evil::DEG_iterator_temp_object_from_dupli(
              ob, first_dupli, eval_mode, false, &tmp_object, &tmp_runtime) ||
          !should_draw_object_cb(tmp_object))
      {
        evil::DEG_iterator_temp_object_free_properties(first_dupli, &tmp_object);
        continue;
      }

      tmp_object.light_linking = ob->light_linking;
      SET_FLAG_FROM_TEST(tmp_object.transflag,
                         flag_is_set(key.flags, InstancesFlags::IsNegativeScale),
                         OB_NEG_SCALE);
      /* Should use DrawInstances data instead. */
      tmp_object.runtime->object_to_world = float4x4();
      tmp_object.runtime->world_to_object = float4x4();

      blender::draw::ObjectRef ob_ref(tmp_object, ob, instances);
      draw_object_cb(ob_ref);

      evil::DEG_iterator_temp_object_free_properties(first_dupli, &tmp_object);
    }
  }
  DEG_OBJECT_ITER_END;
}

}  // namespace blender::draw

/** \} */

/* -------------------------------------------------------------------- */
/** \name Garbage Collection
 * \{ */

void DRW_cache_free_old_batches(Main *bmain)
{
  using namespace blender::draw;
  Scene *scene;
  static int lasttime = 0;
  int ctime = int(BLI_time_now_seconds());

  if (U.vbotimeout == 0 || (ctime - lasttime) < U.vbocollectrate || ctime == lasttime) {
    return;
  }

  lasttime = ctime;

  for (scene = static_cast<Scene *>(bmain->scenes.first); scene;
       scene = static_cast<Scene *>(scene->id.next))
  {
    LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
      Depsgraph *depsgraph = BKE_scene_get_depsgraph(scene, view_layer);
      if (depsgraph == nullptr) {
        continue;
      }

      /* TODO(fclem): This is not optimal since it iter over all dupli instances.
       * In this case only the source object should be tagged. */
      DEGObjectIterSettings deg_iter_settings = {nullptr};
      deg_iter_settings.depsgraph = depsgraph;
      deg_iter_settings.flags = DEG_OBJECT_ITER_FOR_RENDER_ENGINE_FLAGS;
      DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, ob) {
        DRW_batch_cache_free_old(ob, ctime);
      }
      DEG_OBJECT_ITER_END;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rendering (DRW_engines)
 * \{ */

static void drw_engines_cache_populate(blender::draw::ObjectRef &ref,
                                       DupliCacheManager &dupli_cache,
                                       ExtractionGraph &extraction)
{
  if (ref.is_dupli() == false) {
    blender::draw::drw_batch_cache_validate(ref.object);
  }
  else {
    dupli_cache.try_add(ref);
  }

  DRWContext &ctx = drw_get();
  ctx.view_data_active->foreach_enabled_engine(
      [&](DrawEngine &instance) { instance.object_sync(ref, *DRW_manager_get()); });

  /* TODO: in the future it would be nice to generate once for all viewports.
   * But we need threaded DRW manager first. */
  if (ref.is_dupli() == false) {
    blender::draw::drw_batch_cache_generate_requested(ref.object, *extraction.graph);
  }
  /* Batch generation for duplis happens after iter_callback. */
}

void DRWContext::sync(iter_callback_t iter_callback)
{
  /* Enable modules and init for next sync. */
  data->modules_begin_sync();

  DupliCacheManager dupli_handler;
  ExtractionGraph extraction;

  /* Custom callback defines the set of object to sync. */
  iter_callback(dupli_handler, extraction);

  dupli_handler.extract_all(extraction);
  extraction.work_and_wait(this->delayed_extraction);

  DRW_curves_update(*view_data_active->manager);
}

void DRWContext::engines_init_and_sync(iter_callback_t iter_callback)
{
  view_data_active->foreach_enabled_engine([&](DrawEngine &instance) { instance.init(); });

  view_data_active->manager->begin_sync(this->obact);

  view_data_active->foreach_enabled_engine([&](DrawEngine &instance) { instance.begin_sync(); });

  sync(iter_callback);

  view_data_active->foreach_enabled_engine([&](DrawEngine &instance) { instance.end_sync(); });

  view_data_active->manager->end_sync();
}

void DRWContext::engines_draw_scene()
{
  /* Start Drawing */
  blender::draw::command::StateSet::set();

  view_data_active->foreach_enabled_engine([&](DrawEngine &instance) {
#ifdef __APPLE__
    if (G.debug & G_DEBUG_GPU) {
      /* Put each engine inside their own command buffers. */
      GPU_flush();
    }
#endif
    GPU_debug_group_begin(instance.name_get().c_str());
    instance.draw(*DRW_manager_get());
    GPU_debug_group_end();
  });

  /* Reset state after drawing */
  blender::draw::command::StateSet::set();

  /* Fix 3D view "lagging" on APPLE and WIN32+NVIDIA. (See #56996, #61474) */
  if (GPU_type_matches_ex(GPU_DEVICE_ANY, GPU_OS_ANY, GPU_DRIVER_ANY, GPU_BACKEND_OPENGL)) {
    GPU_flush();
  }
}

void DRW_draw_region_engine_info(int xoffset, int *yoffset, int line_height)
{
  DRWContext &ctx = drw_get();
  ctx.view_data_active->foreach_enabled_engine([&](DrawEngine &instance) {
    if (instance.info[0] != '\0') {
      const char *buf_step = IFACE_(instance.info);
      do {
        const char *buf = buf_step;
        buf_step = BLI_strchr_or_end(buf, '\n');
        const int buf_len = buf_step - buf;
        *yoffset -= line_height;
        BLF_draw_default(xoffset, *yoffset, 0.0f, buf, buf_len);
      } while (*buf_step ? ((void)buf_step++, true) : false);
    }
  });
}

void DRWContext::enable_engines(bool gpencil_engine_needed, RenderEngineType *render_engine_type)
{
  DRWViewData &view_data = *this->view_data_active;

  SpaceLink *space_data = this->space_data;
  if (space_data && space_data->spacetype == SPACE_IMAGE) {
    if (DRW_engine_external_acquire_for_image_editor(this)) {
      view_data.external.set_used(true);
    }
    else {
      view_data.image.set_used(true);
    }
    view_data.overlay.set_used(true);
    return;
  }

  if (space_data && space_data->spacetype == SPACE_NODE) {
    /* Only enable when drawing the space image backdrop. */
    SpaceNode *snode = (SpaceNode *)space_data;
    if ((snode->flag & SNODE_BACKDRAW) != 0) {
      view_data.image.set_used(true);
      view_data.overlay.set_used(true);
    }
    return;
  }

  if (ELEM(this->mode, DRWContext::SELECT_OBJECT, DRWContext::SELECT_OBJECT_MATERIAL)) {
    view_data.grease_pencil.set_used(gpencil_engine_needed);
    view_data.object_select.set_used(true);
    return;
  }

  if (ELEM(this->mode, DRWContext::SELECT_EDIT_MESH)) {
    view_data.edit_select.set_used(true);
    return;
  }

  if (ELEM(this->mode, DRWContext::DEPTH, DRWContext::DEPTH_ACTIVE_OBJECT)) {
    view_data.grease_pencil.set_used(gpencil_engine_needed);
    view_data.overlay.set_used(true);
    return;
  }

  /* Regular V3D drawing. */
  {
    const eDrawType drawtype = eDrawType(this->v3d->shading.type);
    const bool use_xray = XRAY_ENABLED(this->v3d);

    /* Base engine. */
    switch (drawtype) {
      case OB_WIRE:
      case OB_SOLID:
        view_data.workbench.set_used(true);
        break;
      case OB_MATERIAL:
      case OB_RENDER:
      default:
        if (render_engine_type == &DRW_engine_viewport_eevee_type) {
          view_data.eevee.set_used(true);
        }
        else if (render_engine_type == &DRW_engine_viewport_workbench_type) {
          view_data.workbench.set_used(true);
        }
        else if ((render_engine_type->flag & RE_INTERNAL) == 0) {
          view_data.external.set_used(true);
        }
        else {
          BLI_assert_unreachable();
        }
        break;
    }

    if ((drawtype >= OB_SOLID) || !use_xray) {
      view_data.grease_pencil.set_used(gpencil_engine_needed);
    }

    view_data.compositor.set_used(is_viewport_compositor_enabled());

    view_data.overlay.set_used(true);

#ifdef WITH_DRAW_DEBUG
    if (G.debug_value == 31) {
      view_data.edit_select_debug.set_used(true);
    }
#endif
  }
}

void DRWContext::engines_data_validate()
{
  DRW_view_data_free_unused(this->view_data_active);
}

static bool gpencil_object_is_excluded(View3D *v3d)
{
  if (v3d) {
    return ((v3d->object_type_exclude_viewport & (1 << OB_GREASE_PENCIL)) != 0);
  }
  return false;
}

static bool gpencil_any_exists(Depsgraph *depsgraph)
{
  return (DEG_id_type_any_exists(depsgraph, ID_GD_LEGACY) ||
          DEG_id_type_any_exists(depsgraph, ID_GP));
}

bool DRW_gpencil_engine_needed_viewport(Depsgraph *depsgraph, View3D *v3d)
{
  if (gpencil_object_is_excluded(v3d)) {
    return false;
  }
  return gpencil_any_exists(depsgraph);
}

/* -------------------------------------------------------------------- */
/** \name Callbacks
 * \{ */

static void drw_callbacks_pre_scene(DRWContext &draw_ctx)
{
  RegionView3D *rv3d = draw_ctx.rv3d;

  GPU_matrix_projection_set(rv3d->winmat);
  GPU_matrix_set(rv3d->viewmat);

  if (draw_ctx.evil_C) {
    blender::draw::command::StateSet::set();
    DRW_submission_start();
    ED_region_draw_cb_draw(draw_ctx.evil_C, draw_ctx.region, REGION_DRAW_PRE_VIEW);
    DRW_submission_end();
  }

  /* State is reset later at the beginning of `draw_ctx.engines_draw_scene()`. */
}

static void drw_callbacks_post_scene(DRWContext &draw_ctx)
{
  RegionView3D *rv3d = draw_ctx.rv3d;
  ARegion *region = draw_ctx.region;
  View3D *v3d = draw_ctx.v3d;
  Depsgraph *depsgraph = draw_ctx.depsgraph;

  const bool do_annotations = draw_show_annotation();

  /* State has been reset at the end `draw_ctx.engines_draw_scene()`. */

  DRW_submission_start();
  if (draw_ctx.evil_C) {
    DefaultFramebufferList *dfbl = DRW_context_get()->viewport_framebuffer_list_get();

    GPU_framebuffer_bind(dfbl->overlay_fb);

    GPU_matrix_projection_set(rv3d->winmat);
    GPU_matrix_set(rv3d->viewmat);

    /* annotations - temporary drawing buffer (3d space) */
    /* XXX: Or should we use a proper draw/overlay engine for this case? */
    if (do_annotations) {
      GPU_depth_test(GPU_DEPTH_NONE);
      /* XXX: as `scene->gpd` is not copied for copy-on-eval yet. */
      ED_annotation_draw_view3d(DEG_get_input_scene(depsgraph), depsgraph, v3d, region, true);
      GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
    }

    GPU_depth_test(GPU_DEPTH_NONE);
    /* Apply state for callbacks. */
    GPU_apply_state();

    ED_region_draw_cb_draw(draw_ctx.evil_C, draw_ctx.region, REGION_DRAW_POST_VIEW);

#ifdef WITH_XR_OPENXR
    /* XR callbacks (controllers, custom draw functions) for session mirror. */
    if ((v3d->flag & V3D_XR_SESSION_MIRROR) != 0) {
      if ((v3d->flag2 & V3D_XR_SHOW_CONTROLLERS) != 0) {
        ARegionType *art = WM_xr_surface_controller_region_type_get();
        if (art) {
          ED_region_surface_draw_cb_draw(art, REGION_DRAW_POST_VIEW);
        }
      }
      if ((v3d->flag2 & V3D_XR_SHOW_CUSTOM_OVERLAYS) != 0) {
        SpaceType *st = BKE_spacetype_from_id(SPACE_VIEW3D);
        if (st) {
          ARegionType *art = BKE_regiontype_from_id(st, RGN_TYPE_XR);
          if (art) {
            ED_region_surface_draw_cb_draw(art, REGION_DRAW_POST_VIEW);
          }
        }
      }
    }
#endif

    /* Callback can be nasty and do whatever they want with the state.
     * Don't trust them! */
    blender::draw::command::StateSet::set();

    /* Needed so gizmo isn't occluded. */
    if ((v3d->gizmo_flag & V3D_GIZMO_HIDE) == 0) {
      GPU_depth_test(GPU_DEPTH_NONE);
      DRW_draw_gizmo_3d(draw_ctx.evil_C, region);
    }

    GPU_depth_test(GPU_DEPTH_NONE);
    DRW_draw_region_info(draw_ctx.evil_C, region);

    /* Annotations - temporary drawing buffer (screen-space). */
    /* XXX: Or should we use a proper draw/overlay engine for this case? */
    if (((v3d->flag2 & V3D_HIDE_OVERLAYS) == 0) && (do_annotations)) {
      GPU_depth_test(GPU_DEPTH_NONE);
      /* XXX: as `scene->gpd` is not copied for copy-on-eval yet */
      ED_annotation_draw_view3d(DEG_get_input_scene(depsgraph), depsgraph, v3d, region, false);
    }

    if ((v3d->gizmo_flag & V3D_GIZMO_HIDE) == 0) {
      /* Draw 2D after region info so we can draw on top of the camera passepartout overlay.
       * 'DRW_draw_region_info' sets the projection in pixel-space. */
      GPU_depth_test(GPU_DEPTH_NONE);
      DRW_draw_gizmo_2d(draw_ctx.evil_C, region);
    }

    GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
  }
  else {
    if (v3d && ((v3d->flag2 & V3D_SHOW_ANNOTATION) != 0)) {
      GPU_depth_test(GPU_DEPTH_NONE);
      /* XXX: as `scene->gpd` is not copied for copy-on-eval yet */
      ED_annotation_draw_view3d(DEG_get_input_scene(depsgraph), depsgraph, v3d, region, true);
      GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
    }

#ifdef WITH_XR_OPENXR
    if ((v3d->flag & V3D_XR_SESSION_SURFACE) != 0) {
      DefaultFramebufferList *dfbl = DRW_context_get()->viewport_framebuffer_list_get();

      blender::draw::command::StateSet::set();

      GPU_framebuffer_bind(dfbl->overlay_fb);

      GPU_matrix_projection_set(rv3d->winmat);
      GPU_matrix_set(rv3d->viewmat);

      /* XR callbacks (controllers, custom draw functions) for session surface. */
      if (((v3d->flag2 & V3D_XR_SHOW_CONTROLLERS) != 0) ||
          ((v3d->flag2 & V3D_XR_SHOW_CUSTOM_OVERLAYS) != 0))
      {
        GPU_depth_test(GPU_DEPTH_NONE);
        GPU_apply_state();

        if ((v3d->flag2 & V3D_XR_SHOW_CONTROLLERS) != 0) {
          ARegionType *art = WM_xr_surface_controller_region_type_get();
          if (art) {
            ED_region_surface_draw_cb_draw(art, REGION_DRAW_POST_VIEW);
          }
        }
        if ((v3d->flag2 & V3D_XR_SHOW_CUSTOM_OVERLAYS) != 0) {
          SpaceType *st = BKE_spacetype_from_id(SPACE_VIEW3D);
          if (st) {
            ARegionType *art = BKE_regiontype_from_id(st, RGN_TYPE_XR);
            if (art) {
              ED_region_surface_draw_cb_draw(art, REGION_DRAW_POST_VIEW);
            }
          }
        }

        blender::draw::command::StateSet::set();
      }

      GPU_depth_test(GPU_DEPTH_LESS_EQUAL);
    }
#endif
  }
  DRW_submission_end();

  blender::draw::command::StateSet::set();
}

static void drw_callbacks_pre_scene_2D(DRWContext &draw_ctx)
{
  if (draw_ctx.evil_C) {
    blender::draw::command::StateSet::set();
    DRW_submission_start();
    ED_region_draw_cb_draw(draw_ctx.evil_C, draw_ctx.region, REGION_DRAW_PRE_VIEW);
    DRW_submission_end();
  }

  /* State is reset later at the beginning of `draw_ctx.engines_draw_scene()`. */
}

static void drw_callbacks_post_scene_2D(DRWContext &draw_ctx, View2D &v2d)
{
  const bool do_annotations = draw_show_annotation();
  const bool do_draw_gizmos = (draw_ctx.space_data->spacetype != SPACE_IMAGE);

  /* State has been reset at the end `draw_ctx.engines_draw_scene()`. */

  DRW_submission_start();
  if (draw_ctx.evil_C) {
    DefaultFramebufferList *dfbl = DRW_context_get()->viewport_framebuffer_list_get();

    GPU_framebuffer_bind(dfbl->overlay_fb);

    GPU_depth_test(GPU_DEPTH_NONE);
    GPU_matrix_push_projection();

    wmOrtho2(v2d.cur.xmin, v2d.cur.xmax, v2d.cur.ymin, v2d.cur.ymax);

    if (do_annotations) {
      ED_annotation_draw_view2d(draw_ctx.evil_C, true);
    }

    GPU_depth_test(GPU_DEPTH_NONE);

    ED_region_draw_cb_draw(draw_ctx.evil_C, draw_ctx.region, REGION_DRAW_POST_VIEW);

    GPU_matrix_pop_projection();
    /* Callback can be nasty and do whatever they want with the state.
     * Don't trust them! */
    blender::draw::command::StateSet::set();

    GPU_depth_test(GPU_DEPTH_NONE);

    if (do_annotations) {
      ED_annotation_draw_view2d(draw_ctx.evil_C, false);
    }
  }

  ED_region_pixelspace(draw_ctx.region);

  if (do_draw_gizmos) {
    GPU_depth_test(GPU_DEPTH_NONE);
    DRW_draw_gizmo_2d(draw_ctx.evil_C, draw_ctx.region);
  }

  DRW_submission_end();

  blender::draw::command::StateSet::set();
}

DRWTextStore *DRW_text_cache_ensure()
{
  DRWContext &draw_ctx = drw_get();
  BLI_assert(draw_ctx.text_store_p);
  if (*draw_ctx.text_store_p == nullptr) {
    *draw_ctx.text_store_p = DRW_text_cache_create();
  }
  return *draw_ctx.text_store_p;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Draw Loops (DRW_draw)
 * \{ */

/**
 * Used for both regular and off-screen drawing.
 * The global `DRWContext` needs to be set before calling this function.
 */
static void drw_draw_render_loop_3d(DRWContext &draw_ctx, RenderEngineType *engine_type)
{
  using namespace blender::draw;
  Depsgraph *depsgraph = draw_ctx.depsgraph;
  View3D *v3d = draw_ctx.v3d;

  /* Check if scene needs to perform the populate loop */
  const bool internal_engine = (engine_type->flag & RE_INTERNAL) != 0;
  const bool draw_type_render = v3d->shading.type == OB_RENDER;
  const bool overlays_on = (v3d->flag2 & V3D_HIDE_OVERLAYS) == 0;
  const bool gpencil_engine_needed = DRW_gpencil_engine_needed_viewport(depsgraph, v3d);
  const bool do_populate_loop = internal_engine || overlays_on || !draw_type_render ||
                                gpencil_engine_needed;

  auto should_draw_object = [&](Object &ob) -> bool {
    return BKE_object_is_visible_in_viewport(v3d, &ob);
  };

  draw_ctx.enable_engines(gpencil_engine_needed, engine_type);
  draw_ctx.engines_data_validate();
  draw_ctx.engines_init_and_sync([&](DupliCacheManager &duplis, ExtractionGraph &extraction) {
    /* Only iterate over objects for internal engines or when overlays are enabled */
    if (do_populate_loop) {
      foreach_obref_in_scene(draw_ctx, should_draw_object, [&](ObjectRef &ob_ref) {
        drw_engines_cache_populate(ob_ref, duplis, extraction);
      });
    }
  });

  /* No frame-buffer allowed before drawing. */
  BLI_assert(GPU_framebuffer_active_get() == GPU_framebuffer_back_get());
  GPU_framebuffer_bind(draw_ctx.default_framebuffer());
  GPU_framebuffer_clear_depth_stencil(draw_ctx.default_framebuffer(), 1.0f, 0xFF);

  drw_callbacks_pre_scene(draw_ctx);
  draw_ctx.engines_draw_scene();
  drw_callbacks_post_scene(draw_ctx);

  if (WM_draw_region_get_bound_viewport(draw_ctx.region)) {
    /* Don't unbind the frame-buffer yet in this case and let
     * GPU_viewport_unbind do it, so that we can still do further
     * drawing of action zones on top. */
  }
  else {
    GPU_framebuffer_restore();
  }
}

static void drw_draw_render_loop_2d(DRWContext &draw_ctx)
{
  Depsgraph *depsgraph = draw_ctx.depsgraph;
  ARegion *region = draw_ctx.region;

  /* TODO(jbakker): Only populate when editor needs to draw object.
   * for the image editor this is when showing UVs. */
  const bool do_populate_loop = (draw_ctx.space_data->spacetype == SPACE_IMAGE);

  draw_ctx.enable_engines();
  draw_ctx.engines_data_validate();
  draw_ctx.engines_init_and_sync([&](DupliCacheManager &duplis, ExtractionGraph &extraction) {
    /* Only iterate over objects when overlay uses object data. */
    if (do_populate_loop) {
      DEGObjectIterSettings deg_iter_settings = {nullptr};
      deg_iter_settings.depsgraph = depsgraph;
      deg_iter_settings.flags = DEG_OBJECT_ITER_FOR_RENDER_ENGINE_FLAGS;
      DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, ob) {
        blender::draw::ObjectRef ob_ref(ob);
        drw_engines_cache_populate(ob_ref, duplis, extraction);
      }
      DEG_OBJECT_ITER_END;
    }
  });

  /* No frame-buffer allowed before drawing. */
  BLI_assert(GPU_framebuffer_active_get() == GPU_framebuffer_back_get());
  GPU_framebuffer_bind(draw_ctx.default_framebuffer());
  GPU_framebuffer_clear_depth_stencil(draw_ctx.default_framebuffer(), 1.0f, 0xFF);

  drw_callbacks_pre_scene_2D(draw_ctx);
  draw_ctx.engines_draw_scene();
  drw_callbacks_post_scene_2D(draw_ctx, region->v2d);

  if (WM_draw_region_get_bound_viewport(region)) {
    /* Don't unbind the frame-buffer yet in this case and let
     * GPU_viewport_unbind do it, so that we can still do further
     * drawing of action zones on top. */
  }
  else {
    GPU_framebuffer_restore();
  }
}

void DRW_draw_view(const bContext *C)
{
  Depsgraph *depsgraph = CTX_data_expect_evaluated_depsgraph(C);
  ARegion *region = CTX_wm_region(C);
  GPUViewport *viewport = WM_draw_region_get_bound_viewport(region);

  DRWContext draw_ctx(DRWContext::VIEWPORT, depsgraph, viewport, C);
  draw_ctx.acquire_data();

  if (draw_ctx.v3d) {
    Scene *scene = DEG_get_evaluated_scene(depsgraph);
    RenderEngineType *engine_type = ED_view3d_engine_type(scene, draw_ctx.v3d->shading.type);

    draw_ctx.options.draw_background = (scene->r.alphamode == R_ADDSKY) ||
                                       (draw_ctx.v3d->shading.type != OB_RENDER);

    drw_draw_render_loop_3d(draw_ctx, engine_type);
  }
  else {
    drw_draw_render_loop_2d(draw_ctx);
  }

  draw_ctx.release_data();
}

void DRW_draw_render_loop_offscreen(Depsgraph *depsgraph,
                                    RenderEngineType *engine_type,
                                    ARegion *region,
                                    View3D *v3d,
                                    const bool is_image_render,
                                    const bool draw_background,
                                    const bool do_color_management,
                                    GPUOffScreen *ofs,
                                    GPUViewport *viewport)
{
  const bool is_xr_surface = ((v3d->flag & V3D_XR_SESSION_SURFACE) != 0);

  /* Create temporary viewport if needed or update the existing viewport. */
  GPUViewport *render_viewport = viewport;
  if (viewport == nullptr) {
    render_viewport = GPU_viewport_create();
  }

  GPU_viewport_bind_from_offscreen(render_viewport, ofs, is_xr_surface);

  /* Just here to avoid an assert but shouldn't be required in practice. */
  GPU_framebuffer_restore();

  /* TODO(fclem): We might want to differentiate between render preview and offscreen render in the
   * future. The later can do progressive rendering. */
  BLI_assert(is_xr_surface == !is_image_render);
  UNUSED_VARS_NDEBUG(is_image_render);
  DRWContext::Mode mode = is_xr_surface ? DRWContext::VIEWPORT_XR : DRWContext::VIEWPORT_RENDER;

  DRWContext draw_ctx(mode, depsgraph, render_viewport, nullptr, region, v3d);
  draw_ctx.acquire_data();
  draw_ctx.options.draw_background = draw_background;

  drw_draw_render_loop_3d(draw_ctx, engine_type);

  draw_ctx.release_data();

  if (draw_background) {
    /* HACK(@fclem): In this case we need to make sure the final alpha is 1.
     * We use the blend mode to ensure that. A better way to fix that would
     * be to do that in the color-management shader. */
    GPU_offscreen_bind(ofs, false);
    GPU_clear_color(0.0f, 0.0f, 0.0f, 1.0f);
    /* Pre-multiply alpha over black background. */
    GPU_blend(GPU_BLEND_ALPHA_PREMULT);
  }

  GPU_matrix_identity_set();
  GPU_matrix_identity_projection_set();
  const bool do_overlays = (v3d->flag2 & V3D_HIDE_OVERLAYS) == 0 ||
                           ELEM(v3d->shading.type, OB_WIRE, OB_SOLID) ||
                           (ELEM(v3d->shading.type, OB_MATERIAL) &&
                            (v3d->shading.flag & V3D_SHADING_SCENE_WORLD) == 0) ||
                           (ELEM(v3d->shading.type, OB_RENDER) &&
                            (v3d->shading.flag & V3D_SHADING_SCENE_WORLD_RENDER) == 0);
  GPU_viewport_unbind_from_offscreen(render_viewport, ofs, do_color_management, do_overlays);

  if (draw_background) {
    /* Reset default. */
    GPU_blend(GPU_BLEND_NONE);
  }

  /* Free temporary viewport. */
  if (viewport == nullptr) {
    GPU_viewport_free(render_viewport);
  }
}

bool DRW_render_check_grease_pencil(Depsgraph *depsgraph)
{
  if (gpencil_any_exists(depsgraph)) {
    return true;
  }

  DEGObjectIterSettings deg_iter_settings = {nullptr};
  deg_iter_settings.depsgraph = depsgraph;
  deg_iter_settings.flags = DEG_OBJECT_ITER_FOR_RENDER_ENGINE_FLAGS;
  DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, ob) {
    if (ob->type == OB_GREASE_PENCIL) {
      if (BKE_object_visibility(ob, DAG_EVAL_RENDER) & OB_VISIBLE_SELF) {
        return true;
      }
    }
  }
  DEG_OBJECT_ITER_END;

  return false;
}

void DRW_render_gpencil(RenderEngine *engine, Depsgraph *depsgraph)
{
  using namespace blender::draw;
  /* This function should only be called if there are grease pencil objects,
   * especially important to avoid failing in background renders without GPU context. */
  BLI_assert(DRW_render_check_grease_pencil(depsgraph));

  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);
  RenderResult *render_result = RE_engine_get_result(engine);
  RenderLayer *render_layer = RE_GetRenderLayer(render_result, view_layer->name);
  if (render_layer == nullptr) {
    return;
  }

  Render *render = engine->re;

  DRW_render_context_enable(render);

  DRWContext draw_ctx(DRWContext::RENDER, depsgraph, {engine->resolution_x, engine->resolution_y});
  draw_ctx.acquire_data();
  draw_ctx.options.draw_background = scene->r.alphamode == R_ADDSKY;

  /* Main rendering. */
  rctf view_rect;
  rcti render_rect;
  RE_GetViewPlane(render, &view_rect, &render_rect);
  if (BLI_rcti_is_empty(&render_rect)) {
    BLI_rcti_init(&render_rect, 0, draw_ctx.size[0], 0, draw_ctx.size[1]);
  }

  for (RenderView *render_view = static_cast<RenderView *>(render_result->views.first);
       render_view != nullptr;
       render_view = render_view->next)
  {
    RE_SetActiveRenderView(render, render_view->name);
    gpencil::Engine::render_to_image(engine, render_layer, render_rect);
  }

  command::StateSet::set();

  GPU_depth_test(GPU_DEPTH_NONE);

  blender::gpu::TexturePool::get().reset(true);

  draw_ctx.release_data();

  /* Restore Drawing area. */
  GPU_framebuffer_restore();

  DRW_render_context_disable(render);
}

void DRW_render_to_image(
    RenderEngine *engine,
    Depsgraph *depsgraph,
    std::function<void(RenderEngine *, RenderLayer *, const rcti)> render_view_cb,
    std::function<void(RenderResult *)> store_metadata_cb)
{
  using namespace blender::draw;
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);
  Render *render = engine->re;

  /* IMPORTANT: We don't support immediate mode in render mode!
   * This shall remain in effect until immediate mode supports
   * multiple threads. */

  /* Begin GPU workload Boundary */
  GPU_render_begin();

  DRWContext draw_ctx(DRWContext::RENDER, depsgraph, {engine->resolution_x, engine->resolution_y});
  draw_ctx.acquire_data();
  draw_ctx.options.draw_background = scene->r.alphamode == R_ADDSKY;

  /* Main rendering. */
  rctf view_rect;
  rcti render_rect;
  RE_GetViewPlane(render, &view_rect, &render_rect);
  if (BLI_rcti_is_empty(&render_rect)) {
    BLI_rcti_init(&render_rect, 0, draw_ctx.size[0], 0, draw_ctx.size[1]);
  }

  /* Reset state before drawing */
  command::StateSet::set();

  /* set default viewport */
  GPU_viewport(0, 0, draw_ctx.size[0], draw_ctx.size[1]);

  /* Init render result. */
  RenderResult *render_result = RE_engine_begin_result(engine,
                                                       0,
                                                       0,
                                                       draw_ctx.size[0],
                                                       draw_ctx.size[1],
                                                       view_layer->name,
                                                       /*RR_ALL_VIEWS*/ nullptr);
  RenderLayer *render_layer = static_cast<RenderLayer *>(render_result->layers.first);
  for (RenderView *render_view = static_cast<RenderView *>(render_result->views.first);
       render_view != nullptr;
       render_view = render_view->next)
  {
    RE_SetActiveRenderView(render, render_view->name);
    render_view_cb(engine, render_layer, render_rect);
  }

  RE_engine_end_result(engine, render_result, false, false, false);

  store_metadata_cb(RE_engine_get_result(engine));

  GPU_framebuffer_restore();

  blender::gpu::TexturePool::get().reset(true);

  draw_ctx.release_data();
  DRW_cache_free_old_subdiv();

  /* End GPU workload Boundary */
  GPU_render_end();
}

void DRW_render_object_iter(
    RenderEngine *engine,
    Depsgraph *depsgraph,
    std::function<void(blender::draw::ObjectRef &, RenderEngine *, Depsgraph *)> callback)
{
  using namespace blender::draw;

  DRWContext &draw_ctx = drw_get();
  View3D *v3d = draw_ctx.v3d;

  auto should_draw_object = [&](Object &ob) -> bool {
    if (v3d) {
      return BKE_object_is_visible_in_viewport(v3d, &ob);
    }
    return true;
  };

  draw_ctx.sync([&](DupliCacheManager &duplis, ExtractionGraph &extraction) {
    foreach_obref_in_scene(draw_ctx, should_draw_object, [&](ObjectRef &ob_ref) {
      if (ob_ref.is_dupli() == false) {
        blender::draw::drw_batch_cache_validate(ob_ref.object);
      }
      else {
        duplis.try_add(ob_ref);
      }
      callback(ob_ref, engine, depsgraph);
      if (ob_ref.is_dupli() == false) {
        blender::draw::drw_batch_cache_generate_requested(ob_ref.object, *extraction.graph);
      }
      /* Batch generation for duplis happens after iter_callback. */
    });
  });
}

void DRW_custom_pipeline_begin(DRWContext &draw_ctx, Depsgraph * /*depsgraph*/)
{
  draw_ctx.acquire_data();
  draw_ctx.data->modules_begin_sync();
}

void DRW_custom_pipeline_end(DRWContext &draw_ctx)
{
  GPU_framebuffer_restore();

  /* The use of custom pipeline in other thread using the same
   * resources as the main thread (viewport) may lead to data
   * races and undefined behavior on certain drivers. Using
   * GPU_finish to sync seems to fix the issue. (see #62997) */
  GPUBackendType type = GPU_backend_get_type();
  if (type == GPU_BACKEND_OPENGL) {
    GPU_finish();
  }

  blender::gpu::TexturePool::get().reset(true);
  draw_ctx.release_data();
}

void DRW_cache_restart()
{
  using namespace blender::draw;
  DRWContext &draw_ctx = drw_get();
  draw_ctx.data->modules_exit();
  draw_ctx.acquire_data();
  draw_ctx.data->modules_begin_sync();
}

void DRW_render_set_time(RenderEngine *engine, Depsgraph *depsgraph, int frame, float subframe)
{
  DRWContext &draw_ctx = drw_get();
  RE_engine_frame_set(engine, frame, subframe);
  draw_ctx.scene = DEG_get_evaluated_scene(depsgraph);
  draw_ctx.view_layer = DEG_get_evaluated_view_layer(depsgraph);
}

static struct DRWSelectBuffer {
  blender::gpu::FrameBuffer *framebuffer_depth_only;
  blender::gpu::Texture *texture_depth;
} g_select_buffer = {nullptr};

static void draw_select_framebuffer_depth_only_setup(const int size[2])
{
  if (g_select_buffer.framebuffer_depth_only == nullptr) {
    g_select_buffer.framebuffer_depth_only = GPU_framebuffer_create("framebuffer_depth_only");
  }

  if ((g_select_buffer.texture_depth != nullptr) &&
      ((GPU_texture_width(g_select_buffer.texture_depth) != size[0]) ||
       (GPU_texture_height(g_select_buffer.texture_depth) != size[1])))
  {
    GPU_texture_free(g_select_buffer.texture_depth);
    g_select_buffer.texture_depth = nullptr;
  }

  if (g_select_buffer.texture_depth == nullptr) {
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;
    g_select_buffer.texture_depth = GPU_texture_create_2d(
        "select_depth",
        size[0],
        size[1],
        1,
        blender::gpu::TextureFormat::SFLOAT_32_DEPTH,
        usage,
        nullptr);

    GPU_framebuffer_texture_attach(
        g_select_buffer.framebuffer_depth_only, g_select_buffer.texture_depth, 0, 0);

    GPU_framebuffer_check_valid(g_select_buffer.framebuffer_depth_only, nullptr);
  }
}

void DRW_draw_select_loop(Depsgraph *depsgraph,
                          ARegion *region,
                          View3D *v3d,
                          bool use_obedit_skip,
                          bool draw_surface,
                          bool /*use_nearest*/,
                          const bool do_material_sub_selection,
                          const rcti *rect,
                          DRW_SelectPassFn select_pass_fn,
                          void *select_pass_user_data,
                          DRW_ObjectFilterFn object_filter_fn,
                          void *object_filter_user_data)
{
  using namespace blender::draw;
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  ViewLayer *view_layer = DEG_get_evaluated_view_layer(depsgraph);
  const int viewport_size[2] = {BLI_rcti_size_x(rect), BLI_rcti_size_y(rect)};

  Object *obact = BKE_view_layer_active_object_get(view_layer);
  Object *obedit = use_obedit_skip ? nullptr : OBEDIT_FROM_OBACT(obact);

  bool use_obedit = false;
  const ToolSettings *ts = scene->toolsettings;

  /* obedit_ctx_mode is used for selecting the right draw engines */
  // eContextObjectMode obedit_ctx_mode;
  /* object_mode is used for filtering objects in the depsgraph */
  eObjectMode object_mode = eObjectMode::OB_MODE_EDIT;
  int object_type = 0;
  if (obedit != nullptr) {
    object_type = obedit->type;
    object_mode = eObjectMode(obedit->mode);
    if (obedit->type == OB_MBALL) {
      use_obedit = true;
      // obedit_ctx_mode = CTX_MODE_EDIT_METABALL;
    }
    else if (obedit->type == OB_ARMATURE) {
      use_obedit = true;
      // obedit_ctx_mode = CTX_MODE_EDIT_ARMATURE;
    }
  }

  if ((v3d->overlay.flag & V3D_OVERLAY_BONE_SELECT) &&
      /* Only restrict selection to bones when the user turns on "Lock Object Modes".
       * If the lock is off, skip this so other objects can still be selected.
       * See #66950 & #125822. */
      (ts->object_flag & SCE_OBJECT_MODE_LOCK))
  {
    if (!(v3d->flag2 & V3D_HIDE_OVERLAYS)) {
      /* NOTE: don't use "BKE_object_pose_armature_get" here, it breaks selection. */
      Object *obpose = OBPOSE_FROM_OBACT(obact);
      if (obpose == nullptr) {
        Object *obweight = OBWEIGHTPAINT_FROM_OBACT(obact);
        if (obweight) {
          /* Only use Armature pose selection, when connected armature is in pose mode. */
          Object *ob_armature = BKE_modifiers_is_deformed_by_armature(obweight);
          if (ob_armature && ob_armature->mode == OB_MODE_POSE) {
            obpose = ob_armature;
          }
        }
      }

      if (obpose) {
        use_obedit = true;
        object_type = obpose->type;
        object_mode = eObjectMode(obpose->mode);
        // obedit_ctx_mode = CTX_MODE_POSE;
      }
    }
  }

  bool use_gpencil = !use_obedit && !draw_surface &&
                     DRW_gpencil_engine_needed_viewport(depsgraph, v3d);

  DRWContext::Mode mode = do_material_sub_selection ? DRWContext::SELECT_OBJECT_MATERIAL :
                                                      DRWContext::SELECT_OBJECT;

  DRWContext draw_ctx(mode, depsgraph, viewport_size, nullptr, region, v3d);
  draw_ctx.acquire_data();
  draw_ctx.enable_engines(use_gpencil);
  draw_ctx.engines_data_validate();
  draw_ctx.engines_init_and_sync([&](DupliCacheManager &duplis, ExtractionGraph &extraction) {
    if (use_obedit) {
      FOREACH_OBJECT_IN_MODE_BEGIN (scene, view_layer, v3d, object_type, object_mode, ob_iter) {
        /* Depsgraph usually does this, but we use a different iterator.
         * So we have to do it manually. */
        ob_iter->runtime->select_id = DEG_get_original(ob_iter)->runtime->select_id;

        blender::draw::ObjectRef ob_ref(ob_iter);
        drw_engines_cache_populate(ob_ref, duplis, extraction);
      }
      FOREACH_OBJECT_IN_MODE_END;
    }
    else {
      /* When selecting pose-bones in pose mode, check for visibility not select-ability
       * as pose-bones have their own selection restriction flag. */
      const bool use_pose_exception = (draw_ctx.object_pose != nullptr);

      const int object_type_exclude_select = v3d->object_type_exclude_select;
      bool filter_exclude = false;

      auto should_draw_object = [&](Object &ob) {
        if (!BKE_object_is_visible_in_viewport(v3d, &ob)) {
          return false;
        }
        if (use_pose_exception && (ob.mode & OB_MODE_POSE)) {
          if ((ob.base_flag & BASE_ENABLED_AND_VISIBLE_IN_DEFAULT_VIEWPORT) == 0) {
            return false;
          }
        }
        else {
          if ((ob.base_flag & BASE_SELECTABLE) == 0) {
            return false;
          }
        }

        if ((object_type_exclude_select & (1 << ob.type)) == 0) {
          if (object_filter_fn != nullptr) {
            if (ob.base_flag & BASE_FROM_DUPLI) {
              /* pass (use previous filter_exclude value) */
            }
            else {
              filter_exclude = (object_filter_fn(&ob, object_filter_user_data) == false);
            }
            if (filter_exclude) {
              return false;
            }
          }
        }
        return true;
      };

      foreach_obref_in_scene(draw_ctx, should_draw_object, [&](ObjectRef &ob_ref) {
        drw_engines_cache_populate(ob_ref, duplis, extraction);
      });
    }
  });

  /* Setup frame-buffer. */
  draw_select_framebuffer_depth_only_setup(viewport_size);
  GPU_framebuffer_bind(g_select_buffer.framebuffer_depth_only);
  GPU_framebuffer_clear_depth(g_select_buffer.framebuffer_depth_only, 1.0f);

  /* WORKAROUND: Needed for Select-Next for keeping the same code-flow as Overlay-Next. */
  /* TODO(pragma37): Some engines retrieve the depth texture before this point (See #132922).
   * Check with @fclem. */
  BLI_assert(DRW_context_get()->viewport_texture_list_get()->depth == nullptr);
  DRW_context_get()->viewport_texture_list_get()->depth = g_select_buffer.texture_depth;

  drw_callbacks_pre_scene(draw_ctx);
  /* Only 1-2 passes. */
  while (true) {
    if (!select_pass_fn(DRW_SELECT_PASS_PRE, select_pass_user_data)) {
      break;
    }
    draw_ctx.engines_draw_scene();
    if (!select_pass_fn(DRW_SELECT_PASS_POST, select_pass_user_data)) {
      break;
    }
  }

  /* WORKAROUND: Do not leave ownership to the viewport list. */
  DRW_context_get()->viewport_texture_list_get()->depth = nullptr;

  draw_ctx.release_data();

  GPU_framebuffer_restore();
}

void DRW_draw_depth_loop(Depsgraph *depsgraph,
                         ARegion *region,
                         View3D *v3d,
                         GPUViewport *viewport,
                         const bool use_gpencil,
                         const bool use_only_selected,
                         const bool use_only_active_object)
{
  using namespace blender::draw;

  DRWContext draw_ctx(use_only_active_object ? DRWContext::DEPTH_ACTIVE_OBJECT : DRWContext::DEPTH,
                      depsgraph,
                      viewport,
                      nullptr,
                      region,
                      v3d);
  draw_ctx.acquire_data();
  draw_ctx.enable_engines(use_gpencil);
  draw_ctx.engines_init_and_sync([&](DupliCacheManager &duplis, ExtractionGraph &extraction) {
    auto should_draw_object = [&](Object &ob) {
      if (!BKE_object_is_visible_in_viewport(v3d, &ob)) {
        return false;
      }
      if (use_only_selected && !(ob.base_flag & BASE_SELECTED)) {
        return false;
      }
      return true;
    };

    if (use_only_active_object) {
      blender::draw::ObjectRef ob_ref(draw_ctx.obact);
      drw_engines_cache_populate(ob_ref, duplis, extraction);
    }
    else {
      foreach_obref_in_scene(draw_ctx, should_draw_object, [&](ObjectRef &ob_ref) {
        drw_engines_cache_populate(ob_ref, duplis, extraction);
      });
    }
  });

  /* Setup frame-buffer. */
  blender::gpu::Texture *depth_tx = GPU_viewport_depth_texture(viewport);
  blender::gpu::FrameBuffer *depth_fb = nullptr;
  GPU_framebuffer_ensure_config(&depth_fb,
                                {
                                    GPU_ATTACHMENT_TEXTURE(depth_tx),
                                    GPU_ATTACHMENT_NONE,
                                });
  GPU_framebuffer_bind(depth_fb);
  GPU_framebuffer_clear_depth(depth_fb, 1.0f);

  draw_ctx.engines_draw_scene();

  /* TODO: Reading depth for operators should be done here. */

  GPU_framebuffer_restore();
  GPU_framebuffer_free(depth_fb);

  draw_ctx.release_data();
}

void DRW_draw_select_id(Depsgraph *depsgraph, ARegion *region, View3D *v3d)
{
  SELECTID_Context *sel_ctx = DRW_select_engine_context_get();
  GPUViewport *viewport = WM_draw_region_get_viewport(region);
  if (!viewport) {
    /* Selection engine requires a viewport.
     * TODO(@germano): This should be done internally in the engine. */
    sel_ctx->max_index_drawn_len = 1;
    return;
  }

  using namespace blender::draw;

  /* Make sure select engine gets the correct vertex size. */
  UI_SetTheme(SPACE_VIEW3D, RGN_TYPE_WINDOW);

  DRWContext draw_ctx(DRWContext::SELECT_EDIT_MESH, depsgraph, viewport, nullptr, region, v3d);
  draw_ctx.acquire_data();
  draw_ctx.enable_engines();
  draw_ctx.engines_init_and_sync([&](DupliCacheManager &duplis, ExtractionGraph &extraction) {
    for (Object *obj_eval : sel_ctx->objects) {
      blender::draw::ObjectRef ob_ref(obj_eval);
      drw_engines_cache_populate(ob_ref, duplis, extraction);
    }

    if (RETOPOLOGY_ENABLED(v3d) && !XRAY_ENABLED(v3d)) {
      auto should_draw_object = [&](Object &ob) {
        if (ob.type != OB_MESH) {
          /* The iterator has evaluated meshes for all solid objects.
           * It also has non-mesh objects however, which are not supported here. */
          return false;
        }
        if (DRW_object_is_in_edit_mode(&ob)) {
          /* Only background (non-edit) objects are used for occlusion. */
          return false;
        }
        if (!BKE_object_is_visible_in_viewport(v3d, &ob)) {
          return false;
        }
        return true;
      };

      foreach_obref_in_scene(draw_ctx, should_draw_object, [&](ObjectRef &ob_ref) {
        drw_engines_cache_populate(ob_ref, duplis, extraction);
      });
    }
  });

  draw_ctx.engines_draw_scene();

  draw_ctx.release_data();
}

bool DRW_draw_in_progress()
{
  return DRWContext::is_active();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Manager State
 * \{ */

const DRWContext *DRW_context_get()
{
  return &drw_get();
}

bool DRWContext::is_playback() const
{
  if (this->evil_C != nullptr) {
    wmWindowManager *wm = CTX_wm_manager(this->evil_C);
    return ED_screen_animation_playing(wm) != nullptr;
  }
  return false;
}

bool DRWContext::is_navigating() const
{
  return (rv3d) && (rv3d->rflag & (RV3D_NAVIGATING | RV3D_PAINTING));
}

bool DRWContext::is_painting() const
{
  return (rv3d) && (rv3d->rflag & (RV3D_PAINTING));
}

bool DRWContext::is_transforming() const
{
  return (G.moving & (G_TRANSFORM_OBJ | G_TRANSFORM_EDIT)) != 0;
}

bool DRWContext::is_viewport_compositor_enabled() const
{
  if (!this->v3d) {
    return false;
  }

  if (this->v3d->shading.use_compositor == V3D_SHADING_USE_COMPOSITOR_DISABLED) {
    return false;
  }

  if (!(this->v3d->shading.type >= OB_MATERIAL)) {
    return false;
  }

  if (!this->scene->compositing_node_group) {
    return false;
  }

  if (!this->rv3d) {
    return false;
  }

  if (this->v3d->shading.use_compositor == V3D_SHADING_USE_COMPOSITOR_CAMERA &&
      this->rv3d->persp != RV3D_CAMOB)
  {
    return false;
  }

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name DRW_engines
 * \{ */

void DRW_engines_register()
{
  RE_engines_register(&DRW_engine_viewport_eevee_type);
  RE_engines_register(&DRW_engine_viewport_workbench_type);
}

void DRW_engines_free()
{
  blender::eevee::Engine::free_static();
  blender::workbench::Engine::free_static();
  blender::draw::gpencil::Engine::free_static();
  blender::image_engine::Engine::free_static();
  blender::draw::overlay::Engine::free_static();
  blender::draw::edit_select::Engine::free_static();
#ifdef WITH_DRAW_DEBUG
  blender::draw::edit_select_debug::Engine::free_static();
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name DRW_module
 * \{ */

void DRW_module_init()
{
  using namespace blender::draw;
  /* setup callbacks */
  BKE_curve_batch_cache_dirty_tag_cb = DRW_curve_batch_cache_dirty_tag;
  BKE_curve_batch_cache_free_cb = DRW_curve_batch_cache_free;

  BKE_mesh_batch_cache_dirty_tag_cb = DRW_mesh_batch_cache_dirty_tag;
  BKE_mesh_batch_cache_free_cb = DRW_mesh_batch_cache_free;

  BKE_lattice_batch_cache_dirty_tag_cb = DRW_lattice_batch_cache_dirty_tag;
  BKE_lattice_batch_cache_free_cb = DRW_lattice_batch_cache_free;

  BKE_particle_batch_cache_dirty_tag_cb = DRW_particle_batch_cache_dirty_tag;
  BKE_particle_batch_cache_free_cb = DRW_particle_batch_cache_free;

  BKE_curves_batch_cache_dirty_tag_cb = DRW_curves_batch_cache_dirty_tag;
  BKE_curves_batch_cache_free_cb = DRW_curves_batch_cache_free;

  BKE_pointcloud_batch_cache_dirty_tag_cb = DRW_pointcloud_batch_cache_dirty_tag;
  BKE_pointcloud_batch_cache_free_cb = DRW_pointcloud_batch_cache_free;

  BKE_volume_batch_cache_dirty_tag_cb = DRW_volume_batch_cache_dirty_tag;
  BKE_volume_batch_cache_free_cb = DRW_volume_batch_cache_free;

  BKE_grease_pencil_batch_cache_dirty_tag_cb = DRW_grease_pencil_batch_cache_dirty_tag;
  BKE_grease_pencil_batch_cache_free_cb = DRW_grease_pencil_batch_cache_free;

  BKE_subsurf_modifier_free_gpu_cache_cb = DRW_subdiv_cache_free;
}

void DRW_module_exit()
{
  GPU_TEXTURE_FREE_SAFE(g_select_buffer.texture_depth);
  GPU_FRAMEBUFFER_FREE_SAFE(g_select_buffer.framebuffer_depth_only);

  DRW_shaders_free();
}

/** \} */
