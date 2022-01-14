/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "BLI_utildefines.h"

#include "ED_keyframes_keylist.h"

#include "DNA_anim_types.h"
#include "DNA_curve_types.h"

#include "MEM_guardedalloc.h"

#include "BKE_fcurve.h"

#include <functional>
#include <optional>

namespace blender::editor::animation::tests {

const float KEYLIST_NEAR_ERROR = 0.1;
const float FRAME_STEP = 0.005;

static void build_fcurve(FCurve &fcurve)
{
  fcurve.totvert = 3;
  fcurve.bezt = static_cast<BezTriple *>(
      MEM_callocN(sizeof(BezTriple) * fcurve.totvert, "BezTriples"));
  fcurve.bezt[0].vec[1][0] = 10.0f;
  fcurve.bezt[0].vec[1][1] = 1.0f;
  fcurve.bezt[1].vec[1][0] = 20.0f;
  fcurve.bezt[1].vec[1][1] = 2.0f;
  fcurve.bezt[2].vec[1][0] = 30.0f;
  fcurve.bezt[2].vec[1][1] = 1.0f;
}

static AnimKeylist *create_test_keylist()
{
  FCurve *fcurve = BKE_fcurve_create();
  build_fcurve(*fcurve);

  AnimKeylist *keylist = ED_keylist_create();
  fcurve_to_keylist(nullptr, fcurve, keylist, 0);
  BKE_fcurve_free(fcurve);

  ED_keylist_prepare_for_direct_access(keylist);
  return keylist;
}

static void assert_act_key_column(const ActKeyColumn *column,
                                  const std::optional<float> expected_frame)
{
  if (expected_frame.has_value()) {
    EXPECT_NE(column, nullptr);
    EXPECT_NEAR(column->cfra, *expected_frame, KEYLIST_NEAR_ERROR);
  }
  else {
    EXPECT_EQ(column, nullptr);
  }
}

using KeylistFindFunction = std::function<const ActKeyColumn *(const AnimKeylist *, float)>;

static float check_keylist_find_range(const AnimKeylist *keylist,
                                      KeylistFindFunction keylist_find_func,
                                      const float frame_from,
                                      const float frame_to,
                                      const std::optional<float> expected_frame)
{
  float cfra = frame_from;
  for (; cfra < frame_to; cfra += FRAME_STEP) {
    const ActKeyColumn *found = keylist_find_func(keylist, cfra);
    assert_act_key_column(found, expected_frame);
  }
  return cfra;
}

static float check_keylist_find_next_range(const AnimKeylist *keylist,
                                           const float frame_from,
                                           const float frame_to,
                                           const std::optional<float> expected_frame)
{
  return check_keylist_find_range(
      keylist, ED_keylist_find_next, frame_from, frame_to, expected_frame);
}

TEST(keylist, find_next)
{
  AnimKeylist *keylist = create_test_keylist();

  float cfra = check_keylist_find_next_range(keylist, 0.0f, 9.99f, 10.0f);
  cfra = check_keylist_find_next_range(keylist, cfra, 19.99f, 20.0f);
  cfra = check_keylist_find_next_range(keylist, cfra, 29.99f, 30.0f);
  cfra = check_keylist_find_next_range(keylist, cfra, 39.99f, std::nullopt);

  ED_keylist_free(keylist);
}

static float check_keylist_find_prev_range(const AnimKeylist *keylist,
                                           const float frame_from,
                                           const float frame_to,
                                           const std::optional<float> expected_frame)
{
  return check_keylist_find_range(
      keylist, ED_keylist_find_prev, frame_from, frame_to, expected_frame);
}

TEST(keylist, find_prev)
{
  AnimKeylist *keylist = create_test_keylist();

  float cfra = check_keylist_find_prev_range(keylist, 0.0f, 10.01f, std::nullopt);
  cfra = check_keylist_find_prev_range(keylist, cfra, 20.01f, 10.0f);
  cfra = check_keylist_find_prev_range(keylist, cfra, 30.01f, 20.0f);
  cfra = check_keylist_find_prev_range(keylist, cfra, 49.99f, 30.0f);

  ED_keylist_free(keylist);
}

static float check_keylist_find_exact_range(const AnimKeylist *keylist,
                                            const float frame_from,
                                            const float frame_to,
                                            const std::optional<float> expected_frame)
{
  return check_keylist_find_range(
      keylist, ED_keylist_find_exact, frame_from, frame_to, expected_frame);
}

TEST(keylist, find_exact)
{
  AnimKeylist *keylist = create_test_keylist();

  float cfra = check_keylist_find_exact_range(keylist, 0.0f, 9.99f, std::nullopt);
  cfra = check_keylist_find_exact_range(keylist, cfra, 10.01f, 10.0f);
  cfra = check_keylist_find_exact_range(keylist, cfra, 19.99f, std::nullopt);
  cfra = check_keylist_find_exact_range(keylist, cfra, 20.01f, 20.0f);
  cfra = check_keylist_find_exact_range(keylist, cfra, 29.99f, std::nullopt);
  cfra = check_keylist_find_exact_range(keylist, cfra, 30.01f, 30.0f);
  cfra = check_keylist_find_exact_range(keylist, cfra, 49.99f, std::nullopt);

  ED_keylist_free(keylist);
}

}  // namespace blender::editor::animation::tests
