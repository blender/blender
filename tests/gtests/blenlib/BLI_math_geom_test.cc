/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "BLI_math.h"

TEST(math_geom, DistToLine2DSimple)
{
	float p[2] = {5.0f, 1.0f},
	      a[2] = {0.0f, 0.0f},
	      b[2] = {2.0f, 0.0f};
	float distance = dist_to_line_v2(p, a, b);
	EXPECT_NEAR(1.0f, distance, 1e-6);
}

TEST(math_geom, DistToLineSegment2DSimple)
{
	float p[2] = {3.0f, 1.0f},
	      a[2] = {0.0f, 0.0f},
	      b[2] = {2.0f, 0.0f};
	float distance = dist_to_line_segment_v2(p, a, b);
	EXPECT_NEAR(sqrtf(2.0f), distance, 1e-6);
}
