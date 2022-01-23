#include "BKE_brush_engine.h"
#include "BKE_brush_engine.hh"

#include "DNA_sculpt_brush_types.h"

using BrushChannelSetIF = blender::brush::BrushChannelSetIF;
using BrushChannelFloat = blender::brush::BrushChannelIF<float>;
using BrushChannelFloat4 = blender::brush::BrushChannelIF<float[4]>;

float BKE_brush_channelset_get_final_float(BrushChannelSet *child,
                                           BrushChannelSet *parent,
                                           const char *idname,
                                           BrushMappingData *mapdata)
{
  BrushChannelSetIF chset_child(child);
  BrushChannelSetIF chset_parent(parent);

  if (!child && parent) {
    BrushChannelFloat ch = chset_parent.lookup<float>(idname);
    return ch.isValid() ? ch.evaluate(mapdata) : 0.0f;
  }
  else if (!parent) {
    BrushChannelFloat ch = chset_child.lookup<float>(idname);
    return ch.isValid() ? ch.evaluate(mapdata) : 0.0f;
  }

  BrushChannelFloat ch = chset_child.lookup<float>(idname);

  return chset_child.getFinalValue<float>(chset_parent, ch, mapdata);
}

int BKE_brush_channelset_get_final_vector(BrushChannelSet *child,
                                          BrushChannelSet *parent,
                                          const char *idname,
                                          float r_vec[4],
                                          BrushMappingData *mapdata)
{
  BrushChannelSetIF chset_child(child);
  BrushChannelSetIF chset_parent(parent);

  if (!child || !parent) {
    BrushChannelFloat4 ch;

    ch = (parent ? chset_parent : chset_child).lookup<float[4]>(idname);

    if (ch.isValid()) {
      r_vec[0] = ch.evaluate(mapdata, 0);
      r_vec[1] = ch.evaluate(mapdata, 1);
      r_vec[2] = ch.evaluate(mapdata, 2);

      if (ch.type() == BRUSH_CHANNEL_TYPE_VEC4) {
        r_vec[3] = ch.evaluate(mapdata, 3);
      }
    }
  }
  else if (!parent) {
    BrushChannelFloat4 ch = chset_child.lookup<float[4]>(idname);
    return ch.isValid() ? ch.evaluate(mapdata) : 0.0f;
  }

  BrushChannelFloat4 ch = chset_child.lookup<float[4]>(idname);

  r_vec[0] = chset_child.getFinalValue<float[4]>(chset_parent, ch, mapdata, 0);
  r_vec[1] = chset_child.getFinalValue<float[4]>(chset_parent, ch, mapdata, 1);
  r_vec[2] = chset_child.getFinalValue<float[4]>(chset_parent, ch, mapdata, 2);

  if (ch.type() == BRUSH_CHANNEL_TYPE_VEC4) {
    r_vec[3] = chset_child.getFinalValue<float[4]>(chset_parent, ch, mapdata, 3);
  }

  return 0;
}

namespace blender {
namespace brush {

}  // namespace brush
}  // namespace blender
