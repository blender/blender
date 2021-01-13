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
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#pragma once

#include <string>

#include "BLI_string_ref.hh"

namespace blender {

/* Format to a cryptomatte meta data key.
 *
 * Cryptomatte stores meta data. The keys are formatted containing a hash that
 * is generated from its layer name.
 *
 * The output of this function is:
 * 'cryptomatte/{hash of layer_name}/{key_name}'.
 */
std::string BKE_cryptomatte_meta_data_key(const StringRef layer_name,
                                          const StringRefNull key_name);

/* Extract the cryptomatte layer name from the given `render_pass_name`.
 *
 * Cryptomatte passes are formatted with a trailing number for storing multiple samples that belong
 * to the same cryptomatte layer. This function would remove the trailing numbers to determine the
 * cryptomatte layer name.
 *
 * # Example
 *
 * A render_pass_name could be 'View Layer.CryptoMaterial02'. The cryptomatte layer would be 'View
 * Layer.CryptoMaterial'.
 *
 * NOTE: The return type is a sub-string of `render_pass_name` and therefore cannot outlive the
 * `render_pass_name` internal data.
 */
StringRef BKE_cryptomatte_extract_layer_name(const StringRef render_pass_name);

}  // namespace blender
