/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_session_uid.hh"

namespace blender {

TEST(SessionUID, GenerateBasic)
{
  {
    const SessionUID uid = BLI_session_uid_generate();
    EXPECT_TRUE(uid.is_generated());
  }

  {
    const SessionUID uid1 = BLI_session_uid_generate();
    const SessionUID uid2 = BLI_session_uid_generate();

    EXPECT_FALSE(uid1 == uid2);
  }
}

}  // namespace blender
