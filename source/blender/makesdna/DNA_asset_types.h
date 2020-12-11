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

/** \file
 * \ingroup DNA
 */

#ifndef __DNA_ASSET_TYPES_H__
#define __DNA_ASSET_TYPES_H__

#include "DNA_listBase.h"

/**
 * \brief User defined tag.
 * Currently only used by assets, could be used more often at some point.
 * Maybe add a custom icon and color to these in future?
 */
typedef struct AssetTag {
  struct AssetTag *next, *prev;
  char name[64]; /* MAX_NAME */
} AssetTag;

/**
 * \brief The meta-data of an asset.
 * By creating and giving this for a data-block (#ID.asset_data), the data-block becomes an asset.
 *
 * \note This struct must be readable without having to read anything but blocks from the ID it is
 *       attached to! That way, asset information of a file can be read, without reading anything
 *       more than that from the file. So pointers to other IDs or ID data are strictly forbidden.
 */
typedef struct AssetMetaData {
  /** Custom asset meta-data. Cannot store pointers to IDs (#STRUCT_NO_DATABLOCK_IDPROPERTIES)! */
  struct IDProperty *properties;

  /** Optional description of this asset for display in the UI. Dynamic length. */
  char *description;
  /** User defined tags for this asset. The asset manager uses these for filtering, but how they
   * function exactly (e.g. how they are registered to provide a list of searchable available tags)
   * is up to the asset-engine. */
  ListBase tags; /* AssetTag */
  short active_tag;
  /** Store the number of tags to avoid continuous counting. Could be turned into runtime data, we
   * can always reliably reconstruct it from the list. */
  short tot_tags;

  char _pad[4];
} AssetMetaData;

#endif /* __DNA_ASSET_TYPES_H__ */
