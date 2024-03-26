/* SPDX-License-Identifier: Apache-2.0 */

#include "BLI_fixed_width_int.hh"
#include "BLI_fixed_width_int_str.hh"
#include "BLI_rand.hh"
#include "BLI_timeit.hh"
#include "BLI_vector.hh"

#include "testing/testing.h"

/* See `BLI_fixed_width_int_str.hh` for why this is necessary.  */
#ifdef WITH_GMP

namespace blender::fixed_width_int::tests {

TEST(fixed_width_int, IsZero)
{
  EXPECT_TRUE(is_zero(UInt256(0)));
  EXPECT_TRUE(is_zero(UInt256(10) - UInt256(10)));
  EXPECT_TRUE(is_zero(UInt256(10) - UInt256(15) + UInt256(5)));
  EXPECT_FALSE(is_zero(UInt256(10)));

  EXPECT_TRUE(is_zero(Int256(0)));
  EXPECT_TRUE(is_zero(Int256(10) - Int256(10)));
  EXPECT_TRUE(is_zero(Int256(10) - Int256(15) + Int256(5)));
  EXPECT_FALSE(is_zero(Int256(10)));
  EXPECT_FALSE(is_zero(Int256(-10)));
}

TEST(fixed_width_int, ToString)
{
  {
    const std::string str = "4875677549274093345634534";
    EXPECT_EQ(UInt256(str).to_string(), str);
  }
  {
    const std::string str = "0";
    EXPECT_EQ(UInt256(str).to_string(), str);
  }
  {
    const std::string str = "4875677549274093345634534";
    EXPECT_EQ(Int256(str).to_string(), str);
  }
  {
    const std::string str = "-4875677549274093345634534";
    EXPECT_EQ(Int256(str).to_string(), str);
  }
  {
    const std::string str = "0";
    EXPECT_EQ(Int256(str).to_string(), str);
  }
}

TEST(fixed_width_int, Add256)
{
  EXPECT_EQ(UInt256("290213998554153310989149424513459608072") +
                UInt256("236559186774771353723629567597011581379"),
            UInt256("526773185328924664712778992110471189451"));
  EXPECT_EQ(UInt256("211377365172829431692550347604827003294") +
                UInt256("151035310604094577723885879186052138391"),
            UInt256("362412675776924009416436226790879141685"));
  EXPECT_EQ(UInt256("34490924248914309185690728897294455642") +
                UInt256("151329651396698072567782489740109235288"),
            UInt256("185820575645612381753473218637403690930"));
  EXPECT_EQ(UInt256("23020790973174243895398009931650855178") +
                UInt256("242538071468046767660828531945711005380"),
            UInt256("265558862441221011556226541877361860558"));
  EXPECT_EQ(UInt256("220030846719277288761017165278417179519") +
                UInt256("13817458575896368146281651263001012349"),
            UInt256("233848305295173656907298816541418191868"));
  EXPECT_EQ(UInt256("225958958932723616286848406010143428110") +
                UInt256("309322190961572274983773819144991425669"),
            UInt256("535281149894295891270622225155134853779"));
  EXPECT_EQ(UInt256("166851370558999106635673647011389012481") +
                UInt256("85443075281725354911889976920463997722"),
            UInt256("252294445840724461547563623931853010203"));
  EXPECT_EQ(UInt256("274485954517155769304275705148933346392") +
                UInt256("215279677420695754877443907998549347900"),
            UInt256("489765631937851524181719613147482694292"));
  EXPECT_EQ(UInt256("3522191569845770793524407096643088669") +
                UInt256("100106234023644716469012457480771518776"),
            UInt256("103628425593490487262536864577414607445"));
  EXPECT_EQ(UInt256("163994307071630654616433355844082912619") +
                UInt256("263001956277142014131208604303902541977"),
            UInt256("426996263348772668747641960147985454596"));
}

TEST(fixed_width_int, Fuzzy)
{
  RandomNumberGenerator rng;
  for ([[maybe_unused]] const int i : IndexRange(10000)) {
    {
      const uint64_t a = rng.get_uint64();
      const uint64_t b = rng.get_uint64();
      EXPECT_EQ(a + b, uint64_t(UInt64_8(a) + UInt64_8(b)));
      EXPECT_EQ(a * b, uint64_t(UInt64_8(a) * UInt64_8(b)));
      EXPECT_EQ(a - b, uint64_t(UInt64_8(a) - UInt64_8(b)));
      EXPECT_EQ(a < b, UInt64_8(a) < UInt64_8(b));
      EXPECT_EQ(a > b, UInt64_8(a) > UInt64_8(b));
      EXPECT_EQ(a <= b, UInt64_8(a) <= UInt64_8(b));
      EXPECT_EQ(a >= b, UInt64_8(a) >= UInt64_8(b));
      EXPECT_EQ(a == b, UInt64_8(a) == UInt64_8(b));
      EXPECT_EQ(a != b, UInt64_8(a) != UInt64_8(b));
      EXPECT_FLOAT_EQ(double(a), double(UInt64_8(a)));
      EXPECT_FLOAT_EQ(float(a), float(UInt64_8(a)));
    }
    {
      const int64_t a = int64_t(rng.get_uint64()) * (rng.get_float() < 0.5f ? -1 : 1);
      const int64_t b = int64_t(rng.get_uint64()) * (rng.get_float() < 0.5f ? -1 : 1);
      EXPECT_EQ(a + b, int64_t(Int64_8(a) + Int64_8(b)));
      EXPECT_EQ(a * b, int64_t(Int64_8(a) * Int64_8(b)));
      EXPECT_EQ(a - b, int64_t(Int64_8(a) - Int64_8(b)));
      EXPECT_EQ(a < b, Int64_8(a) < Int64_8(b));
      EXPECT_EQ(a > b, Int64_8(a) > Int64_8(b));
      EXPECT_EQ(a <= b, Int64_8(a) <= Int64_8(b));
      EXPECT_EQ(a >= b, Int64_8(a) >= Int64_8(b));
      EXPECT_EQ(a == b, Int64_8(a) == Int64_8(b));
      EXPECT_EQ(a != b, Int64_8(a) != Int64_8(b));
      EXPECT_EQ(a == 0, is_zero(Int64_8(a)));
      EXPECT_EQ(b == 0, is_zero(Int64_8(b)));
      EXPECT_EQ(a < 0, is_negative(Int64_8(a)));
      EXPECT_EQ(b < 0, is_negative(Int64_8(b)));
      EXPECT_FLOAT_EQ(double(a), double(Int64_8(a)));
      EXPECT_FLOAT_EQ(float(a), float(Int64_8(a)));
    }
  }
}

}  // namespace blender::fixed_width_int::tests

#endif /* WITH_GMP */
