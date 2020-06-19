#include "testing/testing.h"

// Keep first since utildefines defines AT which conflicts with STL
#include "intern/abc_axis_conversion.h"

extern "C" {
#include "BLI_math.h"
#include "BLI_utildefines.h"
}

using namespace blender::io::alembic;

TEST(abc_matrix, CreateRotationMatrixY_YfromZ)
{
  // Input variables
  float rot_x_mat[3][3];
  float rot_y_mat[3][3];
  float rot_z_mat[3][3];
  float euler[3] = {0.f, M_PI_4, 0.f};

  // Construct expected matrices
  float unit[3][3];
  float rot_z_min_quart_pi[3][3];  // rotation of -pi/4 radians over z-axis

  unit_m3(unit);
  unit_m3(rot_z_min_quart_pi);
  rot_z_min_quart_pi[0][0] = M_SQRT1_2;
  rot_z_min_quart_pi[0][1] = -M_SQRT1_2;
  rot_z_min_quart_pi[1][0] = M_SQRT1_2;
  rot_z_min_quart_pi[1][1] = M_SQRT1_2;

  // Run tests
  create_swapped_rotation_matrix(rot_x_mat, rot_y_mat, rot_z_mat, euler, ABC_YUP_FROM_ZUP);

  EXPECT_M3_NEAR(rot_x_mat, unit, 1e-5f);
  EXPECT_M3_NEAR(rot_y_mat, unit, 1e-5f);
  EXPECT_M3_NEAR(rot_z_mat, rot_z_min_quart_pi, 1e-5f);
}

TEST(abc_matrix, CreateRotationMatrixZ_YfromZ)
{
  // Input variables
  float rot_x_mat[3][3];
  float rot_y_mat[3][3];
  float rot_z_mat[3][3];
  float euler[3] = {0.f, 0.f, M_PI_4};

  // Construct expected matrices
  float unit[3][3];
  float rot_y_quart_pi[3][3];  // rotation of pi/4 radians over y-axis

  unit_m3(unit);
  unit_m3(rot_y_quart_pi);
  rot_y_quart_pi[0][0] = M_SQRT1_2;
  rot_y_quart_pi[0][2] = -M_SQRT1_2;
  rot_y_quart_pi[2][0] = M_SQRT1_2;
  rot_y_quart_pi[2][2] = M_SQRT1_2;

  // Run tests
  create_swapped_rotation_matrix(rot_x_mat, rot_y_mat, rot_z_mat, euler, ABC_YUP_FROM_ZUP);

  EXPECT_M3_NEAR(rot_x_mat, unit, 1e-5f);
  EXPECT_M3_NEAR(rot_y_mat, rot_y_quart_pi, 1e-5f);
  EXPECT_M3_NEAR(rot_z_mat, unit, 1e-5f);
}

TEST(abc_matrix, CreateRotationMatrixXYZ_YfromZ)
{
  // Input variables
  float rot_x_mat[3][3];
  float rot_y_mat[3][3];
  float rot_z_mat[3][3];
  // in degrees: X=10, Y=20, Z=30
  float euler[3] = {0.17453292012214f, 0.34906581044197f, 0.52359879016876f};

  // Construct expected matrices
  float rot_x_p10[3][3];  // rotation of +10 degrees over x-axis
  float rot_y_p30[3][3];  // rotation of +30 degrees over y-axis
  float rot_z_m20[3][3];  // rotation of -20 degrees over z-axis

  unit_m3(rot_x_p10);
  rot_x_p10[1][1] = 0.9848077297210693f;
  rot_x_p10[1][2] = 0.1736481785774231f;
  rot_x_p10[2][1] = -0.1736481785774231f;
  rot_x_p10[2][2] = 0.9848077297210693f;

  unit_m3(rot_y_p30);
  rot_y_p30[0][0] = 0.8660253882408142f;
  rot_y_p30[0][2] = -0.5f;
  rot_y_p30[2][0] = 0.5f;
  rot_y_p30[2][2] = 0.8660253882408142f;

  unit_m3(rot_z_m20);
  rot_z_m20[0][0] = 0.9396926164627075f;
  rot_z_m20[0][1] = -0.3420201241970062f;
  rot_z_m20[1][0] = 0.3420201241970062f;
  rot_z_m20[1][1] = 0.9396926164627075f;

  // Run tests
  create_swapped_rotation_matrix(rot_x_mat, rot_y_mat, rot_z_mat, euler, ABC_YUP_FROM_ZUP);

  EXPECT_M3_NEAR(rot_x_mat, rot_x_p10, 1e-5f);
  EXPECT_M3_NEAR(rot_y_mat, rot_y_p30, 1e-5f);
  EXPECT_M3_NEAR(rot_z_mat, rot_z_m20, 1e-5f);
}

TEST(abc_matrix, CreateRotationMatrixXYZ_ZfromY)
{
  // Input variables
  float rot_x_mat[3][3];
  float rot_y_mat[3][3];
  float rot_z_mat[3][3];
  // in degrees: X=10, Y=20, Z=30
  float euler[3] = {0.1745329201221466f, 0.3490658104419708f, 0.5235987901687622f};

  // Construct expected matrices
  float rot_x_p10[3][3];  // rotation of +10 degrees over x-axis
  float rot_y_m30[3][3];  // rotation of -30 degrees over y-axis
  float rot_z_p20[3][3];  // rotation of +20 degrees over z-axis

  unit_m3(rot_x_p10);
  rot_x_p10[1][1] = 0.9848077297210693f;
  rot_x_p10[1][2] = 0.1736481785774231f;
  rot_x_p10[2][1] = -0.1736481785774231f;
  rot_x_p10[2][2] = 0.9848077297210693f;

  unit_m3(rot_y_m30);
  rot_y_m30[0][0] = 0.8660253882408142f;
  rot_y_m30[0][2] = 0.5f;
  rot_y_m30[2][0] = -0.5f;
  rot_y_m30[2][2] = 0.8660253882408142f;

  unit_m3(rot_z_p20);
  rot_z_p20[0][0] = 0.9396926164627075f;
  rot_z_p20[0][1] = 0.3420201241970062f;
  rot_z_p20[1][0] = -0.3420201241970062f;
  rot_z_p20[1][1] = 0.9396926164627075f;

  // Run tests
  create_swapped_rotation_matrix(rot_x_mat, rot_y_mat, rot_z_mat, euler, ABC_ZUP_FROM_YUP);

  EXPECT_M3_NEAR(rot_x_mat, rot_x_p10, 1e-5f);
  EXPECT_M3_NEAR(rot_y_mat, rot_y_m30, 1e-5f);
  EXPECT_M3_NEAR(rot_z_mat, rot_z_p20, 1e-5f);
}

TEST(abc_matrix, CopyM44AxisSwap_YfromZ)
{
  float result[4][4];

  /* Construct an input matrix that performs a rotation like the tests
   * above. This matrix was created by rotating a cube in Blender over
   * (X=10, Y=20, Z=30 degrees in XYZ order) and translating over (1, 2, 3) */
  float input[4][4] = {
      {0.81379765272f, 0.4698463380336f, -0.342020124197f, 0.f},
      {-0.44096961617f, 0.8825641274452f, 0.163175910711f, 0.f},
      {0.37852230668f, 0.0180283170193f, 0.925416588783f, 0.f},
      {1.f, 2.f, 3.f, 1.f},
  };

  copy_m44_axis_swap(result, input, ABC_YUP_FROM_ZUP);

  /* Check the resulting rotation & translation. */
  float trans[4] = {1.f, 3.f, -2.f, 1.f};
  EXPECT_V4_NEAR(trans, result[3], 1e-5f);

  /* This matrix was created by rotating a cube in Blender over
   * (X=10, Y=30, Z=-20 degrees in XZY order) and translating over (1, 3, -2) */
  float expect[4][4] = {
      {0.813797652721f, -0.342020124197f, -0.469846338033f, 0.f},
      {0.378522306680f, 0.925416588783f, -0.018028317019f, 0.f},
      {0.440969616174f, -0.163175910711f, 0.882564127445f, 0.f},
      {1.f, 3.f, -2.f, 1.f},
  };
  EXPECT_M4_NEAR(expect, result, 1e-5f);
}

TEST(abc_matrix, CopyM44AxisSwapWithScale_YfromZ)
{
  float result[4][4];

  /* Construct an input matrix that performs a rotation like the tests
   * above. This matrix was created by rotating a cube in Blender over
   * (X=10, Y=20, Z=30 degrees in XYZ order), translating over (1, 2, 3),
   * and scaling by (4, 5, 6). */
  float input[4][4] = {
      {3.25519061088f, 1.8793853521347f, -1.368080496788f, 0.f},
      {-2.20484805107f, 4.4128208160400f, 0.815879583358f, 0.f},
      {2.27113389968f, 0.1081698983907f, 5.552499771118f, 0.f},
      {1.f, 2.f, 3.f, 1.f},
  };

  copy_m44_axis_swap(result, input, ABC_YUP_FROM_ZUP);

  /* This matrix was created by rotating a cube in Blender over
   * (X=10, Y=30, Z=-20 degrees in XZY order), translating over (1, 3, -2)
   * and scaling over (4, 6, 5). */
  float expect[4][4] = {
      {3.255190610885f, -1.368080496788f, -1.879385352134f, 0.f},
      {2.271133899688f, 5.552499771118f, -0.108169898390f, 0.f},
      {2.204848051071f, -0.815879583358f, 4.412820816040f, 0.f},
      {1.f, 3.f, -2.f, 1.f},
  };
  EXPECT_M4_NEAR(expect, result, 1e-5f);
}

TEST(abc_matrix, CopyM44AxisSwap_ZfromY)
{
  float result[4][4];

  /* This matrix was created by rotating a cube in Blender over
   * (X=10, Y=30, Z=-20 degrees in XZY order) and translating over (1, 3, -2) */
  float input[4][4] = {
      {0.813797652721f, -0.342020124197f, -0.469846338033f, 0.f},
      {0.378522306680f, 0.925416588783f, -0.018028317019f, 0.f},
      {0.440969616174f, -0.163175910711f, 0.882564127445f, 0.f},
      {1.f, 3.f, -2.f, 1.f},
  };

  copy_m44_axis_swap(result, input, ABC_ZUP_FROM_YUP);

  /* This matrix was created by rotating a cube in Blender over
   * (X=10, Y=20, Z=30 degrees in XYZ order) and translating over (1, 2, 3) */
  float expect[4][4] = {
      {0.813797652721f, 0.469846338033f, -0.342020124197f, 0.f},
      {-0.44096961617f, 0.882564127445f, 0.163175910711f, 0.f},
      {0.378522306680f, 0.018028317019f, 0.925416588783f, 0.f},
      {1.f, 2.f, 3.f, 1.f},
  };

  EXPECT_M4_NEAR(expect, result, 1e-5f);
}

TEST(abc_matrix, CopyM44AxisSwapWithScale_ZfromY)
{
  float result[4][4];

  /* This matrix was created by rotating a cube in Blender over
   * (X=10, Y=30, Z=-20 degrees in XZY order), translating over (1, 3, -2)
   * and scaling over (4, 6, 5). */
  float input[4][4] = {
      {3.2551906108f, -1.36808049678f, -1.879385352134f, 0.f},
      {2.2711338996f, 5.55249977111f, -0.108169898390f, 0.f},
      {2.2048480510f, -0.81587958335f, 4.412820816040f, 0.f},
      {1.f, 3.f, -2.f, 1.f},
  };

  copy_m44_axis_swap(result, input, ABC_ZUP_FROM_YUP);

  /* This matrix was created by rotating a cube in Blender over
   * (X=10, Y=20, Z=30 degrees in XYZ order), translating over (1, 2, 3),
   * and scaling by (4, 5, 6). */
  float expect[4][4] = {
      {3.25519061088f, 1.879385352134f, -1.36808049678f, 0.f},
      {-2.2048480510f, 4.412820816040f, 0.81587958335f, 0.f},
      {2.27113389968f, 0.108169898390f, 5.55249977111f, 0.f},
      {1.f, 2.f, 3.f, 1.f},
  };

  EXPECT_M4_NEAR(expect, result, 1e-5f);
}

TEST(abc_matrix, CopyM44AxisSwapWithScale_gimbal_ZfromY)
{
  float result[4][4];

  /* This matrix represents a rotation over (-90, -0, -0) degrees,
   * and a translation over (-0, -0.1, -0). It is in Y=up. */
  float input[4][4] = {
      {1.000f, 0.000f, 0.000f, 0.000f},
      {0.000f, 0.000f, -1.000f, 0.000f},
      {0.000f, 1.000f, 0.000f, 0.000f},
      {-0.000f, -0.100f, -0.000f, 1.000f},
  };

  copy_m44_axis_swap(result, input, ABC_ZUP_FROM_YUP);

  /* Since the rotation is only over the X-axis, it should not change.
   * The translation does change. */
  float expect[4][4] = {
      {1.000f, 0.000f, 0.000f, 0.000f},
      {0.000f, 0.000f, -1.000f, 0.000f},
      {0.000f, 1.000f, 0.000f, 0.000f},
      {-0.000f, 0.000f, -0.100f, 1.000f},
  };

  EXPECT_M4_NEAR(expect, result, 1e-5f);
}
