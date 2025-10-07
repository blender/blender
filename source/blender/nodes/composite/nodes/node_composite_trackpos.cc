/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_index_range.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string_utf8.h"

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
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_trackpos_cc {

NODE_STORAGE_FUNCS(NodeTrackPosData)

static const EnumPropertyItem mode_items[] = {
    {CMP_NODE_TRACK_POSITION_ABSOLUTE,
     "ABSOLUTE",
     0,
     N_("Absolute"),
     N_("Returns the position and speed of the marker at the current scene frame relative to the "
        "zero origin of the tracking space")},
    {CMP_NODE_TRACK_POSITION_RELATIVE_START,
     "RELATIVE_START",
     0,
     N_("Relative Start"),
     N_("Returns the position and speed of the marker at the current scene frame relative to the "
        "position of the first non-disabled marker in the track")},
    {CMP_NODE_TRACK_POSITION_RELATIVE_FRAME,
     "RELATIVE_FRAME",
     0,
     N_("Relative Frame"),
     N_("Returns the position and speed of the marker at the current scene frame relative to the "
        "position of the marker at the current scene frame plus the user given relative frame")},
    {CMP_NODE_TRACK_POSITION_ABSOLUTE_FRAME,
     "ABSOLUTE_FRAME",
     0,
     N_("Absolute Frame"),
     N_("Returns the position and speed of the marker at the given absolute frame")},
    {0, nullptr, 0, nullptr, nullptr},
};

static void cmp_node_trackpos_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Menu>("Mode")
      .default_value(CMP_NODE_TRACK_POSITION_ABSOLUTE)
      .static_items(mode_items)
      .optional_label();
  b.add_input<decl::Int>("Frame").usage_by_menu(
      "Mode", {CMP_NODE_TRACK_POSITION_RELATIVE_FRAME, CMP_NODE_TRACK_POSITION_ABSOLUTE_FRAME});

  b.add_output<decl::Float>("X");
  b.add_output<decl::Float>("Y");
  b.add_output<decl::Vector>("Speed").subtype(PROP_VELOCITY).dimensions(4);
}

static void init(const bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;

  NodeTrackPosData *data = MEM_callocN<NodeTrackPosData>(__func__);
  node->storage = data;

  const Scene *scene = CTX_data_scene(C);
  if (scene->clip) {
    MovieClip *clip = scene->clip;
    MovieTracking *tracking = &clip->tracking;

    node->id = &clip->id;
    id_us_plus(&clip->id);

    const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
    STRNCPY_UTF8(data->tracking_object, tracking_object->name);

    if (tracking_object->active_track) {
      STRNCPY_UTF8(data->track_name, tracking_object->active_track->name);
    }
  }
}

static void node_composit_buts_trackpos(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;

  uiTemplateID(layout, C, ptr, "clip", nullptr, "CLIP_OT_open", nullptr);

  if (node->id) {
    MovieClip *clip = (MovieClip *)node->id;
    MovieTracking *tracking = &clip->tracking;
    MovieTrackingObject *tracking_object;
    uiLayout *col;
    NodeTrackPosData *data = (NodeTrackPosData *)node->storage;
    PointerRNA tracking_ptr = RNA_pointer_create_discrete(&clip->id, &RNA_MovieTracking, tracking);

    col = &layout->column(false);
    col->prop_search(ptr, "tracking_object", &tracking_ptr, "objects", "", ICON_OBJECT_DATA);

    tracking_object = BKE_tracking_object_get_named(tracking, data->tracking_object);
    if (tracking_object) {
      PointerRNA object_ptr = RNA_pointer_create_discrete(
          &clip->id, &RNA_MovieTrackingObject, tracking_object);

      col->prop_search(ptr, "track_name", &object_ptr, "tracks", "", ICON_ANIM_DATA);
    }
    else {
      layout->prop(ptr, "track_name", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_ANIM_DATA);
    }
  }
}

using namespace blender::compositor;

class TrackPositionOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    MovieTrackingTrack *track = get_movie_tracking_track();

    if (!track) {
      execute_invalid();
      return;
    }

    const float2 current_marker_position = compute_marker_position_at_frame(track, get_frame());
    const int2 size = get_size();

    execute_position(track, current_marker_position, size);
    execute_speed(track, current_marker_position, size);
  }

  void execute_position(MovieTrackingTrack *track, float2 current_marker_position, int2 size)
  {
    const bool should_compute_x = should_compute_output("X");
    const bool should_compute_y = should_compute_output("Y");
    if (!should_compute_x && !should_compute_y) {
      return;
    }

    /* Compute the position relative to the reference marker position. Multiply by the size to get
     * the position in pixel space. */
    const float2 reference_marker_position = compute_reference_marker_position(track);
    const float2 position = (current_marker_position - reference_marker_position) * float2(size);

    if (should_compute_x) {
      Result &result = get_result("X");
      result.allocate_single_value();
      result.set_single_value(position.x);
    }

    if (should_compute_y) {
      Result &result = get_result("Y");
      result.allocate_single_value();
      result.set_single_value(position.y);
    }
  }

  void execute_speed(MovieTrackingTrack *track, float2 current_marker_position, int2 size)
  {
    if (!should_compute_output("Speed")) {
      return;
    }

    /* Compute the speed as the difference between the previous marker position and the current
     * marker position. Notice that we compute the speed from the current to the previous position,
     * not the other way around. */
    const float2 previous_marker_position = compute_temporally_neighboring_marker_position(
        track, current_marker_position, -1);
    const float2 speed_toward_previous = previous_marker_position - current_marker_position;

    /* Compute the speed as the difference between the current marker position and the next marker
     * position. */
    const float2 next_marker_position = compute_temporally_neighboring_marker_position(
        track, current_marker_position, 1);
    const float2 speed_toward_next = current_marker_position - next_marker_position;

    /* Encode both speeds in a 4D vector. Multiply by the size to get the speed in pixel space. */
    const float4 speed = float4(speed_toward_previous * float2(size),
                                speed_toward_next * float2(size));

    Result &result = get_result("Speed");
    result.allocate_single_value();
    result.set_single_value(speed);
  }

  void execute_invalid()
  {
    if (should_compute_output("X")) {
      Result &result = get_result("X");
      result.allocate_single_value();
      result.set_single_value(0.0f);
    }
    if (should_compute_output("Y")) {
      Result &result = get_result("Y");
      result.allocate_single_value();
      result.set_single_value(0.0f);
    }
    if (should_compute_output("Speed")) {
      Result &result = get_result("Speed");
      result.allocate_single_value();
      result.set_single_value(float4(0.0f));
    }
  }

  /* Compute the position of the marker that is delta time away from the evaluation frame. If no
   * marker exist for that particular frame or is disabled, the current marker position is
   * returned. This is useful for computing the speed by providing small negative and positive
   * delta times. */
  float2 compute_temporally_neighboring_marker_position(MovieTrackingTrack *track,
                                                        float2 current_marker_position,
                                                        int time_delta)
  {
    const int local_frame_number = BKE_movieclip_remap_scene_to_clip_frame(
        get_movie_clip(), get_frame() + time_delta);
    MovieTrackingMarker *marker = BKE_tracking_marker_get_exact(track, local_frame_number);

    if (marker == nullptr || marker->flag & MARKER_DISABLED) {
      return current_marker_position;
    }

    return float2(marker->pos);
  }

  /* Compute the position of the reference marker which the output position will be computed
   * relative to. For non-relative modes, this is just the zero origin or the tracking space. See
   * the get_mode() method for more information. */
  float2 compute_reference_marker_position(MovieTrackingTrack *track)
  {
    switch (get_mode()) {
      case CMP_NODE_TRACK_POSITION_RELATIVE_START:
        return compute_first_marker_position(track);
      case CMP_NODE_TRACK_POSITION_RELATIVE_FRAME:
        return compute_marker_position_at_frame(track, get_relative_frame());
      case CMP_NODE_TRACK_POSITION_ABSOLUTE:
      case CMP_NODE_TRACK_POSITION_ABSOLUTE_FRAME:
        return float2(0.0f);
    }

    return float2(0.0f);
  }

  /* Compute the position of the first non-disabled marker in the track. */
  float2 compute_first_marker_position(MovieTrackingTrack *track)
  {
    for (const int i : IndexRange(track->markersnr)) {
      MovieTrackingMarker &marker = track->markers[i];

      if (marker.flag & MARKER_DISABLED) {
        continue;
      }

      return float2(marker.pos);
    }

    return float2(0.0f);
  }

  /* Compute the marker position at the given frame, if no such marker exist, return the position
   * of the temporally nearest marker before it, if no such marker exist, return the position of
   * the temporally nearest marker after it. */
  float2 compute_marker_position_at_frame(MovieTrackingTrack *track, int frame)
  {
    const int local_frame_number = BKE_movieclip_remap_scene_to_clip_frame(get_movie_clip(),
                                                                           frame);
    MovieTrackingMarker *marker = BKE_tracking_marker_get(track, local_frame_number);
    return float2(marker->pos);
  }

  /* Get the movie tracking track corresponding to the given object and track names. If no such
   * track exist, return nullptr. */
  MovieTrackingTrack *get_movie_tracking_track()
  {
    MovieClip *movie_clip = get_movie_clip();
    if (!movie_clip) {
      return nullptr;
    }

    MovieTracking *movie_tracking = &movie_clip->tracking;

    MovieTrackingObject *movie_tracking_object = BKE_tracking_object_get_named(
        movie_tracking, node_storage(bnode()).tracking_object);
    if (!movie_tracking_object) {
      return nullptr;
    }

    return BKE_tracking_object_find_track_with_name(movie_tracking_object,
                                                    node_storage(bnode()).track_name);
  }

  /* Get the size of the movie clip at the evaluation frame. This is constant for all frames in
   * most cases. */
  int2 get_size()
  {
    MovieClipUser user = *DNA_struct_default_get(MovieClipUser);
    BKE_movieclip_user_set_frame(&user, get_frame());

    int2 size;
    BKE_movieclip_get_size(get_movie_clip(), &user, &size.x, &size.y);

    return size;
  }

  /* In the CMP_NODE_TRACK_POSITION_RELATIVE_FRAME mode, this represents the offset that will be
   * added to the current scene frame. See the get_mode() method for more information. */
  int get_relative_frame()
  {
    return this->get_input("Frame").get_single_value_default(0);
  }

  /* Get the frame where the marker will be retrieved. This is the absolute frame for the absolute
   * mode and the current scene frame otherwise. */
  int get_frame()
  {
    if (get_mode() == CMP_NODE_TRACK_POSITION_ABSOLUTE_FRAME) {
      return get_absolute_frame();
    }

    return context().get_frame_number();
  }

  /* In the CMP_NODE_TRACK_POSITION_ABSOLUTE_FRAME mode, this represents the frame where the marker
   * will be retrieved. See the get_mode() method for more information. */
  int get_absolute_frame()
  {
    return this->get_input("Frame").get_single_value_default(0);
  }

  CMPNodeTrackPositionMode get_mode()
  {
    const Result &input = this->get_input("Mode");
    const MenuValue default_menu_value = MenuValue(CMP_NODE_TRACK_POSITION_ABSOLUTE);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    return static_cast<CMPNodeTrackPositionMode>(menu_value.value);
  }

  MovieClip *get_movie_clip()
  {
    return (MovieClip *)bnode().id;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new TrackPositionOperation(context, node);
}

}  // namespace blender::nodes::node_composite_trackpos_cc

static void register_node_type_cmp_trackpos()
{
  namespace file_ns = blender::nodes::node_composite_trackpos_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeTrackPos", CMP_NODE_TRACKPOS);
  ntype.ui_name = "Track Position";
  ntype.ui_description =
      "Provide information about motion tracking points, such as x and y values";
  ntype.enum_name_legacy = "TRACKPOS";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::cmp_node_trackpos_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_trackpos;
  ntype.initfunc_api = file_ns::init;
  blender::bke::node_type_storage(
      ntype, "NodeTrackPosData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_trackpos)
