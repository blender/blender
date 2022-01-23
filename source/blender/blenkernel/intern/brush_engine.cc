#if 1
#include "BKE_brush_engine.h"
#include "BKE_brush_engine.hh"

#include "DNA_sculpt_brush_types.h"

using BrushChannelSetIF = blender::brush::BrushChannelSetIF;
using BrushChannelFloat = blender::brush::BrushChannelIF<float>;

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

namespace blender {
namespace brush {

}  // namespace brush
}  // namespace blender
#endif
