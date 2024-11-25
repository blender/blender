/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#pragma once

struct AssetMetaData;
namespace blender::io::serialize {
class DictionaryValue;
}

namespace blender::ed::asset::index {

AssetMetaData *asset_metadata_from_dictionary(const blender::io::serialize::DictionaryValue &entry);

}
