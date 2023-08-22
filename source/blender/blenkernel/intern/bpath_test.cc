/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "testing/testing.h"

#include "CLG_log.h"

#include "BKE_bpath.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_movieclip_types.h"
#include "DNA_text_types.h"

#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

namespace blender::bke::tests {

#ifdef WIN32
#  define ABSOLUTE_ROOT "C:" SEP_STR
#else
#  define ABSOLUTE_ROOT SEP_STR
#endif

#define RELATIVE_ROOT "//"
#define BASE_DIR ABSOLUTE_ROOT "blendfiles" SEP_STR
#define REBASE_DIR BASE_DIR "rebase" SEP_STR

#define BLENDFILE_NAME "bpath.blend"
#define BLENDFILE_PATH BASE_DIR BLENDFILE_NAME

#define TEXT_PATH_ITEM "texts" SEP_STR "text.txt"
#define TEXT_PATH_ABSOLUTE ABSOLUTE_ROOT TEXT_PATH_ITEM
#define TEXT_PATH_ABSOLUTE_MADE_RELATIVE RELATIVE_ROOT ".." SEP_STR TEXT_PATH_ITEM
#define TEXT_PATH_RELATIVE RELATIVE_ROOT TEXT_PATH_ITEM
#define TEXT_PATH_RELATIVE_MADE_ABSOLUTE BASE_DIR TEXT_PATH_ITEM

#define MOVIECLIP_PATH_ITEM "movieclips" SEP_STR "movieclip.avi"
#define MOVIECLIP_PATH_ABSOLUTE ABSOLUTE_ROOT MOVIECLIP_PATH_ITEM
#define MOVIECLIP_PATH_ABSOLUTE_MADE_RELATIVE RELATIVE_ROOT ".." SEP_STR MOVIECLIP_PATH_ITEM
#define MOVIECLIP_PATH_RELATIVE RELATIVE_ROOT MOVIECLIP_PATH_ITEM
#define MOVIECLIP_PATH_RELATIVE_MADE_ABSOLUTE BASE_DIR MOVIECLIP_PATH_ITEM

class BPathTest : public testing::Test {
 public:
  static void SetUpTestSuite()
  {
    CLG_init();
    BKE_idtype_init();
  }
  static void TearDownTestSuite()
  {
    CLG_exit();
  }

  void SetUp() override
  {
    bmain = BKE_main_new();
    STRNCPY(bmain->filepath, BLENDFILE_PATH);

    BKE_id_new(bmain, ID_TXT, nullptr);
    BKE_id_new(bmain, ID_MC, nullptr);
  }

  void TearDown() override
  {
    BKE_main_free(bmain);
  }

  Main *bmain;
};

TEST_F(BPathTest, rebase_on_relative)
{
  /* Test on relative paths, should be modified. */
  Text *text = reinterpret_cast<Text *>(bmain->texts.first);
  text->filepath = BLI_strdup(TEXT_PATH_RELATIVE);

  MovieClip *movie_clip = reinterpret_cast<MovieClip *>(bmain->movieclips.first);
  STRNCPY(movie_clip->filepath, MOVIECLIP_PATH_RELATIVE);

  BKE_bpath_relative_rebase(bmain, BASE_DIR, REBASE_DIR, nullptr);

  EXPECT_STREQ(text->filepath, RELATIVE_ROOT ".." SEP_STR TEXT_PATH_ITEM);
  EXPECT_STREQ(movie_clip->filepath, RELATIVE_ROOT ".." SEP_STR MOVIECLIP_PATH_ITEM);
}

TEST_F(BPathTest, rebase_on_absolute)
{
  /* Test on absolute paths, should not be modified. */
  Text *text = reinterpret_cast<Text *>(bmain->texts.first);
  text->filepath = BLI_strdup(TEXT_PATH_ABSOLUTE);

  MovieClip *movie_clip = reinterpret_cast<MovieClip *>(bmain->movieclips.first);
  STRNCPY(movie_clip->filepath, MOVIECLIP_PATH_ABSOLUTE);

  BKE_bpath_relative_rebase(bmain, BASE_DIR, REBASE_DIR, nullptr);

  EXPECT_STREQ(text->filepath, TEXT_PATH_ABSOLUTE);
  EXPECT_STREQ(movie_clip->filepath, MOVIECLIP_PATH_ABSOLUTE);
}

TEST_F(BPathTest, convert_to_relative)
{
  Text *text = reinterpret_cast<Text *>(bmain->texts.first);
  text->filepath = BLI_strdup(TEXT_PATH_RELATIVE);

  MovieClip *movie_clip = reinterpret_cast<MovieClip *>(bmain->movieclips.first);
  STRNCPY(movie_clip->filepath, MOVIECLIP_PATH_ABSOLUTE);

  BKE_bpath_relative_convert(bmain, BASE_DIR, nullptr);

  /* Already relative path should not be modified. */
  EXPECT_STREQ(text->filepath, TEXT_PATH_RELATIVE);
  /* Absolute path should be modified. */
  EXPECT_STREQ(movie_clip->filepath, MOVIECLIP_PATH_ABSOLUTE_MADE_RELATIVE);
}

TEST_F(BPathTest, convert_to_absolute)
{
  Text *text = reinterpret_cast<Text *>(bmain->texts.first);
  text->filepath = BLI_strdup(TEXT_PATH_RELATIVE);

  MovieClip *movie_clip = reinterpret_cast<MovieClip *>(bmain->movieclips.first);
  STRNCPY(movie_clip->filepath, MOVIECLIP_PATH_ABSOLUTE);

  BKE_bpath_absolute_convert(bmain, BASE_DIR, nullptr);

  /* Relative path should be modified. */
  EXPECT_STREQ(text->filepath, TEXT_PATH_RELATIVE_MADE_ABSOLUTE);
  /* Already absolute path should not be modified. */
  EXPECT_STREQ(movie_clip->filepath, MOVIECLIP_PATH_ABSOLUTE);
}

TEST_F(BPathTest, list_backup_restore)
{
  Text *text = reinterpret_cast<Text *>(bmain->texts.first);
  text->filepath = BLI_strdup(TEXT_PATH_RELATIVE);

  MovieClip *movie_clip = reinterpret_cast<MovieClip *>(bmain->movieclips.first);
  STRNCPY(movie_clip->filepath, MOVIECLIP_PATH_ABSOLUTE);

  void *path_list_handle = BKE_bpath_list_backup(bmain, static_cast<eBPathForeachFlag>(0));

  ListBase *path_list = reinterpret_cast<ListBase *>(path_list_handle);
  EXPECT_EQ(BLI_listbase_count(path_list), 2);

  MEM_freeN(text->filepath);
  text->filepath = BLI_strdup(TEXT_PATH_ABSOLUTE);
  STRNCPY(movie_clip->filepath, MOVIECLIP_PATH_RELATIVE);

  BKE_bpath_list_restore(bmain, static_cast<eBPathForeachFlag>(0), path_list_handle);

  EXPECT_STREQ(text->filepath, TEXT_PATH_RELATIVE);
  EXPECT_STREQ(movie_clip->filepath, MOVIECLIP_PATH_ABSOLUTE);
  EXPECT_EQ(BLI_listbase_count(path_list), 0);

  BKE_bpath_list_free(path_list_handle);
}

}  // namespace blender::bke::tests
