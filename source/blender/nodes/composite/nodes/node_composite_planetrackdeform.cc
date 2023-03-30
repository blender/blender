/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2013 Blender Foundation */

/** \file
 * \ingroup cmpnodes
 */

#include "BLT_translation.h"

#include "DNA_movieclip_types.h"
#include "DNA_tracking_types.h"

#include "BKE_context.h"
#include "BKE_lib_id.h"
#include "BKE_tracking.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_planetrackdeform_cc {

static void cmp_node_planetrackdeform_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image"));
  b.add_output<decl::Color>(N_("Image"));
  b.add_output<decl::Float>(N_("Plane"));
}

static void init(const bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;

  NodePlaneTrackDeformData *data = MEM_cnew<NodePlaneTrackDeformData>(__func__);
  data->motion_blur_samples = 16;
  data->motion_blur_shutter = 0.5f;
  node->storage = data;

  const Scene *scene = CTX_data_scene(C);
  if (scene->clip) {
    MovieClip *clip = scene->clip;
    MovieTracking *tracking = &clip->tracking;

    node->id = &clip->id;
    id_us_plus(&clip->id);

    const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
    BLI_strncpy(data->tracking_object, tracking_object->name, sizeof(data->tracking_object));

    if (tracking_object->active_plane_track) {
      BLI_strncpy(data->plane_track_name,
                  tracking_object->active_plane_track->name,
                  sizeof(data->plane_track_name));
    }
  }
}

static void node_composit_buts_planetrackdeform(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  NodePlaneTrackDeformData *data = (NodePlaneTrackDeformData *)node->storage;

  uiTemplateID(layout,
               C,
               ptr,
               "clip",
               nullptr,
               "CLIP_OT_open",
               nullptr,
               UI_TEMPLATE_ID_FILTER_ALL,
               false,
               nullptr);

  if (node->id) {
    MovieClip *clip = (MovieClip *)node->id;
    MovieTracking *tracking = &clip->tracking;
    MovieTrackingObject *tracking_object;
    uiLayout *col;
    PointerRNA tracking_ptr;

    RNA_pointer_create(&clip->id, &RNA_MovieTracking, tracking, &tracking_ptr);

    col = uiLayoutColumn(layout, false);
    uiItemPointerR(col, ptr, "tracking_object", &tracking_ptr, "objects", "", ICON_OBJECT_DATA);

    tracking_object = BKE_tracking_object_get_named(tracking, data->tracking_object);
    if (tracking_object) {
      PointerRNA object_ptr;

      RNA_pointer_create(&clip->id, &RNA_MovieTrackingObject, tracking_object, &object_ptr);

      uiItemPointerR(
          col, ptr, "plane_track_name", &object_ptr, "plane_tracks", "", ICON_ANIM_DATA);
    }
    else {
      uiItemR(layout, ptr, "plane_track_name", 0, "", ICON_ANIM_DATA);
    }
  }

  uiItemR(layout, ptr, "use_motion_blur", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  if (data->flag & CMP_NODEFLAG_PLANETRACKDEFORM_MOTION_BLUR) {
    uiItemR(layout, ptr, "motion_blur_samples", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
    uiItemR(layout, ptr, "motion_blur_shutter", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  }
}

using namespace blender::realtime_compositor;

class PlaneTrackDeformOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    get_input("Image").pass_through(get_result("Image"));
    get_result("Plane").allocate_invalid();
    context().set_info_message("Viewport compositor setup not fully supported");
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new PlaneTrackDeformOperation(context, node);
}

}  // namespace blender::nodes::node_composite_planetrackdeform_cc

void register_node_type_cmp_planetrackdeform()
{
  namespace file_ns = blender::nodes::node_composite_planetrackdeform_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_PLANETRACKDEFORM, "Plane Track Deform", NODE_CLASS_DISTORT);
  ntype.declare = file_ns::cmp_node_planetrackdeform_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_planetrackdeform;
  ntype.initfunc_api = file_ns::init;
  node_type_storage(
      &ntype, "NodePlaneTrackDeformData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  ntype.realtime_compositor_unsupported_message = N_(
      "Node not supported in the Viewport compositor");

  nodeRegisterType(&ntype);
}
