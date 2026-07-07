/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 * \brief iOS System File menu implementation.
 */

#import "Foundation/Foundation.h"

#include "BLI_fileops.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"

#include "BLT_translation.hh"
#include "UI_resources.hh"

#include "fsmenu.hh"

namespace blender {

struct FSMenu;

void fsmenu_read_system(struct FSMenu *fsmenu, int read_bookmarks)
{
  char line[FILE_MAXDIR];
  {
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSURL *ipad_doc_url = [[[NSFileManager defaultManager] URLsForDirectory:NSDocumentDirectory
                                                                  inDomains:NSUserDomainMask]
        firstObject];

    BLI_snprintf(line, sizeof(line), "%s", [[ipad_doc_url path] UTF8String]);
    fsmenu_insert_entry(fsmenu,
                        FS_CATEGORY_SYSTEM_BOOKMARKS,
                        line,
                        N_("Documents"),
                        ICON_DOCUMENTS,
                        FS_INSERT_LAST);
    UNUSED_VARS(read_bookmarks);
  }
}

}  // namespace blender
