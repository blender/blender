/* Apache License, Version 2.0 */

extern "C" {

bool EIG_self_adjoint_eigen_solve(const int size, const float *matrix, float *r_eigen_values, float *r_eigen_vectors)
{
	BLI_assert(0);
	UNUSED_VARS(size, matrix, r_eigen_values, r_eigen_vectors);
	return false;
}

void EIG_svd_square_matrix(const int size, const float *matrix, float *r_U, float *r_S, float *r_V)
{
	BLI_assert(0);
	UNUSED_VARS(size, matrix, r_U, r_S, r_V);
}

}
