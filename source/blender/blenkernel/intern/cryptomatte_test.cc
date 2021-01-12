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
 * The Original Code is Copyright (C) 2021 by Blender Foundation.
 */
#include "testing/testing.h"

#include "BKE_cryptomatte.hh"

namespace blender::bke::tests {

TEST(cryptomatte, meta_data_key)
{
  ASSERT_EQ("cryptomatte/c7dbf5e/key",
            BKE_cryptomatte_meta_data_key("ViewLayer.CryptoMaterial", "key"));
  ASSERT_EQ("cryptomatte/b990b65/𝓴𝓮𝔂",
            BKE_cryptomatte_meta_data_key("𝖚𝖓𝖎𝖈𝖔𝖉𝖊.CryptoMaterial", "𝓴𝓮𝔂"));
}

TEST(cryptomatte, extract_layer_name)
{
  ASSERT_EQ("ViewLayer.CryptoMaterial",
            BKE_cryptomatte_extract_layer_name("ViewLayer.CryptoMaterial00"));
  ASSERT_EQ("𝖚𝖓𝖎𝖈𝖔𝖉𝖊", BKE_cryptomatte_extract_layer_name("𝖚𝖓𝖎𝖈𝖔𝖉𝖊13"));
  ASSERT_EQ("NoTrailingSampleNumber",
            BKE_cryptomatte_extract_layer_name("NoTrailingSampleNumber"));
  ASSERT_EQ("W1thM1dd13Numb3rs", BKE_cryptomatte_extract_layer_name("W1thM1dd13Numb3rs09"));
  ASSERT_EQ("", BKE_cryptomatte_extract_layer_name("0123"));
  ASSERT_EQ("", BKE_cryptomatte_extract_layer_name(""));
}

}  // namespace blender::bke::tests
