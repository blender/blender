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
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

#ifndef __BKE_IDTYPE_H__
#define __BKE_IDTYPE_H__

/** \file
 * \ingroup bke
 *
 * ID type structure, helping to factorize common operations and data for all data-block types.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct ID;
struct Main;

/** IDTypeInfo.flags. */
enum {
  /** Indicates that the given IDType does not support copying. */
  IDTYPE_FLAGS_NO_COPY = 1 << 0,
  /** Indicates that the given IDType does not support linking/appending from a library file. */
  IDTYPE_FLAGS_NO_LIBLINKING = 1 << 1,
  /** Indicates that the given IDType does not support making a library-linked ID local. */
  IDTYPE_FLAGS_NO_MAKELOCAL = 1 << 2,
};

/* ********** Prototypes for IDTypeInfo callbacks. ********** */

typedef void (*IDTypeInitDataFunction)(struct ID *id);

/** \param flag: Copying options (see BKE_lib_id.h's LIB_ID_COPY_... flags for more). */
typedef void (*IDTypeCopyDataFunction)(struct Main *bmain,
                                       struct ID *id_dst,
                                       const struct ID *id_src,
                                       const int flag);

typedef void (*IDTypeFreeDataFunction)(struct ID *id);

/** \param flag: See BKE_lib_id.h's LIB_ID_MAKELOCAL_... flags. */
typedef void (*IDTypeMakeLocalFunction)(struct Main *bmain, struct ID *id, const int flags);

typedef struct IDTypeInfo {
  /* ********** General IDType data. ********** */

  /**
   * Unique identifier of this type, either as a short or an array of two chars, see DNA_ID.h's
   * ID_XX enums.
   */
  short id_code;
  /**
   * Bitflag matching id_code, used for filtering (e.g. in file browser), see DNA_ID.h's
   * FILTER_ID_XX enums.
   */
  int id_filter;

  /**
   * Define the position of this data-block type in the virtual list of all data in a Main that is
   * returned by `set_listbasepointers()`.
   * Very important, this has to be unique and below INDEX_ID_MAX, see DNA_ID.h.
   */
  short main_listbase_index;

  /** Memory size of a data-block of that type. */
  size_t struct_size;

  /** The user visible name for this data-block, also used as default name for a new data-block. */
  const char *name;
  /** Plural version of the user-visble name. */
  const char *name_plural;
  /** Translation context to use for UI messages related to that type of data-block. */
  const char *translation_context;

  /** Generic info flags about that data-block type. */
  int flags;

  /* ********** ID management callbacks ********** */

  /* TODO: Note about callbacks: Ideally we could also handle here `BKE_lib_query`'s behavior, as
   * well as read/write of files. However, this is a bit more involved than basic ID management
   * callbacks, so we'll check on this later. */

  /**
   * Initialize a new, empty calloc'ed data-block. May be NULL if there is nothing to do.
   */
  IDTypeInitDataFunction init_data;

  /**
   * Copy the given data-block's data from source to destination. May be NULL if mere memcopy of
   * the ID struct itself is enough.
   */
  IDTypeCopyDataFunction copy_data;

  /**
   * Free the data of the data-block (NOT the ID itself). May be NULL if there is nothing to do.
   */
  IDTypeFreeDataFunction free_data;

  /**
   * Make a linked data-block local. May be NULL if default behavior from
   * `BKE_lib_id_make_local_generic()` is enough.
   */
  IDTypeMakeLocalFunction make_local;
} IDTypeInfo;

/* ********** Declaration of each IDTypeInfo. ********** */

/* Those are defined in the respective BKE files. */
extern IDTypeInfo IDType_ID_SCE;
extern IDTypeInfo IDType_ID_LI;
extern IDTypeInfo IDType_ID_OB;

/* ********** Helpers/Utils API. ********** */

/* Module initialization. */
void BKE_idtype_init(void);

/* General helpers. */
const struct IDTypeInfo *BKE_idtype_get_info_from_idcode(const short id_code);
const struct IDTypeInfo *BKE_idtype_get_info_from_id(const struct ID *id);

#ifdef __cplusplus
}
#endif

#endif /* __BKE_IDTYPE_H__ */
