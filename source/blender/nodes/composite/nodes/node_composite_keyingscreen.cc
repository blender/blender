/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "DNA_movieclip_types.h"
#include "DNA_tracking_types.h"

#include "BLI_math_base.h"
#include "BLI_math_color.h"
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_lib_id.h"
#include "BKE_tracking.h"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** Keying Screen  ******************** */

namespace blender::nodes::node_composite_keyingscreen_cc {

static void cmp_node_keyingscreen_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Color>("Screen").translation_context(BLT_I18NCONTEXT_ID_SCREEN);
}

static void node_composit_init_keyingscreen(const bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;

  NodeKeyingScreenData *data = MEM_cnew<NodeKeyingScreenData>(__func__);
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

  uiTemplateID(layout,
               C,
               ptr,
               "clip",
               nullptr,
               nullptr,
               nullptr,
               UI_TEMPLATE_ID_FILTER_ALL,
               false,
               nullptr);

  if (node->id) {
    MovieClip *clip = (MovieClip *)node->id;
    uiLayout *col;
    PointerRNA tracking_ptr;

    RNA_pointer_create(&clip->id, &RNA_MovieTracking, &clip->tracking, &tracking_ptr);

    col = uiLayoutColumn(layout, true);
    uiItemPointerR(col, ptr, "tracking_object", &tracking_ptr, "objects", "", ICON_OBJECT_DATA);
  }
}

using namespace blender::realtime_compositor;

class KeyingScreenOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    get_result("Screen").allocate_invalid();
    context().set_info_message("Viewport compositor setup not fully supported");
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

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_KEYINGSCREEN, "Keying Screen", NODE_CLASS_MATTE);
  ntype.declare = file_ns::cmp_node_keyingscreen_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_keyingscreen;
  ntype.initfunc_api = file_ns::node_composit_init_keyingscreen;
  node_type_storage(
      &ntype, "NodeKeyingScreenData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  ntype.realtime_compositor_unsupported_message = N_(
      "Node not supported in the Viewport compositor");

  nodeRegisterType(&ntype);
}
