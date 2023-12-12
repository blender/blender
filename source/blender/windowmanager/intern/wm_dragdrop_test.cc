/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

/* #eFileSel_File_Types. */
#include "DNA_space_types.h"

#include "WM_api.hh"
#include "WM_types.hh"

namespace blender::tests {

TEST(wm_drag, wmDragPath)
{
  {
    /**
     * NOTE: `WM_drag_create_path_data` gets the `file_type` from the first path in `paths` and
     * only needs its extension, so there is no need to describe a full path here that can have a
     * different format on Windows or Linux. However callers must ensure that they are valid paths.
     */
    blender::Vector<const char *> paths{"text_file.txt"};
    wmDragPath *path_data = WM_drag_create_path_data(paths);
    blender::Vector<std::string> expected_file_paths{"text_file.txt"};

    EXPECT_EQ(path_data->paths.size(), 1);
    EXPECT_EQ(path_data->tooltip, "text_file.txt");
    EXPECT_EQ(path_data->paths, expected_file_paths);

    /** Test `wmDrag` path data getters. */
    wmDrag drag;
    drag.type = WM_DRAG_PATH;
    drag.poin = path_data;
    EXPECT_STREQ(WM_drag_get_single_path(&drag), "text_file.txt");
    EXPECT_EQ(WM_drag_get_path_file_type(&drag), FILE_TYPE_TEXT);
    EXPECT_EQ(WM_drag_get_paths(&drag), expected_file_paths.as_span());
    EXPECT_STREQ(WM_drag_get_single_path(&drag, FILE_TYPE_TEXT), "text_file.txt");
    EXPECT_EQ(WM_drag_get_single_path(&drag, FILE_TYPE_BLENDER), nullptr);
    EXPECT_TRUE(
        WM_drag_has_path_file_type(&drag, FILE_TYPE_BLENDER | FILE_TYPE_TEXT | FILE_TYPE_IMAGE));
    EXPECT_FALSE(WM_drag_has_path_file_type(&drag, FILE_TYPE_BLENDER | FILE_TYPE_IMAGE));
    MEM_delete(path_data);
  }
  {
    blender::Vector<const char *> paths = {"blender.blend", "text_file.txt", "image.png"};
    wmDragPath *path_data = WM_drag_create_path_data(paths);
    blender::Vector<std::string> expected_file_paths = {
        "blender.blend", "text_file.txt", "image.png"};

    EXPECT_EQ(path_data->paths.size(), 3);
    EXPECT_EQ(path_data->tooltip, "Dragging 3 files");
    EXPECT_EQ(path_data->paths, expected_file_paths);

    /** Test `wmDrag` path data getters. */
    wmDrag drag;
    drag.type = WM_DRAG_PATH;
    drag.poin = path_data;
    EXPECT_STREQ(WM_drag_get_single_path(&drag), "blender.blend");
    EXPECT_EQ(WM_drag_get_path_file_type(&drag), FILE_TYPE_BLENDER);
    EXPECT_EQ(WM_drag_get_paths(&drag), expected_file_paths.as_span());
    EXPECT_STREQ(WM_drag_get_single_path(&drag, FILE_TYPE_BLENDER), "blender.blend");
    EXPECT_STREQ(WM_drag_get_single_path(&drag, FILE_TYPE_IMAGE), "image.png");
    EXPECT_STREQ(WM_drag_get_single_path(&drag, FILE_TYPE_TEXT), "text_file.txt");
    EXPECT_STREQ(
        WM_drag_get_single_path(&drag, FILE_TYPE_BLENDER | FILE_TYPE_TEXT | FILE_TYPE_IMAGE),
        "blender.blend");
    EXPECT_STREQ(WM_drag_get_single_path(&drag, FILE_TYPE_TEXT | FILE_TYPE_IMAGE),
                 "text_file.txt");
    EXPECT_EQ(WM_drag_get_single_path(&drag, FILE_TYPE_ASSET), nullptr);
    EXPECT_TRUE(
        WM_drag_has_path_file_type(&drag, FILE_TYPE_BLENDER | FILE_TYPE_TEXT | FILE_TYPE_IMAGE));
    EXPECT_TRUE(WM_drag_has_path_file_type(&drag, FILE_TYPE_BLENDER | FILE_TYPE_IMAGE));
    EXPECT_TRUE(WM_drag_has_path_file_type(&drag, FILE_TYPE_IMAGE));
    EXPECT_FALSE(WM_drag_has_path_file_type(&drag, FILE_TYPE_ASSET));
    MEM_delete(path_data);
  }
  {
    /** Test `wmDrag` path data getters when the drag type is different to `WM_DRAG_PATH`. */
    wmDrag drag;
    drag.type = WM_DRAG_COLOR;
    EXPECT_EQ(WM_drag_get_single_path(&drag), nullptr);
    EXPECT_EQ(WM_drag_get_path_file_type(&drag), 0);
    EXPECT_EQ(WM_drag_get_paths(&drag).size(), 0);
    EXPECT_EQ(WM_drag_get_single_path(
                  &drag, FILE_TYPE_BLENDER | FILE_TYPE_IMAGE | FILE_TYPE_TEXT | FILE_TYPE_ASSET),
              nullptr);
    EXPECT_FALSE(WM_drag_has_path_file_type(
        &drag, FILE_TYPE_BLENDER | FILE_TYPE_IMAGE | FILE_TYPE_TEXT | FILE_TYPE_ASSET));
  }
}

}  // namespace blender::tests
