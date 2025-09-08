/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* The tests in this file need to be able to test deprecated data as well. */
#define DNA_DEPRECATED_ALLOW

#include "ANIM_versioning.hh"

#include "DNA_action_types.h"

#include "BLI_listbase.h"

#include "testing/testing.h"

namespace blender::animrig::versioning::tests {

TEST(animrig_versioning, action_is_layered)
{
  /* This unit test doesn't put valid data in the action under test. Since action_is_layered()
   * only looks at the length of lists, and not their contents, that should be fine. */

  { /* Animato Action only fcurves / Blender version [2.5, 4.4) */
    bAction action = {};
    Link /* FCurve */ fake_fcurve = {};

    BLI_addtail(&action.curves, &fake_fcurve);
    EXPECT_FALSE(action_is_layered(action))
        << "Animato Actions should NOT be considered 'layered'";
  }

  { /* Animato Action with fcurves + groups / Blender version [2.5, 4.4) */
    bAction action = {};
    Link /* FCurve */ fake_fcurve = {};
    Link /* bActionGroup */ fake_group = {};

    BLI_addtail(&action.curves, &fake_fcurve);
    BLI_addtail(&action.groups, &fake_group);
    EXPECT_FALSE(action_is_layered(action))
        << "Animato Actions should NOT be considered 'layered'";
  }

  { /* Animato Action with only groups / Blender version [2.5, 4.4) */
    bAction action = {};
    Link /* bActionGroup */ fake_group = {};

    BLI_addtail(&action.groups, &fake_group);
    EXPECT_FALSE(action_is_layered(action))
        << "Animato Actions should NOT be considered 'layered'";
  }

  { /* Layered Action with only layers / Blender version 4.4 and newer. */
    bAction action = {};
    action.layer_array_num = 1;

    EXPECT_TRUE(action_is_layered(action)) << "Layered Actions should be considered 'layered'";
  }

  { /* Layered Action with only slots / Blender version 4.4 and newer. */
    bAction action = {};
    action.slot_array_num = 1;

    EXPECT_TRUE(action_is_layered(action)) << "Layered Actions should be considered 'layered'";
  }

  { /* Layered Action as it exists on disk, with forward-compatible info in there. */
    bAction action = {};
    Link /* FCurve */ fake_fcurve = {};
    action.layer_array_num = 1;

    BLI_addtail(&action.curves, &fake_fcurve);
    EXPECT_TRUE(action_is_layered(action))
        << "Layered Actions with forward-compat data should be considered 'layered'";
  }

  { /* Completely zeroed out Action. */
    bAction action = {};
    EXPECT_TRUE(action_is_layered(action)) << "Zero'ed-out Actions should be considered 'layered'";
  }
}

}  // namespace blender::animrig::versioning::tests
