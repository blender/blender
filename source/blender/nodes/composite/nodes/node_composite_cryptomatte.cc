/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include <cstring>
#include <string>

#include <fmt/format.h>

#include "node_composite_util.hh"

#include "BLI_assert.h"
#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector_types.hh"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "IMB_imbuf_types.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "DNA_image_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "BKE_compositor.hh"
#include "BKE_context.hh"
#include "BKE_cryptomatte.hh"
#include "BKE_global.hh"
#include "BKE_image.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_node.hh"

#include "UI_resources.hh"

#include "MEM_guardedalloc.h"

#include "RE_pipeline.h"

#include "NOD_node_extra_info.hh"

#include "COM_node_operation.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

#include <optional>

/* -------------------------------------------------------------------- */
/** \name Cryptomatte
 * \{ */

static blender::bke::cryptomatte::CryptomatteSessionPtr cryptomatte_init_from_node_render(
    const bNode &node, const bool build_meta_data)
{
  blender::bke::cryptomatte::CryptomatteSessionPtr session;

  Scene *scene = (Scene *)node.id;
  if (!scene) {
    return session;
  }
  BLI_assert(GS(scene->id.name) == ID_SCE);

  session = blender::bke::cryptomatte::CryptomatteSessionPtr(
      BKE_cryptomatte_init_from_scene(scene, build_meta_data));
  return session;
}

static blender::bke::cryptomatte::CryptomatteSessionPtr cryptomatte_init_from_node_image(
    const bNode &node)
{
  blender::bke::cryptomatte::CryptomatteSessionPtr session;
  Image *image = (Image *)node.id;
  if (!image) {
    return session;
  }
  BLI_assert(GS(image->id.name) == ID_IM);

  /* Construct an image user to retrieve the first image in the sequence, since the frame number
   * might correspond to a non-existing image. We explicitly do not support the case where the
   * image sequence has a changing structure. */
  ImageUser image_user = {};
  image_user.framenr = BKE_image_sequence_guess_offset(image);

  ImBuf *ibuf = BKE_image_acquire_ibuf(image, &image_user, nullptr);
  RenderResult *render_result = image->rr;
  if (render_result) {
    session = blender::bke::cryptomatte::CryptomatteSessionPtr(
        BKE_cryptomatte_init_from_render_result(render_result));
  }
  BKE_image_release_ibuf(image, ibuf, nullptr);
  return session;
}

static blender::bke::cryptomatte::CryptomatteSessionPtr cryptomatte_init_from_node(
    const bNode &node, const bool build_meta_data)
{
  blender::bke::cryptomatte::CryptomatteSessionPtr session;
  if (node.type_legacy != CMP_NODE_CRYPTOMATTE) {
    return session;
  }

  switch (node.custom1) {
    case CMP_NODE_CRYPTOMATTE_SOURCE_RENDER: {
      return cryptomatte_init_from_node_render(node, build_meta_data);
    }

    case CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE: {
      return cryptomatte_init_from_node_image(node);
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

static void cryptomatte_add(bNode &node, NodeCryptomatte &node_cryptomatte, float encoded_hash)
{
  /* Check if entry already exist. */
  if (cryptomatte_find(node_cryptomatte, encoded_hash)) {
    return;
  }

  CryptomatteEntry *entry = MEM_callocN<CryptomatteEntry>(__func__);
  entry->encoded_hash = encoded_hash;
  blender::bke::cryptomatte::CryptomatteSessionPtr session = cryptomatte_init_from_node(node,
                                                                                        true);
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

void ntreeCompositCryptomatteSyncFromAdd(bNode *node)
{
  BLI_assert(ELEM(node->type_legacy, CMP_NODE_CRYPTOMATTE, CMP_NODE_CRYPTOMATTE_LEGACY));
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  if (n->runtime.add[0] != 0.0f) {
    cryptomatte_add(*node, *n, n->runtime.add[0]);
    zero_v3(n->runtime.add);
  }
}

void ntreeCompositCryptomatteSyncFromRemove(bNode *node)
{
  BLI_assert(ELEM(node->type_legacy, CMP_NODE_CRYPTOMATTE, CMP_NODE_CRYPTOMATTE_LEGACY));
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  if (n->runtime.remove[0] != 0.0f) {
    cryptomatte_remove(*n, n->runtime.remove[0]);
    zero_v3(n->runtime.remove);
  }
}
void ntreeCompositCryptomatteUpdateLayerNames(bNode *node)
{
  BLI_assert(node->type_legacy == CMP_NODE_CRYPTOMATTE);
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  BLI_freelistN(&n->runtime.layers);

  blender::bke::cryptomatte::CryptomatteSessionPtr session = cryptomatte_init_from_node(*node,
                                                                                        false);

  if (session) {
    for (blender::StringRef layer_name :
         blender::bke::cryptomatte::BKE_cryptomatte_layer_names_get(*session))
    {
      CryptomatteLayer *layer = MEM_callocN<CryptomatteLayer>(__func__);
      layer_name.copy_utf8_truncated(layer->name);
      BLI_addtail(&n->runtime.layers, layer);
    }
  }
}

void ntreeCompositCryptomatteLayerPrefix(const bNode *node, char *r_prefix, size_t prefix_maxncpy)
{
  BLI_assert(node->type_legacy == CMP_NODE_CRYPTOMATTE);
  NodeCryptomatte *node_cryptomatte = (NodeCryptomatte *)node->storage;
  blender::bke::cryptomatte::CryptomatteSessionPtr session = cryptomatte_init_from_node(*node,
                                                                                        false);
  std::string first_layer_name;

  if (session) {
    for (blender::StringRef layer_name :
         blender::bke::cryptomatte::BKE_cryptomatte_layer_names_get(*session))
    {
      if (first_layer_name.empty()) {
        first_layer_name = layer_name;
      }

      if (layer_name == node_cryptomatte->layer_name) {
        BLI_strncpy_utf8(r_prefix, node_cryptomatte->layer_name, prefix_maxncpy);
        return;
      }
    }
  }

  const char *cstr = first_layer_name.c_str();
  BLI_strncpy_utf8(r_prefix, cstr, prefix_maxncpy);
}

CryptomatteSession *ntreeCompositCryptomatteSession(bNode *node)
{
  blender::bke::cryptomatte::CryptomatteSessionPtr session_ptr = cryptomatte_init_from_node(*node,
                                                                                            true);
  return session_ptr.release();
}

namespace blender::nodes::node_composite_base_cryptomatte_cc {

NODE_STORAGE_FUNCS(NodeCryptomatte)

using namespace blender::compositor;

class BaseCryptoMatteOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  /* Should return the input image result. */
  virtual Result &get_input_image() = 0;

  /* Should returns all the Cryptomatte layers in order. */
  virtual Vector<Result> get_layers() = 0;

  /* If only a subset area of the Cryptomatte layers is to be considered, this method should return
   * the lower bound of that area. The upper bound will be derived from the operation domain. */
  virtual int2 get_layers_lower_bound()
  {
    return int2(0);
  }

  void execute() override
  {
    Vector<Result> layers = get_layers();
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
  void compute_pick(const Vector<Result> &layers)
  {
    Result &output_pick = get_result("Pick");
    /* Promote to full precision since it stores the identifiers of the first Cryptomatte rank,
     * which is a 32-bit float. See the shader for more information. */
    output_pick.set_precision(ResultPrecision::Full);
    /* Inform viewers that the pick result should not be color managed. */
    output_pick.meta_data.is_non_color_data = true;

    if (this->context().use_gpu()) {
      this->compute_pick_gpu(layers);
    }
    else {
      this->compute_pick_cpu(layers);
    }
  }

  void compute_pick_gpu(const Vector<Result> &layers)
  {
    /* See this->compute_pick for why full precision is necessary. */
    gpu::Shader *shader = context().get_shader("compositor_cryptomatte_pick",
                                               ResultPrecision::Full);
    GPU_shader_bind(shader);

    const int2 lower_bound = this->get_layers_lower_bound();
    GPU_shader_uniform_2iv(shader, "lower_bound", lower_bound);

    const Result &first_layer = layers[0];
    first_layer.bind_as_texture(shader, "first_layer_tx");

    const Domain domain = compute_domain();
    Result &output_pick = get_result("Pick");
    output_pick.allocate_texture(domain);
    output_pick.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    first_layer.unbind_as_texture();
    output_pick.unbind_as_image();
  }

  void compute_pick_cpu(const Vector<Result> &layers)
  {
    const int2 lower_bound = this->get_layers_lower_bound();

    const Result &first_layer = layers[0];

    const Domain domain = this->compute_domain();
    Result &output = this->get_result("Pick");
    output.allocate_texture(domain);

    /* Blender provides a Cryptomatte picker operator (UI_OT_eyedropper_color) that can pick a
     * Cryptomatte entity from an image. That image is a specially encoded image that the picker
     * operator can understand. In particular, its red channel is the identifier of the entity in
     * the first rank, while the green and blue channels are arbitrary [0, 1] compressed versions
     * of the identifier to make the image more humane-viewable, but they are actually ignored by
     * the picker operator, as can be seen in functions like eyedropper_color_sample_text_update,
     * where only the red channel is considered.
     *
     * This shader just computes this special image given the first Cryptomatte layer. The output
     * needs to be in full precision since the identifier is a 32-bit float.
     *
     * This is the same concept as the "keyable" image described in section "Matte Extraction:
     * Implementation Details" in the original Cryptomatte publication:
     *
     *   Friedman, Jonah, and Andrew C. Jones. "Fully automatic id mattes with support for motion
     * blur and transparency." ACM SIGGRAPH 2015 Posters. 2015. 1-1.
     *
     * Except we put the identifier in the red channel by convention instead of the suggested blue
     * channel. */
    parallel_for(domain.size, [&](const int2 texel) {
      /* Each layer stores two ranks, each rank contains a pair, the identifier and the coverage of
       * the entity identified by the identifier. */
      float2 first_rank = float4(first_layer.load_pixel<Color>(texel + lower_bound)).xy();
      float id_of_first_rank = first_rank.x;

      /* There is no logic to this, we just compute arbitrary compressed versions of the identifier
       * in the [0, 1] range to make the image more human-viewable. */
      uint32_t hash_value;
      std::memcpy(&hash_value, &id_of_first_rank, sizeof(uint32_t));
      float green = float(hash_value << 8) / float(0xFFFFFFFFu);
      float blue = float(hash_value << 16) / float(0xFFFFFFFFu);

      output.store_pixel(texel, Color(id_of_first_rank, green, blue, 1.0f));
    });
  }

  /* Computes and returns the matte by accumulating the coverage of all entities whose identifiers
   * are selected by the user, across all layers. See the shader for more information. */
  Result compute_matte(const Vector<Result> &layers)
  {
    if (this->context().use_gpu()) {
      return this->compute_matte_gpu(layers);
    }

    return this->compute_matte_cpu(layers);
  }

  Result compute_matte_gpu(const Vector<Result> &layers)
  {
    const Domain domain = compute_domain();
    Result output_matte = context().create_result(ResultType::Float);
    output_matte.allocate_texture(domain);

    /* Clear the matte to zero to ready it to accumulate the coverage. */
    const float4 zero_color = float4(0.0f);
    GPU_texture_clear(output_matte, GPU_DATA_FLOAT, zero_color);

    Vector<float> identifiers = get_identifiers();
    /* The user haven't selected any entities, return the currently zero matte. */
    if (identifiers.is_empty()) {
      return output_matte;
    }

    gpu::Shader *shader = context().get_shader("compositor_cryptomatte_matte");
    GPU_shader_bind(shader);

    const int2 lower_bound = this->get_layers_lower_bound();
    GPU_shader_uniform_2iv(shader, "lower_bound", lower_bound);
    GPU_shader_uniform_1i(shader, "identifiers_count", identifiers.size());
    GPU_shader_uniform_1f_array(shader, "identifiers", identifiers.size(), identifiers.data());

    for (const Result &layer : layers) {
      layer.bind_as_texture(shader, "layer_tx");

      /* Bind the matte with read access, since we will be accumulating in it. */
      output_matte.bind_as_image(shader, "matte_img", true);

      compute_dispatch_threads_at_least(shader, domain.size);

      layer.unbind_as_texture();
      output_matte.unbind_as_image();
    }

    GPU_shader_unbind();

    return output_matte;
  }

  Result compute_matte_cpu(const Vector<Result> &layers)
  {
    const Domain domain = compute_domain();
    Result matte = context().create_result(ResultType::Float);
    matte.allocate_texture(domain);

    /* Clear the matte to zero to ready it to accumulate the coverage. */
    parallel_for(domain.size, [&](const int2 texel) { matte.store_pixel(texel, 0.0f); });

    Vector<float> identifiers = get_identifiers();
    /* The user haven't selected any entities, return the currently zero matte. */
    if (identifiers.is_empty()) {
      return matte;
    }

    const int2 lower_bound = this->get_layers_lower_bound();
    for (const Result &layer_result : layers) {
      /* Loops over all identifiers selected by the user, and accumulate the coverage of ranks
       * whose identifiers match that of the user selected identifiers.
       *
       * This is described in section "Matte Extraction: Implementation Details" in the original
       * Cryptomatte publication:
       *
       *   Friedman, Jonah, and Andrew C. Jones. "Fully automatic id mattes with support for motion
       * blur and transparency." ACM SIGGRAPH 2015 Posters. 2015. 1-1.
       */
      parallel_for(domain.size, [&](const int2 texel) {
        float4 layer = float4(layer_result.load_pixel<Color>(texel + lower_bound));

        /* Each Cryptomatte layer stores two ranks. */
        float2 first_rank = layer.xy();
        float2 second_rank = layer.zw();

        /* Each Cryptomatte rank stores a pair of an identifier and the coverage of the entity
         * identified by that identifier. */
        float identifier_of_first_rank = first_rank.x;
        float coverage_of_first_rank = first_rank.y;
        float identifier_of_second_rank = second_rank.x;
        float coverage_of_second_rank = second_rank.y;

        /* Loop over all identifiers selected by the user, if the identifier of either of the ranks
         * match it, accumulate its coverage. */
        float total_coverage = 0.0f;
        for (const float &identifier : identifiers) {
          if (identifier_of_first_rank == identifier) {
            total_coverage += coverage_of_first_rank;
          }
          if (identifier_of_second_rank == identifier) {
            total_coverage += coverage_of_second_rank;
          }
        }

        /* Add the total coverage to the coverage accumulated by previous layers. */
        matte.store_pixel(texel, matte.load_pixel<float>(texel) + total_coverage);
      });
    }

    return matte;
  }

  /* Computes the output image result by pre-multiplying the matte to the image. */
  void compute_image(const Result &matte)
  {
    if (this->context().use_gpu()) {
      this->compute_image_gpu(matte);
    }
    else {
      this->compute_image_cpu(matte);
    }
  }

  void compute_image_gpu(const Result &matte)
  {
    gpu::Shader *shader = context().get_shader("compositor_cryptomatte_image");
    GPU_shader_bind(shader);

    Result &input_image = get_input_image();
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

  void compute_image_cpu(const Result &matte)
  {
    Result &input = get_input_image();

    const Domain domain = compute_domain();
    Result &output = get_result("Image");
    output.allocate_texture(domain);

    parallel_for(domain.size, [&](const int2 texel) {
      float4 input_color = float4(input.load_pixel<Color, true>(texel));
      float input_matte = matte.load_pixel<float>(texel);

      /* Premultiply the alpha to the image. */
      output.store_pixel(texel, Color(input_color * input_matte));
    });
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
};

}  // namespace blender::nodes::node_composite_base_cryptomatte_cc

namespace blender::nodes::node_composite_cryptomatte_cc {

NODE_STORAGE_FUNCS(NodeCryptomatte)

static bke::bNodeSocketTemplate cmp_node_cryptomatte_out[] = {
    {SOCK_RGBA, N_("Image")},
    {SOCK_FLOAT, N_("Matte")},
    {SOCK_RGBA, N_("Pick")},
    {-1, ""},
};

static void cmp_node_cryptomatte_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({0.0f, 0.0f, 0.0f, 1.0f})
      .structure_type(StructureType::Dynamic);

  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic);
  b.add_output<decl::Float>("Matte").structure_type(StructureType::Dynamic);
  b.add_output<decl::Color>("Pick").structure_type(StructureType::Dynamic);
}

static void node_init_cryptomatte(bNodeTree * /*ntree*/, bNode *node)
{
  NodeCryptomatte *user = MEM_callocN<NodeCryptomatte>(__func__);
  node->storage = user;
}

static void node_init_api_cryptomatte(const bContext *C, PointerRNA *ptr)
{
  Scene *scene = CTX_data_scene(C);
  bNode *node = static_cast<bNode *>(ptr->data);
  BLI_assert(node->type_legacy == CMP_NODE_CRYPTOMATTE);
  node->id = &scene->id;
  id_us_plus(node->id);
}

static void node_free_cryptomatte(bNode *node)
{
  BLI_assert(ELEM(node->type_legacy, CMP_NODE_CRYPTOMATTE, CMP_NODE_CRYPTOMATTE_LEGACY));
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

static void node_update_cryptomatte(bNodeTree *ntree, bNode *node)
{
  cmp_node_update_default(ntree, node);
  ntreeCompositCryptomatteUpdateLayerNames(node);
}

static void node_extra_info(NodeExtraInfoParams &parameters)
{
  if (parameters.node.custom1 != CMP_NODE_CRYPTOMATTE_SOURCE_RENDER) {
    return;
  }

  SpaceNode *space_node = CTX_wm_space_node(&parameters.C);
  if (space_node->node_tree_sub_type != SNODE_COMPOSITOR_SCENE) {
    NodeExtraInfoRow row;
    row.text = RPT_("Node Unsupported");
    row.tooltip = TIP_(
        "The Cryptomatte node in render mode is only supported for scene compositing");
    row.icon = ICON_ERROR;
    parameters.rows.append(std::move(row));
    return;
  }

  /* EEVEE supports passes. */
  const Scene *scene = CTX_data_scene(&parameters.C);
  if (StringRef(scene->r.engine) == RE_engine_id_BLENDER_EEVEE) {
    return;
  }

  if (!bke::compositor::is_viewport_compositor_used(parameters.C)) {
    return;
  }

  NodeExtraInfoRow row;
  row.text = RPT_("Passes Not Supported");
  row.tooltip = TIP_("Render passes in the Viewport compositor are only supported in EEVEE");
  row.icon = ICON_ERROR;
  parameters.rows.append(std::move(row));
}

using namespace blender::compositor;
using namespace blender::nodes::node_composite_base_cryptomatte_cc;

class CryptoMatteOperation : public BaseCryptoMatteOperation {
 public:
  using BaseCryptoMatteOperation::BaseCryptoMatteOperation;

  Result &get_input_image() override
  {
    return get_input("Image");
  }

  /* Returns all the relevant Cryptomatte layers from the selected source. */
  Vector<Result> get_layers() override
  {
    switch (get_source()) {
      case CMP_NODE_CRYPTOMATTE_SOURCE_RENDER:
        return get_layers_from_render();
      case CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE:
        return get_layers_from_image();
    }

    BLI_assert_unreachable();
    return Vector<Result>();
  }

  /* Returns all the relevant Cryptomatte layers from the selected layer. */
  Vector<Result> get_layers_from_render()
  {
    Vector<Result> layers;

    Scene *scene = get_scene();
    if (!scene) {
      return layers;
    }

    const std::string type_name = get_type_name();

    int view_layer_index = 0;
    LISTBASE_FOREACH_INDEX (ViewLayer *, view_layer, &scene->view_layers, view_layer_index) {
      /* Find out which type of Cryptomatte layer the node uses, if non matched, then this is not
       * the view layer used by the node and we check other view layers. */
      const char *cryptomatte_type = nullptr;
      const std::string layer_prefix = std::string(view_layer->name) + ".";
      if (type_name == layer_prefix + RE_PASSNAME_CRYPTOMATTE_OBJECT) {
        cryptomatte_type = RE_PASSNAME_CRYPTOMATTE_OBJECT;
      }
      else if (type_name == layer_prefix + RE_PASSNAME_CRYPTOMATTE_ASSET) {
        cryptomatte_type = RE_PASSNAME_CRYPTOMATTE_ASSET;
      }
      else if (type_name == layer_prefix + RE_PASSNAME_CRYPTOMATTE_MATERIAL) {
        cryptomatte_type = RE_PASSNAME_CRYPTOMATTE_MATERIAL;
      }

      /* Not the view layer used by the node. */
      if (!cryptomatte_type) {
        continue;
      }

      /* Each layer stores two ranks/levels, so do ceiling division by two. */
      const int cryptomatte_layers_count = int(math::ceil(view_layer->cryptomatte_levels / 2.0f));
      for (int i = 0; i < cryptomatte_layers_count; i++) {
        const std::string pass_name = fmt::format("{}{:02}", cryptomatte_type, i);
        Result pass_result = this->context().get_pass(scene, view_layer_index, pass_name.c_str());

        /* If this Cryptomatte layer wasn't found, then all later Cryptomatte layers can't be used
         * even if they were found. */
        if (!pass_result.is_allocated()) {
          return layers;
        }
        layers.append(pass_result);
      }

      /* The target view later was processed already, no need to check other view layers. */
      return layers;
    }

    return layers;
  }

  /* Returns all the relevant Cryptomatte layers from the selected EXR image. */
  Vector<Result> get_layers_from_image()
  {
    Vector<Result> layers;

    Image *image = this->get_image();
    if (!image || image->type != IMA_TYPE_MULTILAYER) {
      return layers;
    }

    /* The render result structure of the image is populated as a side effect of the acquisition of
     * an image buffer, so acquire an image buffer and immediately release it since it is not
     * actually needed. */
    ImageUser image_user_for_layer = this->get_image_user();
    ImBuf *image_buffer = BKE_image_acquire_ibuf(image, &image_user_for_layer, nullptr);
    BKE_image_release_ibuf(image, image_buffer, nullptr);
    if (!image_buffer || !image->rr) {
      return layers;
    }

    RenderResult *render_result = BKE_image_acquire_renderresult(nullptr, image);

    /* Gather all pass names first before retrieving the images because render layers might get
     * freed when retrieving the images. */
    Vector<std::string> pass_names;

    int layer_index;
    const std::string type_name = this->get_type_name();
    LISTBASE_FOREACH_INDEX (RenderLayer *, render_layer, &render_result->layers, layer_index) {
      /* If the Cryptomatte type name doesn't start with the layer name, then it is not a
       * Cryptomatte layer. Unless it is an unnamed layer, in which case, we need to check its
       * passes. */
      const bool is_unnamed_layer = render_layer->name[0] == '\0';
      if (!is_unnamed_layer && !StringRefNull(type_name).startswith(render_layer->name)) {
        continue;
      }

      LISTBASE_FOREACH (RenderPass *, render_pass, &render_layer->passes) {
        /* If the combined pass name doesn't start with the Cryptomatte type name, then it is not a
         * Cryptomatte layer. Furthermore, if it is equal to the Cryptomatte type name with no
         * suffix, then it can be ignored, because it is a deprecated Cryptomatte preview layer
         * according to the "EXR File: Layer Naming" section of the Cryptomatte specification. */
        const std::string combined_name = this->get_combined_layer_pass_name(render_layer,
                                                                             render_pass);
        if (combined_name == type_name || !StringRef(combined_name).startswith(type_name)) {
          continue;
        }

        pass_names.append(render_pass->name);
      }

      /* If we already found Cryptomatte layers, no need to check other render layers. */
      if (!pass_names.is_empty()) {
        break;
      }
    }

    BKE_image_release_renderresult(nullptr, image, render_result);

    image_user_for_layer.layer = layer_index;
    for (const std::string &pass_name : pass_names) {
      Result pass_result = context().cache_manager().cached_images.get(
          context(), image, &image_user_for_layer, pass_name.c_str());
      layers.append(pass_result);
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
    ntreeCompositCryptomatteLayerPrefix(&bnode(), type_name, sizeof(type_name));
    return std::string(type_name);
  }

  int2 get_layers_lower_bound() override
  {
    switch (get_source()) {
      case CMP_NODE_CRYPTOMATTE_SOURCE_RENDER: {
        return this->context().get_compositing_region().min;
      }
      case CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE:
        return int2(0);
    }

    BLI_assert_unreachable();
    return int2(0);
  }

  /* The domain should be centered with the same size as the source. In case of invalid source,
   * fall back to the domain inferred from the input. */
  Domain compute_domain() override
  {
    switch (get_source()) {
      case CMP_NODE_CRYPTOMATTE_SOURCE_RENDER:
        return Domain(context().get_compositing_region_size());
      case CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE:
        return compute_image_domain();
    }

    BLI_assert_unreachable();
    return Domain::identity();
  }

  /* In case of an image source, the domain should be centered with the same size as the source
   * image. In case of an invalid image, fall back to the domain inferred from the input. */
  Domain compute_image_domain()
  {
    BLI_assert(get_source() == CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE);

    Image *image = get_image();
    if (!image) {
      return NodeOperation::compute_domain();
    }

    ImageUser image_user = this->get_image_user();
    ImBuf *image_buffer = BKE_image_acquire_ibuf(image, &image_user, nullptr);
    if (!image_buffer) {
      return NodeOperation::compute_domain();
    }

    const int2 image_size = int2(image_buffer->x, image_buffer->y);
    BKE_image_release_ibuf(image, image_buffer, nullptr);
    return Domain(image_size);
  }

  ImageUser get_image_user()
  {
    BLI_assert(this->get_source() == CMP_NODE_CRYPTOMATTE_SOURCE_IMAGE);

    Image *image = this->get_image();
    BLI_assert(image);

    /* Compute the effective frame number of the image if it was animated. */
    ImageUser image_user_for_frame = node_storage(bnode()).iuser;
    BKE_image_user_frame_calc(image, &image_user_for_frame, this->context().get_frame_number());

    return image_user_for_frame;
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

static void register_node_type_cmp_cryptomatte()
{
  namespace file_ns = blender::nodes::node_composite_cryptomatte_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeCryptomatteV2", CMP_NODE_CRYPTOMATTE);
  ntype.ui_name = "Cryptomatte";
  ntype.ui_description =
      "Generate matte for individual objects and materials using Cryptomatte render passes";
  ntype.enum_name_legacy = "CRYPTOMATTE_V2";
  ntype.nclass = NODE_CLASS_MATTE;
  ntype.declare = file_ns::cmp_node_cryptomatte_declare;
  blender::bke::node_type_size(ntype, 240, 100, 700);
  ntype.initfunc = file_ns::node_init_cryptomatte;
  ntype.initfunc_api = file_ns::node_init_api_cryptomatte;
  ntype.get_extra_info = file_ns::node_extra_info;
  ntype.updatefunc = file_ns::node_update_cryptomatte;
  blender::bke::node_type_storage(
      ntype, "NodeCryptomatte", file_ns::node_free_cryptomatte, file_ns::node_copy_cryptomatte);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_cryptomatte)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cryptomatte Legacy
 * \{ */

bNodeSocket *ntreeCompositCryptomatteAddSocket(bNodeTree *ntree, bNode *node)
{
  BLI_assert(node->type_legacy == CMP_NODE_CRYPTOMATTE_LEGACY);
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  char sockname[32];
  n->inputs_num++;
  SNPRINTF_UTF8(sockname, "Crypto %.2d", n->inputs_num - 1);
  bNodeSocket *sock = blender::bke::node_add_static_socket(
      *ntree, *node, SOCK_IN, SOCK_RGBA, PROP_NONE, "", sockname);
  return sock;
}

int ntreeCompositCryptomatteRemoveSocket(bNodeTree *ntree, bNode *node)
{
  BLI_assert(node->type_legacy == CMP_NODE_CRYPTOMATTE_LEGACY);
  NodeCryptomatte *n = static_cast<NodeCryptomatte *>(node->storage);
  if (n->inputs_num < 2) {
    return 0;
  }
  bNodeSocket *sock = static_cast<bNodeSocket *>(node->inputs.last);
  blender::bke::node_remove_socket(*ntree, *node, *sock);
  n->inputs_num--;
  return 1;
}

namespace blender::nodes::node_composite_legacy_cryptomatte_cc {

static void node_init_cryptomatte_legacy(bNodeTree *ntree, bNode *node)
{
  namespace file_ns = blender::nodes::node_composite_cryptomatte_cc;
  file_ns::node_init_cryptomatte(ntree, node);

  bke::node_add_static_socket(*ntree, *node, SOCK_IN, SOCK_RGBA, PROP_NONE, "image", "Image");

  /* Add three inputs by default, as recommended by the Cryptomatte specification. */
  ntreeCompositCryptomatteAddSocket(ntree, node);
  ntreeCompositCryptomatteAddSocket(ntree, node);
  ntreeCompositCryptomatteAddSocket(ntree, node);
}

using namespace blender::compositor;
using namespace blender::nodes::node_composite_base_cryptomatte_cc;

class LegacyCryptoMatteOperation : public BaseCryptoMatteOperation {
 public:
  using BaseCryptoMatteOperation::BaseCryptoMatteOperation;

  Result &get_input_image() override
  {
    return get_input("image");
  }

  Vector<Result> get_layers() override
  {
    Vector<Result> layers;
    /* Add all valid results of all inputs except the first input, which is the input image. */
    for (const bNodeSocket *input_socket : bnode().input_sockets().drop_front(1)) {
      if (!is_socket_available(input_socket)) {
        continue;
      }

      const Result input = get_input(input_socket->identifier);
      if (input.is_single_value()) {
        /* If this Cryptomatte layer is not valid, because it is not an image, then all later
         * Cryptomatte layers can't be used even if they were valid. */
        break;
      }
      layers.append(input);
    }
    return layers;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new LegacyCryptoMatteOperation(context, node);
}

}  // namespace blender::nodes::node_composite_legacy_cryptomatte_cc

static void register_node_type_cmp_cryptomatte_legacy()
{
  namespace legacy_file_ns = blender::nodes::node_composite_legacy_cryptomatte_cc;
  namespace file_ns = blender::nodes::node_composite_cryptomatte_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeCryptomatte", CMP_NODE_CRYPTOMATTE_LEGACY);
  ntype.ui_name = "Cryptomatte (Legacy)";
  ntype.ui_description = "Deprecated. Use Cryptomatte Node instead";
  ntype.enum_name_legacy = "CRYPTOMATTE";
  ntype.nclass = NODE_CLASS_MATTE;
  blender::bke::node_type_socket_templates(&ntype, nullptr, file_ns::cmp_node_cryptomatte_out);
  ntype.initfunc = legacy_file_ns::node_init_cryptomatte_legacy;
  blender::bke::node_type_storage(
      ntype, "NodeCryptomatte", file_ns::node_free_cryptomatte, file_ns::node_copy_cryptomatte);
  ntype.gather_link_search_ops = nullptr;
  ntype.get_compositor_operation = legacy_file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_cryptomatte_legacy)

/** \} */
