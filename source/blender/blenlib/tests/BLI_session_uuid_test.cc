/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_session_uuid.h"

TEST(SessionUUID, GenerateBasic)
{
  {
    const SessionUUID uuid = BLI_session_uuid_generate();
    EXPECT_TRUE(BLI_session_uuid_is_generated(&uuid));
  }

  {
    const SessionUUID uuid1 = BLI_session_uuid_generate();
    const SessionUUID uuid2 = BLI_session_uuid_generate();

    EXPECT_FALSE(BLI_session_uuid_is_equal(&uuid1, &uuid2));
  }
}
