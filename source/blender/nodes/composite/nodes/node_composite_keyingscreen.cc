/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string.h"

#include "DNA_defaults.h"
#include "DNA_movieclip_types.h"
#include "DNA_tracking_types.h"

#include "BKE_context.hh"
#include "BKE_lib_id.hh"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "COM_keying_screen.hh"
#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** Keying Screen  ******************** */

namespace blender::nodes::node_composite_keyingscreen_cc {

NODE_STORAGE_FUNCS(NodeKeyingScreenData)

static void cmp_node_keyingscreen_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Color>("Screen").translation_context(BLT_I18NCONTEXT_ID_SCREEN);
}

static void node_composit_init_keyingscreen(const bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;

  NodeKeyingScreenData *data = MEM_cnew<NodeKeyingScreenData>(__func__);
  data->smoothness = 0.0f;
  node->storage = data;

  const Scene *scene = CTX_data_scene(C);
  if (scene->clip) {
    MovieClip *clip = scene->clip;

    node->id = &clip->id;
    id_us_plus(&clip->id);

    const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
    STRNCPY(data->tracking_object, tracking_object->name);
  }
}

static void node_composit_buts_keyingscreen(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;

  uiTemplateID(layout, C, ptr, "clip", nullptr, nullptr, nullptr);

  if (node->id) {
    MovieClip *clip = (MovieClip *)node->id;
    uiLayout *col;
    PointerRNA tracking_ptr = RNA_pointer_create_discrete(
        &clip->id, &RNA_MovieTracking, &clip->tracking);

    col = uiLayoutColumn(layout, true);
    uiItemPointerR(col, ptr, "tracking_object", &tracking_ptr, "objects", "", ICON_OBJECT_DATA);
  }

  uiItemR(layout, ptr, "smoothness", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

using namespace blender::compositor;

class KeyingScreenOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &keying_screen = get_result("Screen");
    MovieTrackingObject *movie_tracking_object = get_movie_tracking_object();
    if (!movie_tracking_object) {
      keying_screen.allocate_invalid();
      return;
    }

    Result &cached_keying_screen = context().cache_manager().keying_screens.get(
        context(), get_movie_clip(), movie_tracking_object, get_smoothness());

    if (!cached_keying_screen.is_allocated()) {
      keying_screen.allocate_invalid();
      return;
    }

    keying_screen.wrap_external(cached_keying_screen);
  }

  Domain compute_domain() override
  {
    return Domain(get_size());
  }

  MovieTrackingObject *get_movie_tracking_object()
  {
    MovieClip *movie_clip = get_movie_clip();
    if (!movie_clip) {
      return nullptr;
    }

    MovieTracking *movie_tracking = &movie_clip->tracking;

    MovieTrackingObject *movie_tracking_object = BKE_tracking_object_get_named(
        movie_tracking, node_storage(bnode()).tracking_object);
    if (movie_tracking_object) {
      return movie_tracking_object;
    }

    return BKE_tracking_object_get_active(movie_tracking);
  }

  int2 get_size()
  {
    MovieClipUser movie_clip_user = *DNA_struct_default_get(MovieClipUser);
    const int scene_frame = context().get_frame_number();
    const int clip_frame = BKE_movieclip_remap_scene_to_clip_frame(get_movie_clip(), scene_frame);
    BKE_movieclip_user_set_frame(&movie_clip_user, clip_frame);

    int2 size;
    BKE_movieclip_get_size(get_movie_clip(), &movie_clip_user, &size.x, &size.y);
    return size;
  }

  /* The reciprocal of the smoothness is used as a shaping parameter for the radial basis function
   * used in the RBF interpolation. The exponential nature of the function can cause numerical
   * instability for low smoothness values, so we empirically choose 0.15 as a lower limit. */
  float get_smoothness()
  {
    return math::interpolate(0.15f, 1.0f, node_storage(bnode()).smoothness);
  }

  MovieClip *get_movie_clip()
  {
    return reinterpret_cast<MovieClip *>(bnode().id);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new KeyingScreenOperation(context, node);
}

}  // namespace blender::nodes::node_composite_keyingscreen_cc

void register_node_type_cmp_keyingscreen()
{
  namespace file_ns = blender::nodes::node_composite_keyingscreen_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeKeyingScreen", CMP_NODE_KEYINGSCREEN);
  ntype.ui_name = "Keying Screen";
  ntype.ui_description = "Create plates for use as a color reference for keying nodes";
  ntype.enum_name_legacy = "KEYINGSCREEN";
  ntype.nclass = NODE_CLASS_MATTE;
  ntype.declare = file_ns::cmp_node_keyingscreen_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_keyingscreen;
  ntype.initfunc_api = file_ns::node_composit_init_keyingscreen;
  blender::bke::node_type_storage(
      &ntype, "NodeKeyingScreenData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(&ntype);
}
