/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "BLI_math_matrix.h"

TEST(math_matrix, interp_m4_m4m4_regular)
{
  /* Test 4x4 matrix interpolation without singularity, i.e. without axis flip. */

  /* Transposed matrix, so that the code here is written in the same way as print_m4() outputs. */
  /* This matrix represents T=(0.1, 0.2, 0.3), R=(40, 50, 60) degrees, S=(0.7, 0.8, 0.9) */
  float matrix_a[4][4] = {
      {0.224976f, -0.333770f, 0.765074f, 0.100000f},
      {0.389669f, 0.647565f, 0.168130f, 0.200000f},
      {-0.536231f, 0.330541f, 0.443163f, 0.300000f},
      {0.000000f, 0.000000f, 0.000000f, 1.000000f},
  };
  transpose_m4(matrix_a);

  float matrix_i[4][4];
  unit_m4(matrix_i);

  float result[4][4];
  const float epsilon = 1e-6;
  interp_m4_m4m4(result, matrix_i, matrix_a, 0.0f);
  EXPECT_M4_NEAR(result, matrix_i, epsilon);

  interp_m4_m4m4(result, matrix_i, matrix_a, 1.0f);
  EXPECT_M4_NEAR(result, matrix_a, epsilon);

  /* This matrix is based on the current implementation of the code, and isn't guaranteed to be
   * correct. It's just consistent with the current implementation. */
  float matrix_halfway[4][4] = {
      {0.690643f, -0.253244f, 0.484996f, 0.050000f},
      {0.271924f, 0.852623f, 0.012348f, 0.100000f},
      {-0.414209f, 0.137484f, 0.816778f, 0.150000f},
      {0.000000f, 0.000000f, 0.000000f, 1.000000f},
  };

  transpose_m4(matrix_halfway);
  interp_m4_m4m4(result, matrix_i, matrix_a, 0.5f);
  EXPECT_M4_NEAR(result, matrix_halfway, epsilon);
}

TEST(math_matrix, interp_m3_m3m3_singularity)
{
  /* A singluarity means that there is an axis mirror in the rotation component of the matrix. This
   * is reflected in its negative determinant.
   *
   * The interpolation of 4x4 matrices performs linear interpolation on the translation component,
   * and then uses the 3x3 interpolation function to handle rotation and scale. As a result, this
   * test for a singularity in the rotation matrix only needs to test the 3x3 case. */

  /* Transposed matrix, so that the code here is written in the same way as print_m4() outputs. */
  /* This matrix represents R=(4, 5, 6) degrees, S=(-1, 1, 1) */
  float matrix_a[3][3] = {
      {-0.990737f, -0.098227f, 0.093759f},
      {-0.104131f, 0.992735f, -0.060286f},
      {0.087156f, 0.069491f, 0.993768f},
  };
  transpose_m3(matrix_a);
  EXPECT_NEAR(-1.0f, determinant_m3_array(matrix_a), 1e-6);

  float matrix_i[3][3];
  unit_m3(matrix_i);

  float result[3][3];
  const float epsilon = 1e-6;
  interp_m3_m3m3(result, matrix_i, matrix_a, 0.0f);
  EXPECT_M3_NEAR(result, matrix_i, epsilon);

  /* This fails for matrices with a negative determinant, i.e. with an axis mirror in the rotation
   * component. See T77154. */
  // interp_m3_m3m3(result, matrix_i, matrix_a, 1.0f);
  // EXPECT_M3_NEAR(result, matrix_a, epsilon);
}
