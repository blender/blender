/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_implicit_sharing.hh"
#include "BLI_string_ref.hh"

#define RET_OK 0
#define RET_ERROR 1

struct BlendDataReader;
struct BlendWriter;
struct ID;
struct Image;
struct Main;
struct PackedFile;
struct ReportList;
struct VFont;
struct Volume;
struct bSound;

enum ePF_FileCompare {
  PF_CMP_EQUAL = 0,
  PF_CMP_DIFFERS = 1,
  PF_CMP_NOFILE = 2,
};

enum ePF_FileStatus {
  PF_WRITE_ORIGINAL = 3,
  PF_WRITE_LOCAL = 4,
  PF_USE_LOCAL = 5,
  PF_USE_ORIGINAL = 6,
  PF_KEEP = 7,
  PF_REMOVE = 8,

  PF_ASK = 10,
};

constexpr int64_t PACKED_FILE_MAX_SIZE = INT32_MAX;

/* Pack. */

PackedFile *BKE_packedfile_duplicate(const PackedFile *pf_src);
PackedFile *BKE_packedfile_new(ReportList *reports,
                               const char *filepath_rel,
                               const char *basepath);
PackedFile *BKE_packedfile_new_from_memory(
    const void *mem, int memlen, const blender::ImplicitSharingInfo *sharing_info = nullptr);

/**
 * No libraries for now.
 */
void BKE_packedfile_pack_all(Main *bmain, ReportList *reports, bool verbose);
void BKE_packedfile_pack_all_libraries(Main *bmain, ReportList *reports);

/* Unpack. */

/**
 * #BKE_packedfile_unpack_to_file() looks at the existing files (abs_name, local_name)
 * and a packed file.
 *
 * It returns a char *to the existing file name / new file name or NULL when
 * there was an error or when the user decides to cancel the operation.
 *
 * \warning 'abs_name' may be relative still! (use a `//` prefix)
 * be sure to run #BLI_path_abs on it first.
 */
char *BKE_packedfile_unpack_to_file(ReportList *reports,
                                    const char *ref_file_name,
                                    const char *abs_name,
                                    const char *local_name,
                                    PackedFile *pf,
                                    enum ePF_FileStatus how);
char *BKE_packedfile_unpack(Main *bmain,
                            ReportList *reports,
                            ID *id,
                            const char *orig_file_path,
                            PackedFile *pf,
                            enum ePF_FileStatus how);
int BKE_packedfile_unpack_vfont(Main *bmain,
                                ReportList *reports,
                                VFont *vfont,
                                enum ePF_FileStatus how);
int BKE_packedfile_unpack_sound(Main *bmain,
                                ReportList *reports,
                                bSound *sound,
                                enum ePF_FileStatus how);
int BKE_packedfile_unpack_image(Main *bmain,
                                ReportList *reports,
                                Image *ima,
                                enum ePF_FileStatus how);
int BKE_packedfile_unpack_volume(Main *bmain,
                                 ReportList *reports,
                                 Volume *volume,
                                 enum ePF_FileStatus how);
void BKE_packedfile_unpack_all(Main *bmain, ReportList *reports, enum ePF_FileStatus how);
int BKE_packedfile_unpack_all_libraries(Main *bmain, ReportList *reports);

int BKE_packedfile_write_to_file(ReportList *reports,
                                 const char *ref_file_name,
                                 const char *filepath_rel,
                                 PackedFile *pf);

/* Free. */

void BKE_packedfile_free(PackedFile *pf);

/* Info. */

struct PackedFileCount {
  /** Counts e.g. packed images and sounds. */
  int individual_files = 0;
  /** Counts bakes that may consist of multiple files. */
  int bakes = 0;

  int total() const
  {
    return this->individual_files + this->bakes;
  }
};

PackedFileCount BKE_packedfile_count_all(Main *bmain);

/**
 * This function compares a packed file to a 'real' file.
 * It returns an integer indicating if:
 *
 * - #PF_EQUAL:     the packed file and original file are identical.
 * - #PF_DIFFERENT: the packed file and original file differ.
 * - #PF_NOFILE:    the original file doesn't exist.
 */
ePF_FileCompare BKE_packedfile_compare_to_file(const char *ref_file_name,
                                               const char *filepath_rel,
                                               const PackedFile *pf);

/* Read. */

int BKE_packedfile_seek(PackedFile *pf, int offset, int whence);
void BKE_packedfile_rewind(PackedFile *pf);
int BKE_packedfile_read(PackedFile *pf, void *data, int size);

/**
 * ID should be not NULL, return true if there's a packed file.
 */
bool BKE_packedfile_id_check(const ID *id);
/**
 * ID should be not NULL, throws error when ID is Library.
 */
void BKE_packedfile_id_unpack(Main *bmain, ID *id, ReportList *reports, enum ePF_FileStatus how);

void BKE_packedfile_blend_write(BlendWriter *writer, const PackedFile *pf);
void BKE_packedfile_blend_read(BlendDataReader *reader,
                               PackedFile **pf_p,
                               blender::StringRefNull filepath);
