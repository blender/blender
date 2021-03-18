/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct BlendFileData;
struct BlendFileReadParams;
struct ID;
struct Main;
struct MemFile;
struct ReportList;
struct UserDef;
struct bContext;

void BKE_blendfile_read_setup_ex(struct bContext *C,
                                 struct BlendFileData *bfd,
                                 const struct BlendFileReadParams *params,
                                 struct ReportList *reports,
                                 /* Extra args. */
                                 const bool startup_update_defaults,
                                 const char *startup_app_template);

void BKE_blendfile_read_setup(struct bContext *C,
                              struct BlendFileData *bfd,
                              const struct BlendFileReadParams *params,
                              struct ReportList *reports);

struct BlendFileData *BKE_blendfile_read(const char *filepath,
                                         const struct BlendFileReadParams *params,
                                         struct ReportList *reports);

struct BlendFileData *BKE_blendfile_read_from_memory(const void *filebuf,
                                                     int filelength,
                                                     const struct BlendFileReadParams *params,
                                                     struct ReportList *reports);

struct BlendFileData *BKE_blendfile_read_from_memfile(struct Main *bmain,
                                                      struct MemFile *memfile,
                                                      const struct BlendFileReadParams *params,
                                                      struct ReportList *reports);
void BKE_blendfile_read_make_empty(struct bContext *C);

struct UserDef *BKE_blendfile_userdef_read(const char *filepath, struct ReportList *reports);
struct UserDef *BKE_blendfile_userdef_read_from_memory(const void *filebuf,
                                                       int filelength,
                                                       struct ReportList *reports);
struct UserDef *BKE_blendfile_userdef_from_defaults(void);

bool BKE_blendfile_userdef_write(const char *filepath, struct ReportList *reports);
bool BKE_blendfile_userdef_write_app_template(const char *filepath, struct ReportList *reports);

bool BKE_blendfile_userdef_write_all(struct ReportList *reports);

struct WorkspaceConfigFileData *BKE_blendfile_workspace_config_read(const char *filepath,
                                                                    const void *filebuf,
                                                                    int filelength,
                                                                    struct ReportList *reports);
bool BKE_blendfile_workspace_config_write(struct Main *bmain,
                                          const char *filepath,
                                          struct ReportList *reports);
void BKE_blendfile_workspace_config_data_free(struct WorkspaceConfigFileData *workspace_config);

/* partial blend file writing */
void BKE_blendfile_write_partial_tag_ID(struct ID *id, bool set);
void BKE_blendfile_write_partial_begin(struct Main *bmain_src);
bool BKE_blendfile_write_partial(struct Main *bmain_src,
                                 const char *filepath,
                                 const int write_flags,
                                 const int remap_mode,
                                 struct ReportList *reports);
void BKE_blendfile_write_partial_end(struct Main *bmain_src);

#ifdef __cplusplus
}
#endif
