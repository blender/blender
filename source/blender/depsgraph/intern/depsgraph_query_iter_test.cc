/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_gtest_base.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"
#include "BKE_scene.hh"
#include "BKE_viewer_path.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "BLI_listbase.hh"
#include "BLI_memory_utils.hh"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "testing/testing.h"

namespace blender::deg::tests {

class DepsgraphObjectIteratorTest : public bke::BlenderGTestBase {};

TEST_F(DepsgraphObjectIteratorTest, ObjectOnlyViewerPath)
{
  Main *bmain = BKE_main_new();
  BLI_SCOPED_DEFER([&]() { BKE_main_free(bmain); });
  Scene *scene = BKE_scene_add(bmain, "Scene");
  ViewLayer *view_layer = static_cast<ViewLayer *>(scene->view_layers.first);
  Object *object_a = BKE_object_add(bmain, scene, view_layer, OB_EMPTY, "Object A");
  Object *object_b = BKE_object_add(bmain, scene, view_layer, OB_EMPTY, "Object B");

  Depsgraph *depsgraph = DEG_graph_new(bmain, scene, view_layer, DAG_EVAL_VIEWPORT);
  BLI_SCOPED_DEFER([&]() { DEG_graph_free(depsgraph); });
  DEG_graph_build_from_view_layer(depsgraph);
  BKE_scene_graph_update_tagged(depsgraph, bmain);

  ViewerPath viewer_path{};
  BLI_SCOPED_DEFER([&]() { BKE_viewer_path_clear(&viewer_path); });
  IDViewerPathElem *id_elem = BKE_viewer_path_elem_new_id();
  id_elem->id = &object_a->id;
  BLI_addtail(&viewer_path.path, id_elem);

  ASSERT_EQ(viewer_path.path.count(), 1);
  ASSERT_EQ(id_elem->base.next, nullptr);

  DEGObjectIterSettings settings{};
  settings.depsgraph = depsgraph;
  settings.flags = DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY | DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET;
  settings.viewer_path = &viewer_path;

  int object_count = 0;
  bool found_object_a = false;
  bool found_object_b = false;
  DEG_OBJECT_ITER_BEGIN (&settings, object) {
    const Object *object_orig = DEG_get_original(object);
    found_object_a |= object_orig == object_a;
    found_object_b |= object_orig == object_b;
    object_count++;
  }
  DEG_OBJECT_ITER_END;

  EXPECT_EQ(object_count, 2);
  EXPECT_TRUE(found_object_a);
  EXPECT_TRUE(found_object_b);
}

}  // namespace blender::deg::tests
