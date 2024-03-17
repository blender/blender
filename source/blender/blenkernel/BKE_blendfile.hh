/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

struct bContext;
struct BlendFileData;
struct BlendFileReadParams;
struct BlendFileReadReport;
struct BlendFileReadWMSetupData;
struct ID;
struct Main;
struct MemFile;
struct ReportList;
struct UserDef;
struct WorkspaceConfigFileData;

/**
 * Check whether given path ends with a blend file compatible extension
 * (`.blend`, `.ble` or `.blend.gz`).
 *
 * \param str: The path to check.
 * \return true is this path ends with a blender file extension.
 */
bool BKE_blendfile_extension_check(const char *str);
/**
 * Try to explode given path into its 'library components'
 * (i.e. a .blend file, id type/group, and data-block itself).
 *
 * \param path: the full path to explode.
 * \param r_dir: the string that'll contain path up to blend file itself ('library' path).
 * WARNING! Must be at least #FILE_MAX_LIBEXTRA long (it also stores group and name strings)!
 * \param r_group: a pointer within `r_dir` to the 'group' part of the path, if any ('\0'
 * terminated). May be NULL.
 * \param r_name: a pointer within `r_dir` to the data-block name, if any ('\0' terminated). May be
 * NULL.
 * \return true if path contains a blend file.
 */
bool BKE_blendfile_library_path_explode(const char *path,
                                        char *r_dir,
                                        char **r_group,
                                        char **r_name);

/**
 * Check whether a given path is actually a Blender-readable, valid .blend file.
 *
 * \note Currently does attempt to open and read (part of) the given file.
 */
bool BKE_blendfile_is_readable(const char *path, ReportList *reports);

/**
 * Shared setup function that makes the data from `bfd` into the current blend file,
 * replacing the contents of #G.main.
 * This uses the bfd returned by #BKE_blendfile_read and similarly named functions.
 *
 * This is done in a separate step so the caller may perform actions after it is known the file
 * loaded correctly but before the file replaces the existing blend file contents.
 */
void BKE_blendfile_read_setup_readfile(bContext *C,
                                       BlendFileData *bfd,
                                       const BlendFileReadParams *params,
                                       BlendFileReadWMSetupData *wm_setup_data,
                                       BlendFileReadReport *reports,
                                       bool startup_update_defaults,
                                       const char *startup_app_template);

/**
 * Simpler version of #BKE_blendfile_read_setup_readfile used when reading undo steps from
 * memfile. */
void BKE_blendfile_read_setup_undo(bContext *C,
                                   BlendFileData *bfd,
                                   const BlendFileReadParams *params,
                                   BlendFileReadReport *reports);

/**
 * \return Blend file data, this must be passed to
 * #BKE_blendfile_read_setup_readfile/#BKE_blendfile_read_setup_undo when non-NULL.
 */
BlendFileData *BKE_blendfile_read(const char *filepath,
                                  const BlendFileReadParams *params,
                                  BlendFileReadReport *reports);

/**
 * \return Blend file data, this must be passed to
 * #BKE_blendfile_read_setup_readfile/#BKE_blendfile_read_setup_undo when non-NULL.
 */
BlendFileData *BKE_blendfile_read_from_memory(const void *file_buf,
                                              int file_buf_size,
                                              const BlendFileReadParams *params,
                                              ReportList *reports);

/**
 * \return Blend file data, this must be passed to
 * #BKE_blendfile_read_setup_readfile/#BKE_blendfile_read_setup_undo when non-NULL.
 *
 * \note `memfile` is the undo buffer.
 */
BlendFileData *BKE_blendfile_read_from_memfile(Main *bmain,
                                               MemFile *memfile,
                                               const BlendFileReadParams *params,
                                               ReportList *reports);
/**
 * Utility to make a file 'empty' used for startup to optionally give an empty file.
 * Handy for tests.
 */
void BKE_blendfile_read_make_empty(bContext *C);

/**
 * Only read the #UserDef from a .blend.
 */
UserDef *BKE_blendfile_userdef_read(const char *filepath, ReportList *reports);
UserDef *BKE_blendfile_userdef_read_from_memory(const void *file_buf,
                                                int file_buf_size,
                                                ReportList *reports);
UserDef *BKE_blendfile_userdef_from_defaults();

/**
 * Only write the #UserDef in a `.blend`.
 * \return success.
 */
bool BKE_blendfile_userdef_write(const char *filepath, ReportList *reports);
/**
 * Only write the #UserDef in a `.blend`, merging with the existing blend file.
 * \return success.
 *
 * \note In the future we should re-evaluate user preferences,
 * possibly splitting out system/hardware specific preferences.
 */
bool BKE_blendfile_userdef_write_app_template(const char *filepath, ReportList *reports);

bool BKE_blendfile_userdef_write_all(ReportList *reports);

WorkspaceConfigFileData *BKE_blendfile_workspace_config_read(const char *filepath,
                                                             const void *file_buf,
                                                             int file_buf_size,
                                                             ReportList *reports);
bool BKE_blendfile_workspace_config_write(Main *bmain, const char *filepath, ReportList *reports);
void BKE_blendfile_workspace_config_data_free(WorkspaceConfigFileData *workspace_config);

/* Partial blend file writing. */

void BKE_blendfile_write_partial_tag_ID(ID *id, bool set);
void BKE_blendfile_write_partial_begin(Main *bmain_src);
/**
 * \param remap_mode: Choose the kind of path remapping or none #eBLO_WritePathRemap.
 * \return Success.
 */
bool BKE_blendfile_write_partial(
    Main *bmain_src, const char *filepath, int write_flags, int remap_mode, ReportList *reports);
void BKE_blendfile_write_partial_end(Main *bmain_src);
