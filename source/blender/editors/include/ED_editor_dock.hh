#pragma once

#include "DNA_space_enums.h"

struct bContext;
struct Scene;
struct ScrArea;
struct SpaceLink;

namespace blender::ed::editor_dock {

SpaceLink *add_docked_space(ScrArea *area, const eSpace_Type type, const Scene *scene);
void activate_docked_space(bContext *C, ScrArea *docked_area, SpaceLink *space);

}  // namespace blender::ed::editor_dock
