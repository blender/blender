/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_MovieClipNode.h"

#include "COM_MovieClipOperation.h"

#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "DNA_movieclip_types.h"

#include "IMB_imbuf.hh"

namespace blender::compositor {

MovieClipNode::MovieClipNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void MovieClipNode::convert_to_operations(NodeConverter &converter,
                                          const CompositorContext &context) const
{
  NodeOutput *output_movie_clip = this->get_output_socket(0);
  NodeOutput *alpha_movie_clip = this->get_output_socket(1);
  NodeOutput *offset_xmovie_clip = this->get_output_socket(2);
  NodeOutput *offset_ymovie_clip = this->get_output_socket(3);
  NodeOutput *scale_movie_clip = this->get_output_socket(4);
  NodeOutput *angle_movie_clip = this->get_output_socket(5);

  const bNode *editor_node = this->get_bnode();
  MovieClip *movie_clip = (MovieClip *)editor_node->id;
  MovieClipUser *movie_clip_user = (MovieClipUser *)editor_node->storage;
  bool cache_frame = !context.is_rendering();

  ImBuf *ibuf = nullptr;
  if (movie_clip) {
    if (cache_frame) {
      ibuf = BKE_movieclip_get_ibuf(movie_clip, movie_clip_user);
    }
    else {
      ibuf = BKE_movieclip_get_ibuf_flag(
          movie_clip, movie_clip_user, movie_clip->flag, MOVIECLIP_CACHE_SKIP);
    }
  }

  /* Always connect the output image. */
  MovieClipOperation *operation = new MovieClipOperation();
  operation->set_movie_clip(movie_clip);
  operation->set_movie_clip_user(movie_clip_user);
  operation->set_framenumber(context.get_framenumber());
  operation->set_cache_frame(cache_frame);

  converter.add_operation(operation);
  converter.map_output_socket(output_movie_clip, operation->get_output_socket());
  converter.add_preview(operation->get_output_socket());

  MovieClipAlphaOperation *alpha_operation = new MovieClipAlphaOperation();
  alpha_operation->set_movie_clip(movie_clip);
  alpha_operation->set_movie_clip_user(movie_clip_user);
  alpha_operation->set_framenumber(context.get_framenumber());
  alpha_operation->set_cache_frame(cache_frame);

  converter.add_operation(alpha_operation);
  converter.map_output_socket(alpha_movie_clip, alpha_operation->get_output_socket());

  MovieTrackingStabilization *stab = &movie_clip->tracking.stabilization;
  float loc[2], scale, angle;
  loc[0] = 0.0f;
  loc[1] = 0.0f;
  scale = 1.0f;
  angle = 0.0f;

  if (ibuf) {
    if (stab->flag & TRACKING_2D_STABILIZATION) {
      int clip_framenr = BKE_movieclip_remap_scene_to_clip_frame(movie_clip,
                                                                 context.get_framenumber());

      BKE_tracking_stabilization_data_get(
          movie_clip, clip_framenr, ibuf->x, ibuf->y, loc, &scale, &angle);
    }
  }

  converter.add_output_value(offset_xmovie_clip, loc[0]);
  converter.add_output_value(offset_ymovie_clip, loc[1]);
  converter.add_output_value(scale_movie_clip, scale);
  converter.add_output_value(angle_movie_clip, angle);

  if (ibuf) {
    IMB_freeImBuf(ibuf);
  }
}

}  // namespace blender::compositor
