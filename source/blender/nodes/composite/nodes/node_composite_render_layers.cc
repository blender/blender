/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_composite_util.hh"

#include "BLI_assert.h"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math_vector_types.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_compositor.hh"
#include "BKE_context.hh"
#include "BKE_image.hh"
#include "BKE_lib_id.hh"
#include "BKE_scene.hh"

#include "DEG_depsgraph_query.hh"

#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "GPU_shader.hh"

#include "NOD_node_extra_info.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

static blender::bke::bNodeSocketTemplate cmp_node_rlayers_out[] = {
    {SOCK_RGBA, N_("Image"), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_FLOAT, N_("Alpha"), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_FLOAT, RE_PASSNAME_DEPTH, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_VECTOR, RE_PASSNAME_NORMAL, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_VECTOR, RE_PASSNAME_UV, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_VECTOR, RE_PASSNAME_VECTOR, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_VECTOR, RE_PASSNAME_POSITION, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, RE_PASSNAME_DEPRECATED, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, RE_PASSNAME_DEPRECATED, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, RE_PASSNAME_SHADOW, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, RE_PASSNAME_AO, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, RE_PASSNAME_DEPRECATED, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, RE_PASSNAME_DEPRECATED, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, RE_PASSNAME_DEPRECATED, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_FLOAT, RE_PASSNAME_INDEXOB, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_FLOAT, RE_PASSNAME_INDEXMA, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_FLOAT, RE_PASSNAME_MIST, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, RE_PASSNAME_EMIT, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, RE_PASSNAME_ENVIRONMENT, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, RE_PASSNAME_DIFFUSE_DIRECT, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, RE_PASSNAME_DIFFUSE_INDIRECT, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, RE_PASSNAME_DIFFUSE_COLOR, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, RE_PASSNAME_GLOSSY_DIRECT, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, RE_PASSNAME_GLOSSY_INDIRECT, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, RE_PASSNAME_GLOSSY_COLOR, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, RE_PASSNAME_TRANSM_DIRECT, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, RE_PASSNAME_TRANSM_INDIRECT, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, RE_PASSNAME_TRANSM_COLOR, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, RE_PASSNAME_SUBSURFACE_DIRECT, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, RE_PASSNAME_SUBSURFACE_INDIRECT, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {SOCK_RGBA, RE_PASSNAME_SUBSURFACE_COLOR, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {-1, ""},
};
#define NUM_LEGACY_SOCKETS (ARRAY_SIZE(cmp_node_rlayers_out) - 1)

static const char *cmp_node_legacy_pass_name(const char *name)
{
  if (STREQ(name, "Diffuse Direct")) {
    return "DiffDir";
  }
  if (STREQ(name, "Diffuse Indirect")) {
    return "DiffInd";
  }
  if (STREQ(name, "Diffuse Color")) {
    return "DiffCol";
  }
  if (STREQ(name, "Glossy Direct")) {
    return "GlossDir";
  }
  if (STREQ(name, "Glossy Indirect")) {
    return "GlossInd";
  }
  if (STREQ(name, "Glossy Color")) {
    return "GlossCol";
  }
  if (STREQ(name, "Transmission Direct")) {
    return "TransDir";
  }
  if (STREQ(name, "Transmission Indirect")) {
    return "TransInd";
  }
  if (STREQ(name, "Transmission Color")) {
    return "TransCol";
  }
  if (STREQ(name, "Volume Direct")) {
    return "VolumeDir";
  }
  if (STREQ(name, "Volume Indirect")) {
    return "VolumeInd";
  }
  if (STREQ(name, "Volume Color")) {
    return "VolumeCol";
  }
  if (STREQ(name, "Ambient Occlusion")) {
    return "AO";
  }
  if (STREQ(name, "Environment")) {
    return "Env";
  }
  if (STREQ(name, "Material Index")) {
    return "IndexMA";
  }
  if (STREQ(name, "Object Index")) {
    return "IndexOB";
  }
  if (STREQ(name, "Grease Pencil")) {
    return "GreasePencil";
  }
  if (STREQ(name, "Emission")) {
    return "Emit";
  }

  return nullptr;
}

static void cmp_node_render_layers_add_pass_output(bNodeTree *ntree,
                                                   bNode *node,
                                                   const char *name,
                                                   const char *passname,
                                                   int rres_index,
                                                   eNodeSocketDatatype type,
                                                   int /*is_rlayers*/,
                                                   LinkNodePair *available_sockets,
                                                   int *prev_index)
{
  bNodeSocket *sock = (bNodeSocket *)BLI_findstring(
      &node->outputs, name, offsetof(bNodeSocket, name));

  /* Rename legacy socket names to new ones. */
  if (sock == nullptr) {
    const char *legacy_name = cmp_node_legacy_pass_name(name);
    if (legacy_name) {
      sock = (bNodeSocket *)BLI_findstring(
          &node->outputs, legacy_name, offsetof(bNodeSocket, name));
      if (sock) {
        STRNCPY(sock->name, name);
        STRNCPY(sock->identifier, name);
      }
    }
  }

  /* Replace if types don't match. */
  if (sock && sock->type != type) {
    blender::bke::node_remove_socket(*ntree, *node, *sock);
    sock = nullptr;
  }

  /* Create socket if it doesn't exist yet. */
  if (sock == nullptr) {
    if (rres_index >= 0) {
      sock = node_add_socket_from_template(
          ntree, node, &cmp_node_rlayers_out[rres_index], SOCK_OUT);
    }
    else {
      sock = blender::bke::node_add_static_socket(
          *ntree, *node, SOCK_OUT, type, PROP_NONE, name, name);
    }
    /* extra socket info */
    NodeImageLayer *sockdata = MEM_callocN<NodeImageLayer>(__func__);
    sock->storage = sockdata;
  }

  NodeImageLayer *sockdata = (NodeImageLayer *)sock->storage;
  if (sockdata) {
    STRNCPY_UTF8(sockdata->pass_name, passname);
  }

  /* Reorder sockets according to order that passes are added. */
  const int after_index = (*prev_index)++;
  bNodeSocket *after_sock = (bNodeSocket *)BLI_findlink(&node->outputs, after_index);
  BLI_remlink(&node->outputs, sock);
  BLI_insertlinkafter(&node->outputs, after_sock, sock);

  BLI_linklist_append(available_sockets, sock);
}

struct RLayerUpdateData {
  LinkNodePair *available_sockets;
  int prev_index;
};

static void node_cmp_rlayers_register_pass(bNodeTree *ntree,
                                           bNode *node,
                                           Scene *scene,
                                           ViewLayer *view_layer,
                                           const char *name,
                                           eNodeSocketDatatype type)
{
  RLayerUpdateData *data = (RLayerUpdateData *)node->storage;

  if (scene == nullptr || view_layer == nullptr || data == nullptr || node->id != (ID *)scene) {
    return;
  }

  ViewLayer *node_view_layer = (ViewLayer *)BLI_findlink(&scene->view_layers, node->custom1);
  if (node_view_layer != view_layer) {
    return;
  }

  /* Special handling for the Combined pass to ensure compatibility. */
  if (STREQ(name, RE_PASSNAME_COMBINED)) {
    cmp_node_render_layers_add_pass_output(
        ntree, node, "Image", name, -1, type, true, data->available_sockets, &data->prev_index);
    cmp_node_render_layers_add_pass_output(ntree,
                                           node,
                                           "Alpha",
                                           name,
                                           -1,
                                           SOCK_FLOAT,
                                           true,
                                           data->available_sockets,
                                           &data->prev_index);
  }
  else {
    cmp_node_render_layers_add_pass_output(
        ntree, node, name, name, -1, type, true, data->available_sockets, &data->prev_index);
  }
}

struct CreateOutputUserData {
  bNodeTree &ntree;
  bNode &node;
};

static void cmp_node_rlayer_create_outputs_cb(void *userdata,
                                              Scene *scene,
                                              ViewLayer *view_layer,
                                              const char *name,
                                              int /*channels*/,
                                              const char * /*chanid*/,
                                              eNodeSocketDatatype type)
{
  CreateOutputUserData &data = *(CreateOutputUserData *)userdata;
  node_cmp_rlayers_register_pass(&data.ntree, &data.node, scene, view_layer, name, type);
}

static void cmp_node_rlayer_create_outputs(bNodeTree *ntree,
                                           bNode *node,
                                           LinkNodePair *available_sockets)
{
  Scene *scene = (Scene *)node->id;

  if (scene) {
    RenderEngineType *engine_type = RE_engines_find(scene->r.engine);
    if (engine_type && engine_type->update_render_passes) {
      ViewLayer *view_layer = (ViewLayer *)BLI_findlink(&scene->view_layers, node->custom1);
      if (view_layer) {
        RLayerUpdateData *data = MEM_mallocN<RLayerUpdateData>("render layer update data");
        data->available_sockets = available_sockets;
        data->prev_index = -1;
        node->storage = data;

        CreateOutputUserData userdata = {*ntree, *node};

        RenderEngine *engine = RE_engine_create(engine_type);
        RE_engine_update_render_passes(
            engine, scene, view_layer, cmp_node_rlayer_create_outputs_cb, &userdata);
        RE_engine_free(engine);

        if ((scene->r.mode & R_EDGE_FRS) &&
            (view_layer->freestyle_config.flags & FREESTYLE_AS_RENDER_PASS))
        {
          node_cmp_rlayers_register_pass(
              ntree, node, scene, view_layer, RE_PASSNAME_FREESTYLE, SOCK_RGBA);
        }

        if (view_layer->grease_pencil_flags & GREASE_PENCIL_AS_SEPARATE_PASS) {
          node_cmp_rlayers_register_pass(
              ntree, node, scene, view_layer, RE_PASSNAME_GREASE_PENCIL, SOCK_RGBA);
        }

        MEM_freeN(data);
        node->storage = nullptr;

        return;
      }
    }
  }

  int prev_index = -1;
  cmp_node_render_layers_add_pass_output(ntree,
                                         node,
                                         "Image",
                                         RE_PASSNAME_COMBINED,
                                         RRES_OUT_IMAGE,
                                         SOCK_RGBA,
                                         true,
                                         available_sockets,
                                         &prev_index);
  cmp_node_render_layers_add_pass_output(ntree,
                                         node,
                                         "Alpha",
                                         RE_PASSNAME_COMBINED,
                                         RRES_OUT_ALPHA,
                                         SOCK_FLOAT,
                                         true,
                                         available_sockets,
                                         &prev_index);
}

static void cmp_node_render_layers_verify_outputs(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *sock, *sock_next;
  LinkNodePair available_sockets = {nullptr, nullptr};

  cmp_node_rlayer_create_outputs(ntree, node, &available_sockets);

  /* Get rid of sockets whose passes are not available in the image.
   * If sockets that are not available would be deleted, the connections to them would be lost
   * when e.g. opening a file (since there's no render at all yet).
   * Therefore, sockets with connected links will just be set as unavailable.
   *
   * Another important detail comes from compatibility with the older socket model, where there
   * was a fixed socket per pass type that was just hidden or not. Therefore, older versions expect
   * the first 31 passes to belong to a specific pass type.
   * So, we keep those 31 always allocated before the others as well,
   * even if they have no links attached. */
  int sock_index = 0;
  for (sock = (bNodeSocket *)node->outputs.first; sock; sock = sock_next, sock_index++) {
    sock_next = sock->next;
    if (BLI_linklist_index(available_sockets.list, sock) >= 0) {
      sock->flag &= ~SOCK_HIDDEN;
      blender::bke::node_set_socket_availability(*ntree, *sock, true);
    }
    else {
      bNodeLink *link;
      for (link = (bNodeLink *)ntree->links.first; link; link = link->next) {
        if (link->fromsock == sock) {
          break;
        }
      }
      if (!link && sock_index >= NUM_LEGACY_SOCKETS) {
        MEM_freeN(reinterpret_cast<NodeImageLayer *>(sock->storage));
        blender::bke::node_remove_socket(*ntree, *node, *sock);
      }
      else {
        blender::bke::node_set_socket_availability(*ntree, *sock, false);
      }
    }
  }

  BLI_linklist_free(available_sockets.list, nullptr);
}

void node_cmp_rlayers_outputs(bNodeTree *ntree, bNode *node)
{
  cmp_node_render_layers_verify_outputs(ntree, node);
}

const char *node_cmp_rlayers_sock_to_pass(int sock_index)
{
  if (sock_index >= NUM_LEGACY_SOCKETS) {
    return nullptr;
  }
  const char *name = cmp_node_rlayers_out[sock_index].name;
  /* Exception for alpha, which is derived from Combined. */
  return STREQ(name, "Alpha") ? RE_PASSNAME_COMBINED : name;
}

namespace blender::nodes::node_composite_render_layer_cc {

static void node_composit_init_rlayers(const bContext *C, PointerRNA *ptr)
{
  Scene *scene = CTX_data_scene(C);
  bNode *node = (bNode *)ptr->data;
  int sock_index = 0;

  node->id = &scene->id;
  id_us_plus(node->id);

  for (bNodeSocket *sock = (bNodeSocket *)node->outputs.first; sock;
       sock = sock->next, sock_index++)
  {
    NodeImageLayer *sockdata = MEM_callocN<NodeImageLayer>(__func__);
    sock->storage = sockdata;

    STRNCPY_UTF8(sockdata->pass_name, node_cmp_rlayers_sock_to_pass(sock_index));
  }
}

static void node_composit_free_rlayers(bNode *node)
{
  /* free extra socket info */
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
    if (sock->storage) {
      MEM_freeN(reinterpret_cast<NodeImageLayer *>(sock->storage));
    }
  }
}

static void node_composit_copy_rlayers(bNodeTree * /*dst_ntree*/,
                                       bNode *dest_node,
                                       const bNode *src_node)
{
  /* copy extra socket info */
  const bNodeSocket *src_output_sock = (bNodeSocket *)src_node->outputs.first;
  bNodeSocket *dest_output_sock = (bNodeSocket *)dest_node->outputs.first;
  while (dest_output_sock != nullptr) {
    dest_output_sock->storage = MEM_dupallocN(src_output_sock->storage);

    src_output_sock = src_output_sock->next;
    dest_output_sock = dest_output_sock->next;
  }
}

static void cmp_node_rlayers_update(bNodeTree *ntree, bNode *node)
{
  cmp_node_render_layers_verify_outputs(ntree, node);

  cmp_node_update_default(ntree, node);
}

static void node_composit_buts_viewlayers(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  uiLayout *col, *row;

  uiTemplateID(layout, C, ptr, "scene", nullptr, nullptr, nullptr);

  if (!node->id) {
    return;
  }

  col = &layout->column(false);
  row = &col->row(true);
  row->prop(ptr, "layer", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);

  PropertyRNA *prop = RNA_struct_find_property(ptr, "layer");
  const char *layer_name;
  if (!RNA_property_enum_identifier(C, ptr, prop, RNA_property_enum_get(ptr, prop), &layer_name)) {
    return;
  }

  PointerRNA scn_ptr;
  char scene_name[MAX_ID_NAME - 2];
  scn_ptr = RNA_pointer_get(ptr, "scene");
  RNA_string_get(&scn_ptr, "name", scene_name);

  PointerRNA op_ptr = row->op(
      "RENDER_OT_render", "", ICON_RENDER_STILL, wm::OpCallContext::InvokeDefault, UI_ITEM_NONE);
  RNA_string_set(&op_ptr, "layer", layer_name);
  RNA_string_set(&op_ptr, "scene", scene_name);
}

static void node_extra_info(NodeExtraInfoParams &parameters)
{
  SpaceNode *space_node = CTX_wm_space_node(&parameters.C);
  if (space_node->node_tree_sub_type != SNODE_COMPOSITOR_SCENE) {
    NodeExtraInfoRow row;
    row.text = RPT_("Node Unsupported");
    row.tooltip = TIP_("The Render Layers node is only supported for scene compositing");
    row.icon = ICON_ERROR;
    parameters.rows.append(std::move(row));
  }

  /* EEVEE supports passes. */
  const Scene *scene = CTX_data_scene(&parameters.C);
  if (StringRef(scene->r.engine) == RE_engine_id_BLENDER_EEVEE) {
    return;
  }

  if (!bke::compositor::is_viewport_compositor_used(parameters.C)) {
    return;
  }

  bool is_any_pass_used = false;
  for (const bNodeSocket *output : parameters.node.output_sockets()) {
    /* Combined pass is always available. */
    if (StringRef(output->name) == "Image" || StringRef(output->name) == "Alpha") {
      continue;
    }
    if (output->is_logically_linked()) {
      is_any_pass_used = true;
      break;
    }
  }

  if (!is_any_pass_used) {
    return;
  }

  NodeExtraInfoRow row;
  row.text = RPT_("Passes Not Supported");
  row.tooltip = TIP_("Render passes in the Viewport compositor are only supported in EEVEE");
  row.icon = ICON_ERROR;
  parameters.rows.append(std::move(row));
}

using namespace blender::compositor;

class RenderLayerOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Scene *scene = reinterpret_cast<const Scene *>(this->bnode().id);
    const int view_layer = this->bnode().custom1;

    Result &image_result = this->get_result("Image");
    Result &alpha_result = this->get_result("Alpha");

    if (image_result.should_compute() || alpha_result.should_compute()) {
      const Result combined_pass = this->context().get_pass(
          scene, view_layer, RE_PASSNAME_COMBINED);
      if (image_result.should_compute()) {
        this->execute_pass(combined_pass, image_result);
      }
      if (alpha_result.should_compute()) {
        this->execute_pass(combined_pass, alpha_result);
      }
    }

    for (const bNodeSocket *output : this->node()->output_sockets()) {
      if (!is_socket_available(output)) {
        continue;
      }

      if (STR_ELEM(output->identifier, "Image", "Alpha")) {
        continue;
      }

      Result &result = this->get_result(output->identifier);
      if (!result.should_compute()) {
        continue;
      }

      const char *pass_name = this->get_pass_name(output->identifier);
      this->context().populate_meta_data_for_pass(scene, view_layer, pass_name, result.meta_data);

      const Result pass = this->context().get_pass(scene, view_layer, pass_name);
      this->execute_pass(pass, result);
    }
  }

  void execute_pass(const Result &pass, Result &result)
  {
    if (!pass.is_allocated()) {
      /* Pass not rendered yet, or not supported by viewport. */
      result.allocate_invalid();
      return;
    }

    if (!this->context().is_valid_compositing_region()) {
      result.allocate_invalid();
      return;
    }

    /* Vector sockets are 3D by default, so we need to overwrite the type if the pass turned out to
     * be 4D. */
    if (result.type() == ResultType::Float3 && pass.type() == ResultType::Float4) {
      result.set_type(pass.type());
    }
    result.set_precision(pass.precision());

    if (this->context().use_gpu()) {
      this->execute_pass_gpu(pass, result);
    }
    else {
      this->execute_pass_cpu(pass, result);
    }
  }

  void execute_pass_gpu(const Result &pass, Result &result)
  {
    gpu::Shader *shader = this->context().get_shader(this->get_shader_name(pass, result),
                                                     result.precision());
    GPU_shader_bind(shader);

    /* The compositing space might be limited to a subset of the pass texture, so only read that
     * compositing region into an appropriately sized result. */
    const int2 lower_bound = this->context().get_compositing_region().min;
    GPU_shader_uniform_2iv(shader, "lower_bound", lower_bound);

    pass.bind_as_texture(shader, "input_tx");

    result.allocate_texture(Domain(this->context().get_compositing_region_size()));
    result.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, result.domain().size);

    GPU_shader_unbind();
    pass.unbind_as_texture();
    result.unbind_as_image();
  }

  const char *get_shader_name(const Result &pass, const Result &result)
  {
    /* Special case for alpha output. */
    if (pass.type() == ResultType::Color && result.type() == ResultType::Float) {
      return "compositor_read_input_alpha";
    }

    switch (pass.type()) {
      case ResultType::Float:
        return "compositor_read_input_float";
      case ResultType::Float3:
      case ResultType::Color:
      case ResultType::Float4:
        return "compositor_read_input_float4";
      case ResultType::Int:
      case ResultType::Int2:
      case ResultType::Float2:
      case ResultType::Bool:
      case ResultType::Menu:
        /* Not supported. */
        break;
      case ResultType::String:
        /* Single only types do not support GPU code path. */
        BLI_assert(Result::is_single_value_only_type(pass.type()));
        BLI_assert_unreachable();
        break;
    }

    BLI_assert_unreachable();
    return nullptr;
  }

  void execute_pass_cpu(const Result &pass, Result &result)
  {
    /* The compositing space might be limited to a subset of the pass texture, so only read that
     * compositing region into an appropriately sized result. */
    const int2 lower_bound = this->context().get_compositing_region().min;

    result.allocate_texture(Domain(this->context().get_compositing_region_size()));

    /* Special case for alpha output. */
    if (pass.type() == ResultType::Color && result.type() == ResultType::Float) {
      parallel_for(result.domain().size, [&](const int2 texel) {
        result.store_pixel(texel, pass.load_pixel<Color>(texel + lower_bound).a);
      });
    }
    else {
      parallel_for(result.domain().size, [&](const int2 texel) {
        result.store_pixel_generic_type(texel, pass.load_pixel_generic_type(texel + lower_bound));
      });
    }
  }

  /* Get the name of the pass corresponding to the output with the given identifier. */
  const char *get_pass_name(StringRef identifier)
  {
    DOutputSocket output = this->node().output_by_identifier(identifier);
    return static_cast<NodeImageLayer *>(output->storage)->pass_name;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new RenderLayerOperation(context, node);
}

static void register_node_type_cmp_rlayers()
{
  namespace file_ns = blender::nodes::node_composite_render_layer_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeRLayers", CMP_NODE_R_LAYERS);
  ntype.ui_name = "Render Layers";
  ntype.ui_description = "Input render passes from a scene render";
  ntype.enum_name_legacy = "R_LAYERS";
  ntype.nclass = NODE_CLASS_INPUT;
  blender::bke::node_type_socket_templates(&ntype, nullptr, cmp_node_rlayers_out);
  ntype.draw_buttons = file_ns::node_composit_buts_viewlayers;
  ntype.initfunc_api = file_ns::node_composit_init_rlayers;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  ntype.flag |= NODE_PREVIEW;
  blender::bke::node_type_storage(ntype,
                                  std::nullopt,
                                  file_ns::node_composit_free_rlayers,
                                  file_ns::node_composit_copy_rlayers);
  ntype.updatefunc = file_ns::cmp_node_rlayers_update;
  ntype.initfunc = node_cmp_rlayers_outputs;
  ntype.get_extra_info = file_ns::node_extra_info;
  blender::bke::node_type_size_preset(ntype, blender::bke::eNodeSizePreset::Large);

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_rlayers)

}  // namespace blender::nodes::node_composite_render_layer_cc
