/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_session_uid.h"

TEST(SessionUID, GenerateBasic)
{
  {
    const SessionUID uid = BLI_session_uid_generate();
    EXPECT_TRUE(BLI_session_uid_is_generated(&uid));
  }

  {
    const SessionUID uid1 = BLI_session_uid_generate();
    const SessionUID uid2 = BLI_session_uid_generate();

    EXPECT_FALSE(BLI_session_uid_is_equal(&uid1, &uid2));
  }
}
