/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

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

/* Pack. */

struct PackedFile *BKE_packedfile_duplicate(const struct PackedFile *pf_src);
struct PackedFile *BKE_packedfile_new(struct ReportList *reports,
                                      const char *filepath_rel,
                                      const char *basepath);
struct PackedFile *BKE_packedfile_new_from_memory(void *mem, int memlen);

/**
 * No libraries for now.
 */
void BKE_packedfile_pack_all(struct Main *bmain, struct ReportList *reports, bool verbose);
void BKE_packedfile_pack_all_libraries(struct Main *bmain, struct ReportList *reports);

/* Unpack. */

/**
 * #BKE_packedfile_unpack_to_file() looks at the existing files (abs_name, local_name)
 * and a packed file.
 *
 * It returns a char *to the existing file name / new file name or NULL when
 * there was an error or when the user decides to cancel the operation.
 *
 * \warning 'abs_name' may be relative still! (use a "//" prefix)
 * be sure to run #BLI_path_abs on it first.
 */
char *BKE_packedfile_unpack_to_file(struct ReportList *reports,
                                    const char *ref_file_name,
                                    const char *abs_name,
                                    const char *local_name,
                                    struct PackedFile *pf,
                                    enum ePF_FileStatus how);
char *BKE_packedfile_unpack(struct Main *bmain,
                            struct ReportList *reports,
                            struct ID *id,
                            const char *orig_file_path,
                            struct PackedFile *pf,
                            enum ePF_FileStatus how);
int BKE_packedfile_unpack_vfont(struct Main *bmain,
                                struct ReportList *reports,
                                struct VFont *vfont,
                                enum ePF_FileStatus how);
int BKE_packedfile_unpack_sound(struct Main *bmain,
                                struct ReportList *reports,
                                struct bSound *sound,
                                enum ePF_FileStatus how);
int BKE_packedfile_unpack_image(struct Main *bmain,
                                struct ReportList *reports,
                                struct Image *ima,
                                enum ePF_FileStatus how);
int BKE_packedfile_unpack_volume(struct Main *bmain,
                                 struct ReportList *reports,
                                 struct Volume *volume,
                                 enum ePF_FileStatus how);
void BKE_packedfile_unpack_all(struct Main *bmain,
                               struct ReportList *reports,
                               enum ePF_FileStatus how);
int BKE_packedfile_unpack_all_libraries(struct Main *bmain, struct ReportList *reports);

int BKE_packedfile_write_to_file(struct ReportList *reports,
                                 const char *ref_file_name,
                                 const char *filepath,
                                 struct PackedFile *pf);

/* Free. */

void BKE_packedfile_free(struct PackedFile *pf);

/* Info. */

int BKE_packedfile_count_all(struct Main *bmain);
/**
 * This function compares a packed file to a 'real' file.
 * It returns an integer indicating if:
 *
 * - #PF_EQUAL:     the packed file and original file are identical.
 * - #PF_DIFFERENT: the packed file and original file differ.
 * - #PF_NOFILE:    the original file doesn't exist.
 */
enum ePF_FileCompare BKE_packedfile_compare_to_file(const char *ref_file_name,
                                                    const char *filepath_rel,
                                                    const struct PackedFile *pf);

/* Read. */

int BKE_packedfile_seek(struct PackedFile *pf, int offset, int whence);
void BKE_packedfile_rewind(struct PackedFile *pf);
int BKE_packedfile_read(struct PackedFile *pf, void *data, int size);

/**
 * ID should be not NULL, return true if there's a packed file.
 */
bool BKE_packedfile_id_check(const struct ID *id);
/**
 * ID should be not NULL, throws error when ID is Library.
 */
void BKE_packedfile_id_unpack(struct Main *bmain,
                              struct ID *id,
                              struct ReportList *reports,
                              enum ePF_FileStatus how);

void BKE_packedfile_blend_write(struct BlendWriter *writer, const struct PackedFile *pf);
void BKE_packedfile_blend_read(struct BlendDataReader *reader, struct PackedFile **pf_p);

#ifdef __cplusplus
}
#endif
