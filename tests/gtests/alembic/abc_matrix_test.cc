#include "testing/testing.h"

// Keep first since utildefines defines AT which conflicts with fucking STL
#include "intern/abc_util.h"

extern "C" {
#include "BLI_utildefines.h"
#include "BLI_math.h"
}


TEST(abc_matrix, CreateRotationMatrixY_YfromZ) {
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
	create_swapped_rotation_matrix(rot_x_mat, rot_y_mat, rot_z_mat, euler,
	                               ABC_YUP_FROM_ZUP);

	EXPECT_M3_NEAR(rot_x_mat, unit, 1e-5f);
	EXPECT_M3_NEAR(rot_y_mat, unit, 1e-5f);
	EXPECT_M3_NEAR(rot_z_mat, rot_z_min_quart_pi, 1e-5f);
}

TEST(abc_matrix, CreateRotationMatrixZ_YfromZ) {
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
	create_swapped_rotation_matrix(rot_x_mat, rot_y_mat, rot_z_mat, euler,
	                               ABC_YUP_FROM_ZUP);

	EXPECT_M3_NEAR(rot_x_mat, unit, 1e-5f);
	EXPECT_M3_NEAR(rot_y_mat, rot_y_quart_pi, 1e-5f);
	EXPECT_M3_NEAR(rot_z_mat, unit, 1e-5f);
}

TEST(abc_matrix, CreateRotationMatrixXYZ_YfromZ) {
	// Input variables
	float rot_x_mat[3][3];
	float rot_y_mat[3][3];
	float rot_z_mat[3][3];
	// in degrees: X=10, Y=20, Z=30
	float euler[3] = {0.1745329201221466f, 0.3490658104419708f, 0.5235987901687622f};

	// Construct expected matrices
	float rot_x_p10[3][3];  // rotation of +10 degrees over x-axis
	float rot_y_p30[3][3];  // rotation of +30 degrees over y-axis
	float rot_z_m20[3][3];  // rotation of -20 degrees over z-axis

	unit_m3(rot_x_p10);
	rot_x_p10[1][1] =  0.9848077297210693f;
	rot_x_p10[1][2] =  0.1736481785774231f;
	rot_x_p10[2][1] = -0.1736481785774231f;
	rot_x_p10[2][2] =  0.9848077297210693f;

	unit_m3(rot_y_p30);
	rot_y_p30[0][0] =  0.8660253882408142f;
	rot_y_p30[0][2] = -0.5f;
	rot_y_p30[2][0] =  0.5f;
	rot_y_p30[2][2] =  0.8660253882408142f;

	unit_m3(rot_z_m20);
	rot_z_m20[0][0] =  0.9396926164627075f;
	rot_z_m20[0][1] = -0.3420201241970062f;
	rot_z_m20[1][0] =  0.3420201241970062f;
	rot_z_m20[1][1] =  0.9396926164627075f;

	// Run tests
	create_swapped_rotation_matrix(rot_x_mat, rot_y_mat, rot_z_mat, euler,
	                               ABC_YUP_FROM_ZUP);

	EXPECT_M3_NEAR(rot_x_mat, rot_x_p10, 1e-5f);
	EXPECT_M3_NEAR(rot_y_mat, rot_y_p30, 1e-5f);
	EXPECT_M3_NEAR(rot_z_mat, rot_z_m20, 1e-5f);
}

TEST(abc_matrix, CreateRotationMatrixXYZ_ZfromY) {
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
	rot_x_p10[1][1] =  0.9848077297210693f;
	rot_x_p10[1][2] =  0.1736481785774231f;
	rot_x_p10[2][1] = -0.1736481785774231f;
	rot_x_p10[2][2] =  0.9848077297210693f;

	unit_m3(rot_y_m30);
	rot_y_m30[0][0] =  0.8660253882408142f;
	rot_y_m30[0][2] =  0.5f;
	rot_y_m30[2][0] = -0.5f;
	rot_y_m30[2][2] =  0.8660253882408142f;

	unit_m3(rot_z_p20);
	rot_z_p20[0][0] =  0.9396926164627075f;
	rot_z_p20[0][1] =  0.3420201241970062f;
	rot_z_p20[1][0] = -0.3420201241970062f;
	rot_z_p20[1][1] =  0.9396926164627075f;

	// Run tests
	create_swapped_rotation_matrix(rot_x_mat, rot_y_mat, rot_z_mat, euler,
	                               ABC_ZUP_FROM_YUP);

	EXPECT_M3_NEAR(rot_x_mat, rot_x_p10, 1e-5f);
	EXPECT_M3_NEAR(rot_y_mat, rot_y_m30, 1e-5f);
	EXPECT_M3_NEAR(rot_z_mat, rot_z_p20, 1e-5f);
}
