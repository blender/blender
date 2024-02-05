/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include <string>

#include "node_composite_util.hh"

#include "BLI_assert.h"
#include "BLI_dynstr.h"
#include "BLI_hash_mm3.hh"
#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "IMB_imbuf_types.hh"

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "DNA_image_types.h"

#include "BKE_context.hh"
#include "BKE_cryptomatte.hh"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"

#include "MEM_guardedalloc.h"

#include "RE_pipeline.h"

#include "COM_node_operation.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

#include <optional>

/* -------------------------------------------------------------------- */
/** \name Cryptomatte
 * \{ */

static blender::bke::cryptomatte::CryptomatteSessionPtr cryptomatte_init_from_node_render(
    const bNode &node, const bool use_meta_data)
{
  blender::bke::cryptomatte::CryptomatteSessionPtr session;

  Scene *scene = (Scene *)node.id;
  if (!scene) {
    return session;
  }
  BLI_assert(GS(scene->id.name) == ID_SCE);

  if (use_meta_data) {
    Render *render = RE_GetSceneRender(scene);
    RenderResult *render_result = render ? RE_AcquireResultRead(render) : nullptr;
    if (render_result) {
      session = blender::bke::cryptomatte::CryptomatteSessionPtr(
          BKE_cryptomatte_init_from_render_result(render_result));
    }
    if (render) {
      RE_ReleaseResult(render);
    }
  }

  if (session == nullptr) {
    session = blender::bke::cryptomatte::CryptomatteSessionPtr(
        BKE_cryptomatte_init_from_scene(scene));
  }
  return session;
}

static blender::bke::cryptomatte::CryptomatteSessionPtr cryptomatte_init_from_node_image(
    const Scene &scene, const bNode &node)
{
  blender::bke::cryptomatte::CryptomatteSessionPtr session;
  Image *image = (Image *)node.id;
  if (!image) {
    return session;
  }
  BLI_assert(GS(image->id.name) == ID_IM);

  NodeCryptomatte *node_cryptomatte = static_cast<NodeCryptomatte *>(node.storage);
  ImageUser *iuser = &node_cryptomatte->iuser;
  BKE_image_user_frame_calc(image, iuser, scene.r.cfra);
  ImBuf *ibuf = BKE_image_acquire_ibuf(image, iuser, nullptr);
  RenderResult *render_result = image->rr;
  if (render_result) {
    session = blender::bke::cryptomatte::CryptomatteSessionPtr(
        BKE_cryptomatte_init_from_render_result(render_result));
  }
  BKE_image_release_ibuf(image, ibuf, nullptr);
  return session;
}

static blender::bke::cryptomatte::CryptomatteSessionPtr cryptomatte_init_from_node(
    const Scene &scene, const bNode &node, const bool use_meta_data)
{
  blender::bke::cryptomatte::CryptomatteSessionPtr session;
  if (node.type != CMP_NODE_CRYPTOMATTE) {
    return session;
  }

  switch (node.custom1) {
    case CMP_NODE_CRYPTOMATTE_SOURCE_RENDER: {
      return cryptomatte_init_from_node_render(node, use_meta_data);
    }

    case CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE: {
      return cryptomatte_init_from_node_image(scene, node);
    }
  }
  return session;
}

static CryptomatteEntry *cryptomatte_find(const NodeCryptomatte &n, float encoded_hash)
{
  LISTBASE_FOREACH (CryptomatteEntry *, entry, &n.entries) {
    if (entry->encoded_hash == encoded_hash) {
      return entry;
    }
  }
  return nullptr;
}

static void cryptomatte_add(const Scene &scene,
                            bNode &node,
                            NodeCryptomatte &node_cryptomatte,
                            float encoded_hash)
{
  /* Check if entry already exist. */
  if (cryptomatte_find(node_cryptomatte, encoded_hash)) {
    return;
  }

  CryptomatteEntry *entry = MEM_cnew<CryptomatteEntry>(__func__);
  entry->encoded_hash = encoded_hash;
  blender::bke::cryptomatte::CryptomatteSessionPtr session = cryptomatte_init_from_node(
      scene, node, true);
  if (session) {
    BKE_cryptomatte_find_name(session.get(), encoded_hash, entry->name, sizeof(entry->name));
  }

  BLI_addtail(&node_cryptomatte.entries, entry);
}

static void cryptomatte_remove(NodeCryptomatte &n, float encoded_hash)
{
  CryptomatteEntry *entry = cryptomatte_find(n, encoded_hash);
  if (!entry) {
    return;
  }
  BLI_remlink(&n.entries, entry);
  MEM_freeN(entry);
}

void ntreeCompositCryptomatteSyncFromAdd(const Scene *scene, bNode *node)
{
  BLI_assert(ELEM(node->type, CMP_NODE_CRYPTOMATTE, CMP_NODE_CRYPTOMATTE_LEGACY));
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  if (n->runtime.add[0] != 0.0f) {
    cryptomatte_add(*scene, *node, *n, n->runtime.add[0]);
    zero_v3(n->runtime.add);
  }
}

void ntreeCompositCryptomatteSyncFromRemove(bNode *node)
{
  BLI_assert(ELEM(node->type, CMP_NODE_CRYPTOMATTE, CMP_NODE_CRYPTOMATTE_LEGACY));
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  if (n->runtime.remove[0] != 0.0f) {
    cryptomatte_remove(*n, n->runtime.remove[0]);
    zero_v3(n->runtime.remove);
  }
}
void ntreeCompositCryptomatteUpdateLayerNames(const Scene *scene, bNode *node)
{
  BLI_assert(node->type == CMP_NODE_CRYPTOMATTE);
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  BLI_freelistN(&n->runtime.layers);

  blender::bke::cryptomatte::CryptomatteSessionPtr session = cryptomatte_init_from_node(
      *scene, *node, false);

  if (session) {
    for (blender::StringRef layer_name :
         blender::bke::cryptomatte::BKE_cryptomatte_layer_names_get(*session))
    {
      CryptomatteLayer *layer = MEM_cnew<CryptomatteLayer>(__func__);
      layer_name.copy(layer->name);
      BLI_addtail(&n->runtime.layers, layer);
    }
  }
}

void ntreeCompositCryptomatteLayerPrefix(const Scene *scene,
                                         const bNode *node,
                                         char *r_prefix,
                                         size_t prefix_maxncpy)
{
  BLI_assert(node->type == CMP_NODE_CRYPTOMATTE);
  NodeCryptomatte *node_cryptomatte = (NodeCryptomatte *)node->storage;
  blender::bke::cryptomatte::CryptomatteSessionPtr session = cryptomatte_init_from_node(
      *scene, *node, false);
  std::string first_layer_name;

  if (session) {
    for (blender::StringRef layer_name :
         blender::bke::cryptomatte::BKE_cryptomatte_layer_names_get(*session))
    {
      if (first_layer_name.empty()) {
        first_layer_name = layer_name;
      }

      if (layer_name == node_cryptomatte->layer_name) {
        BLI_strncpy(r_prefix, node_cryptomatte->layer_name, prefix_maxncpy);
        return;
      }
    }
  }

  const char *cstr = first_layer_name.c_str();
  BLI_strncpy(r_prefix, cstr, prefix_maxncpy);
}

CryptomatteSession *ntreeCompositCryptomatteSession(const Scene *scene, bNode *node)
{
  blender::bke::cryptomatte::CryptomatteSessionPtr session_ptr = cryptomatte_init_from_node(
      *scene, *node, true);
  return session_ptr.release();
}

namespace blender::nodes::node_composite_cryptomatte_cc {

NODE_STORAGE_FUNCS(NodeCryptomatte)

static bNodeSocketTemplate cmp_node_cryptomatte_out[] = {
    {SOCK_RGBA, N_("Image")},
    {SOCK_FLOAT, N_("Matte")},
    {SOCK_RGBA, N_("Pick")},
    {-1, ""},
};

static void cmp_node_cryptomatte_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({0.0f, 0.0f, 0.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Color>("Image");
  b.add_output<decl::Float>("Matte");
  b.add_output<decl::Color>("Pick");
}

static void node_init_cryptomatte(bNodeTree * /*ntree*/, bNode *node)
{
  NodeCryptomatte *user = MEM_cnew<NodeCryptomatte>(__func__);
  node->storage = user;
}

static void node_init_api_cryptomatte(const bContext *C, PointerRNA *ptr)
{
  Scene *scene = CTX_data_scene(C);
  bNode *node = static_cast<bNode *>(ptr->data);
  BLI_assert(node->type == CMP_NODE_CRYPTOMATTE);
  node->id = &scene->id;
  id_us_plus(node->id);
}

static void node_free_cryptomatte(bNode *node)
{
  BLI_assert(ELEM(node->type, CMP_NODE_CRYPTOMATTE, CMP_NODE_CRYPTOMATTE_LEGACY));
  NodeCryptomatte *nc = static_cast<NodeCryptomatte *>(node->storage);

  if (nc) {
    MEM_SAFE_FREE(nc->matte_id);
    BLI_freelistN(&nc->runtime.layers);
    BLI_freelistN(&nc->entries);
    MEM_freeN(nc);
  }
}

static void node_copy_cryptomatte(bNodeTree * /*dst_ntree*/,
                                  bNode *dest_node,
                                  const bNode *src_node)
{
  NodeCryptomatte *src_nc = static_cast<NodeCryptomatte *>(src_node->storage);
  NodeCryptomatte *dest_nc = static_cast<NodeCryptomatte *>(MEM_dupallocN(src_nc));

  BLI_duplicatelist(&dest_nc->entries, &src_nc->entries);
  BLI_listbase_clear(&dest_nc->runtime.layers);
  dest_nc->matte_id = static_cast<char *>(MEM_dupallocN(src_nc->matte_id));
  dest_node->storage = dest_nc;
}

static bool node_poll_cryptomatte(const bNodeType * /*ntype*/,
                                  const bNodeTree *ntree,
                                  const char **r_disabled_hint)
{
  if (STREQ(ntree->idname, "CompositorNodeTree")) {
    Scene *scene;

    /* See node_composit_poll_rlayers. */
    for (scene = static_cast<Scene *>(G.main->scenes.first); scene;
         scene = static_cast<Scene *>(scene->id.next))
    {
      if (scene->nodetree == ntree) {
        break;
      }
    }

    if (scene == nullptr) {
      *r_disabled_hint = RPT_(
          "The node tree must be the compositing node tree of any scene in the file");
    }
    return scene != nullptr;
  }
  *r_disabled_hint = RPT_("Not a compositor node tree");
  return false;
}

using namespace blender::realtime_compositor;

class CryptoMatteOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Vector<GPUTexture *> layers = get_layers();
    if (layers.is_empty()) {
      allocate_invalid();
      return;
    }

    Result &output_pick = get_result("Pick");
    if (output_pick.should_compute()) {
      compute_pick(layers);
    }

    Result &matte_output = get_result("Matte");
    Result &image_output = get_result("Image");
    if (!matte_output.should_compute() && !image_output.should_compute()) {
      return;
    }

    Result matte = compute_matte(layers);

    if (image_output.should_compute()) {
      compute_image(matte);
    }

    if (matte_output.should_compute()) {
      matte_output.steal_data(matte);
    }
    else {
      matte.release();
    }
  }

  void allocate_invalid()
  {
    Result &pick = get_result("Pick");
    if (pick.should_compute()) {
      pick.allocate_invalid();
    }

    Result &matte = get_result("Matte");
    if (matte.should_compute()) {
      matte.allocate_invalid();
    }

    Result &image = get_result("Image");
    if (image.should_compute()) {
      image.allocate_invalid();
    }
  }

  /* Computes the pick result, which is a special human-viewable image that the user can pick
   * entities from using the Cryptomatte picker operator. See the shader for more information. */
  void compute_pick(Vector<GPUTexture *> &layers)
  {
    /* See the comment below for why full precision is necessary. */
    GPUShader *shader = context().get_shader("compositor_cryptomatte_pick", ResultPrecision::Full);
    GPU_shader_bind(shader);

    GPUTexture *first_layer = layers[0];
    const int input_unit = GPU_shader_get_sampler_binding(shader, "first_layer_tx");
    GPU_texture_bind(first_layer, input_unit);

    Result &output_pick = get_result("Pick");

    /* Promote to full precision since it stores the identifiers of the first Cryptomatte rank,
     * which is a 32-bit float. See the shader for more information. */
    output_pick.set_precision(ResultPrecision::Full);

    const Domain domain = compute_domain();
    output_pick.allocate_texture(domain);
    output_pick.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    GPU_texture_unbind(first_layer);
    output_pick.unbind_as_image();
  }

  /* Computes and returns the matte by accumulating the coverage of all entities whose identifiers
   * are selected by the user, across all layers. See the shader for more information. */
  Result compute_matte(Vector<GPUTexture *> &layers)
  {
    const Domain domain = compute_domain();
    Result output_matte = context().create_temporary_result(ResultType::Float);
    output_matte.allocate_texture(domain);

    /* Clear the matte to zero to ready it to accumulate the coverage. */
    const float4 zero_color = float4(0.0f);
    GPU_texture_clear(output_matte.texture(), GPU_DATA_FLOAT, zero_color);

    Vector<float> identifiers = get_identifiers();
    /* The user haven't selected any entities, return the currently zero matte. */
    if (identifiers.is_empty()) {
      return output_matte;
    }

    GPUShader *shader = context().get_shader("compositor_cryptomatte_matte");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "identifiers_count", identifiers.size());
    GPU_shader_uniform_1f_array(shader, "identifiers", identifiers.size(), identifiers.data());

    for (GPUTexture *layer : layers) {
      const int input_unit = GPU_shader_get_sampler_binding(shader, "layer_tx");
      GPU_texture_bind(layer, input_unit);

      /* Bind the matte with read access, since we will be accumulating in it. */
      output_matte.bind_as_image(shader, "matte_img", true);

      compute_dispatch_threads_at_least(shader, domain.size);

      GPU_texture_unbind(layer);
      output_matte.unbind_as_image();
    }

    GPU_shader_unbind();

    return output_matte;
  }

  /* Computes the output image result by pre-multiplying the matte to the image. */
  void compute_image(Result &matte)
  {
    GPUShader *shader = context().get_shader("compositor_cryptomatte_image");
    GPU_shader_bind(shader);

    Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    matte.bind_as_texture(shader, "matte_tx");

    const Domain domain = compute_domain();
    Result &image_output = get_result("Image");
    image_output.allocate_texture(domain);
    image_output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    input_image.unbind_as_texture();
    matte.unbind_as_texture();
    image_output.unbind_as_image();
  }

  /* Returns all the relevant Cryptomatte layers from the selected source. */
  Vector<GPUTexture *> get_layers()
  {
    switch (get_source()) {
      case CMP_NODE_CRYPTOMATTE_SOURCE_RENDER:
        return get_layers_from_render();
      case CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE:
        return get_layers_from_image();
    }

    BLI_assert_unreachable();
    return Vector<GPUTexture *>();
  }

  /* Returns all the relevant Cryptomatte layers from the selected render. */
  Vector<GPUTexture *> get_layers_from_render()
  {
    Vector<GPUTexture *> layers;

    Scene *scene = get_scene();
    if (!scene) {
      return layers;
    }

    Render *render = RE_GetSceneRender(scene);
    if (!render) {
      return layers;
    }

    RenderResult *render_result = RE_AcquireResultRead(render);
    if (!render_result) {
      RE_ReleaseResult(render);
      return layers;
    }

    int view_layer_index;
    const std::string type_name = get_type_name();
    LISTBASE_FOREACH_INDEX (ViewLayer *, view_layer, &scene->view_layers, view_layer_index) {
      RenderLayer *render_layer = RE_GetRenderLayer(render_result, view_layer->name);
      if (!render_layer) {
        continue;
      }

      LISTBASE_FOREACH (RenderPass *, render_pass, &render_layer->passes) {
        /* We are only interested in passes of the current view. Except if the current view is
         * unnamed, that is, in the case of mono rendering, in which case we just return the first
         * view. */
        if (!context().get_view_name().is_empty() &&
            context().get_view_name() != render_pass->view)
        {
          continue;
        }

        /* If the combined pass name doesn't start with the Cryptomatte type name, then it is not a
         * Cryptomatte layer. */
        const std::string combined_name = get_combined_layer_pass_name(render_layer, render_pass);
        if (combined_name == type_name || !StringRef(combined_name).startswith(type_name)) {
          continue;
        }

        GPUTexture *pass_texture = context().get_input_texture(
            scene, view_layer_index, render_pass->name);
        layers.append(pass_texture);
      }

      if (!layers.is_empty()) {
        break;
      }
    }

    RE_ReleaseResult(render);

    return layers;
  }

  /* Returns all the relevant Cryptomatte layers from the selected EXR image. */
  Vector<GPUTexture *> get_layers_from_image()
  {
    Vector<GPUTexture *> layers;

    Image *image = get_image();
    if (!image || image->type != IMA_TYPE_MULTILAYER) {
      return layers;
    }

    /* The render result structure of the image is populated as a side effect of the acquisition of
     * an image buffer, so acquire an image buffer and immediately release it since it is not
     * actually needed. */
    ImageUser image_user_for_layer = *get_image_user();
    ImBuf *image_buffer = BKE_image_acquire_ibuf(image, &image_user_for_layer, nullptr);
    BKE_image_release_ibuf(image, image_buffer, nullptr);
    if (!image_buffer || !image->rr) {
      return layers;
    }

    int layer_index;
    const std::string type_name = get_type_name();
    LISTBASE_FOREACH_INDEX (RenderLayer *, render_layer, &image->rr->layers, layer_index) {
      /* If the Cryptomatte type name name doesn't start with the layer name, then it is not a
       * Cryptomatte layer. Unless it is an unnamed layer, in which case, we need to check its
       * passes. */
      const bool is_unnamed_layer = render_layer->name[0] == '\0';
      if (!is_unnamed_layer && !StringRefNull(type_name).startswith(render_layer->name)) {
        continue;
      }

      image_user_for_layer.layer = layer_index;
      LISTBASE_FOREACH (RenderPass *, render_pass, &render_layer->passes) {
        /* If the combined pass name doesn't start with the Cryptomatte type name, then it is not a
         * Cryptomatte layer. */
        const std::string combined_name = get_combined_layer_pass_name(render_layer, render_pass);
        if (combined_name == type_name || !StringRef(combined_name).startswith(type_name)) {
          continue;
        }

        GPUTexture *pass_texture = context().cache_manager().cached_images.get(
            context(), image, &image_user_for_layer, render_pass->name);
        layers.append(pass_texture);
      }

      /* If we already found Cryptomatte layers, no need to check other render layers. */
      if (!layers.is_empty()) {
        return layers;
      }
    }

    return layers;
  }

  /* Returns the combined name of the render layer and pass using the EXR convention of a period
   * separator. */
  std::string get_combined_layer_pass_name(RenderLayer *render_layer, RenderPass *render_pass)
  {
    if (render_layer->name[0] == '\0') {
      return std::string(render_pass->name);
    }
    return std::string(render_layer->name) + "." + std::string(render_pass->name);
  }

  /* Get the selected type name of the Cryptomatte from the metadata of the image/render. This type
   * name will be used to identify the corresponding layers in the source image/render. See the
   * "EXR File: Layer Naming" section of the Cryptomatte specification for more information on what
   * this represents. */
  std::string get_type_name()
  {
    char type_name[MAX_NAME];
    ntreeCompositCryptomatteLayerPrefix(
        &context().get_scene(), &bnode(), type_name, sizeof(type_name));
    return std::string(type_name);
  }

  /* Get the identifiers of the entities selected by the user to generate a matte from. The
   * identifiers are hashes of the names of the entities encoded in floats. See the "ID Generation"
   * section of the Cryptomatte specification for more information. */
  Vector<float> get_identifiers()
  {
    Vector<float> identifiers;
    LISTBASE_FOREACH (CryptomatteEntry *, cryptomatte_entry, &node_storage(bnode()).entries) {
      identifiers.append(cryptomatte_entry->encoded_hash);
    }
    return identifiers;
  }

  /* The domain should be centered with the same size as the source. In case of invalid source,
   * fallback to the domain inferred from the input. */
  Domain compute_domain() override
  {
    switch (get_source()) {
      case CMP_NODE_CRYPTOMATTE_SOURCE_RENDER:
        return Domain(context().get_render_size());
      case CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE:
        return compute_image_domain();
    }

    BLI_assert_unreachable();
    return Domain::identity();
  }

  /* In case of a render source, the domain should be centered with the same size as the render. In
   * case of an invalid render, fallback to the domain inferred from the input. */
  Domain compute_render_domain()
  {
    BLI_assert(get_source() == CMP_NODE_CRYPTOMATTE_SOURCE_RENDER);

    Scene *scene = get_scene();
    if (!scene) {
      return NodeOperation::compute_domain();
    }

    Render *render = RE_GetSceneRender(scene);
    if (!render) {
      return NodeOperation::compute_domain();
    }

    RenderResult *render_result = RE_AcquireResultRead(render);
    if (!render_result) {
      RE_ReleaseResult(render);
      return NodeOperation::compute_domain();
    }

    const int2 render_size = int2(render_result->rectx, render_result->rectx);
    RE_ReleaseResult(render);
    return Domain(render_size);
  }

  /* In case of an image source, the domain should be centered with the same size as the source
   * image. In case of an invalid image, fallback to the domain inferred from the input. */
  Domain compute_image_domain()
  {
    BLI_assert(get_source() == CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE);

    Image *image = get_image();
    if (!image) {
      return NodeOperation::compute_domain();
    }

    ImageUser image_user = *get_image_user();
    ImBuf *image_buffer = BKE_image_acquire_ibuf(image, &image_user, nullptr);
    if (!image_buffer) {
      return NodeOperation::compute_domain();
    }

    const int2 image_size = int2(image_buffer->x, image_buffer->y);
    BKE_image_release_ibuf(image, image_buffer, nullptr);
    return Domain(image_size);
  }

  const ImageUser *get_image_user()
  {
    BLI_assert(get_source() == CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE);
    return &node_storage(bnode()).iuser;
  }

  Scene *get_scene()
  {
    BLI_assert(get_source() == CMP_NODE_CRYPTOMATTE_SOURCE_RENDER);
    return reinterpret_cast<Scene *>(bnode().id);
  }

  Image *get_image()
  {
    BLI_assert(get_source() == CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE);
    return reinterpret_cast<Image *>(bnode().id);
  }

  CMPNodeCryptomatteSource get_source()
  {
    return static_cast<CMPNodeCryptomatteSource>(bnode().custom1);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new CryptoMatteOperation(context, node);
}

}  // namespace blender::nodes::node_composite_cryptomatte_cc

void register_node_type_cmp_cryptomatte()
{
  namespace file_ns = blender::nodes::node_composite_cryptomatte_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_CRYPTOMATTE, "Cryptomatte", NODE_CLASS_MATTE);
  ntype.declare = file_ns::cmp_node_cryptomatte_declare;
  blender::bke::node_type_size(&ntype, 240, 100, 700);
  ntype.initfunc = file_ns::node_init_cryptomatte;
  ntype.initfunc_api = file_ns::node_init_api_cryptomatte;
  ntype.poll = file_ns::node_poll_cryptomatte;
  node_type_storage(
      &ntype, "NodeCryptomatte", file_ns::node_free_cryptomatte, file_ns::node_copy_cryptomatte);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cryptomatte Legacy
 * \{ */

bNodeSocket *ntreeCompositCryptomatteAddSocket(bNodeTree *ntree, bNode *node)
{
  BLI_assert(node->type == CMP_NODE_CRYPTOMATTE_LEGACY);
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  char sockname[32];
  n->inputs_num++;
  SNPRINTF(sockname, "Crypto %.2d", n->inputs_num - 1);
  bNodeSocket *sock = nodeAddStaticSocket(
      ntree, node, SOCK_IN, SOCK_RGBA, PROP_NONE, nullptr, sockname);
  return sock;
}

int ntreeCompositCryptomatteRemoveSocket(bNodeTree *ntree, bNode *node)
{
  BLI_assert(node->type == CMP_NODE_CRYPTOMATTE_LEGACY);
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  if (n->inputs_num < 2) {
    return 0;
  }
  bNodeSocket *sock = static_cast<bNodeSocket *>(node->inputs.last);
  nodeRemoveSocket(ntree, node, sock);
  n->inputs_num--;
  return 1;
}

namespace blender::nodes::node_composite_legacy_cryptomatte_cc {

static void node_init_cryptomatte_legacy(bNodeTree *ntree, bNode *node)
{
  namespace file_ns = blender::nodes::node_composite_cryptomatte_cc;
  file_ns::node_init_cryptomatte(ntree, node);

  nodeAddStaticSocket(ntree, node, SOCK_IN, SOCK_RGBA, PROP_NONE, "image", "Image");

  /* Add three inputs by default, as recommended by the Cryptomatte specification. */
  ntreeCompositCryptomatteAddSocket(ntree, node);
  ntreeCompositCryptomatteAddSocket(ntree, node);
  ntreeCompositCryptomatteAddSocket(ntree, node);
}

using namespace blender::realtime_compositor;

class CryptoMatteOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    get_input("image").pass_through(get_result("Image"));
    get_result("Matte").allocate_invalid();
    get_result("Pick").allocate_invalid();
    context().set_info_message("Viewport compositor setup not fully supported");
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new CryptoMatteOperation(context, node);
}

}  // namespace blender::nodes::node_composite_legacy_cryptomatte_cc

void register_node_type_cmp_cryptomatte_legacy()
{
  namespace legacy_file_ns = blender::nodes::node_composite_legacy_cryptomatte_cc;
  namespace file_ns = blender::nodes::node_composite_cryptomatte_cc;

  static bNodeType ntype;

  cmp_node_type_base(
      &ntype, CMP_NODE_CRYPTOMATTE_LEGACY, "Cryptomatte (Legacy)", NODE_CLASS_MATTE);
  blender::bke::node_type_socket_templates(&ntype, nullptr, file_ns::cmp_node_cryptomatte_out);
  ntype.initfunc = legacy_file_ns::node_init_cryptomatte_legacy;
  node_type_storage(
      &ntype, "NodeCryptomatte", file_ns::node_free_cryptomatte, file_ns::node_copy_cryptomatte);
  ntype.gather_link_search_ops = nullptr;
  ntype.get_compositor_operation = legacy_file_ns::get_compositor_operation;
  ntype.realtime_compositor_unsupported_message = N_(
      "Node not supported in the Viewport compositor");

  nodeRegisterType(&ntype);
}

/** \} */
