#include <glm/glm.hpp>
#include <cuda_runtime.h>
#include "AdjacentMatrix.h"

__device__ __host__ glm::dvec3
middle_point(const glm::dvec3 &p0, const glm::dvec3 &n0, const glm::dvec3 &p1, const glm::dvec3 &n1) {
	/* How was this derived?
	*
	* Minimize \|x-p0\|^2 + \|x-p1\|^2, where
	* dot(n0, x) == dot(n0, p0)
	* dot(n1, x) == dot(n1, p1)
	*
	* -> Lagrange multipliers, set derivative = 0
	*  Use first 3 equalities to write x in terms of
	*  lambda_1 and lambda_2. Substitute that into the last
	*  two equations and solve for the lambdas. Finally,
	*  add a small epsilon term to avoid issues when n1=n2.
	*/
	double n0p0 = glm::dot(n0, p0), n0p1 = glm::dot(n0, p1),
		n1p0 = glm::dot(n1, p0), n1p1 = glm::dot(n1, p1),
		n0n1 = glm::dot(n0, n1),
		denom = 1.0f / (1.0f - n0n1*n0n1 + 1e-4f),
		lambda_0 = 2.0f*(n0p1 - n0p0 - n0n1*(n1p0 - n1p1))*denom,
		lambda_1 = 2.0f*(n1p0 - n1p1 - n0n1*(n0p1 - n0p0))*denom;

	return 0.5 * (p0 + p1) - 0.25 * (n0 * lambda_0 + n1 * lambda_1);
}

__device__ __host__ glm::dvec3
position_round_4(const  glm::dvec3 &o, const  glm::dvec3 &q,
const  glm::dvec3 &n, const  glm::dvec3 &p,
double scale) {
	double inv_scale = 1.0 / scale;
	glm::dvec3 t = glm::cross(n, q);
	glm::dvec3 d = p - o;
	return o +
		q * std::round(glm::dot(q, d) * inv_scale) * scale +
		t * std::round(glm::dot(t, d) * inv_scale) * scale;
}

__device__ __host__ glm::dvec3
position_floor_4(const glm::dvec3 &o, const glm::dvec3 &q,
const glm::dvec3 &n, const glm::dvec3 &p,
double scale) {
	double inv_scale = 1.0 / scale;
	glm::dvec3 t = glm::cross(n,q);
	glm::dvec3 d = p - o;
	return o +
		q * std::floor(glm::dot(q, d) * inv_scale) * scale +
		t * std::floor(glm::dot(t, d) * inv_scale) * scale;
}


__device__ __host__ double cudaSignum(double value) {
	return std::copysign((double)1, value);
}

__device__ __host__ void
compat_orientation_extrinsic_4(const glm::dvec3 &q0, const glm::dvec3 &n0,
const glm::dvec3 &q1, const glm::dvec3 &n1, glm::dvec3& value1, glm::dvec3& value2) {
	const glm::dvec3 A[2] = { q0, glm::cross(n0, q0) };
	const glm::dvec3 B[2] = { q1, glm::cross(n1, q1) };

	double best_score = -1e10;
	int best_a = 0, best_b = 0;

	for (int i = 0; i < 2; ++i) {
		for (int j = 0; j < 2; ++j) {
			double score = std::abs(glm::dot(A[i], B[j]));
			if (score > best_score + 1e-6) {
				best_a = i;
				best_b = j;
				best_score = score;
			}
		}
	}
	const double dp = glm::dot(A[best_a], B[best_b]);
	value1 = A[best_a];
	value2 = B[best_b] * cudaSignum(dp);
}

__device__ __host__ void
compat_position_extrinsic_4(
const glm::dvec3 &p0, const glm::dvec3 &n0, const glm::dvec3 &q0, const glm::dvec3 &o0,
const glm::dvec3 &p1, const glm::dvec3 &n1, const glm::dvec3 &q1, const glm::dvec3 &o1,
double scale, glm::dvec3& v1, glm::dvec3& v2) {

	glm::dvec3 t0 = glm::cross(n0, q0), t1 = glm::cross(n1, q1);
	glm::dvec3 middle = middle_point(p0, n0, p1, n1);
	glm::dvec3 o0p = position_floor_4(o0, q0, n0, middle, scale);
	glm::dvec3 o1p = position_floor_4(o1, q1, n1, middle, scale);

	double best_cost = 1e10;
	int best_i = -1, best_j = -1;

	for (int i = 0; i<4; ++i) {
		glm::dvec3 o0t = o0p + (q0 * ((i & 1) * scale) + t0 * (((i & 2) >> 1) * scale));
		for (int j = 0; j<4; ++j) {
			glm::dvec3 o1t = o1p + (q1 * ((j & 1) * scale) + t1 * (((j & 2) >> 1) * scale));
			glm::dvec3 t = o0t - o1t;
			double cost = glm::dot(t, t);

			if (cost < best_cost) {
				best_i = i;
				best_j = j;
				best_cost = cost;
			}
		}
	}

	v1 = o0p + (q0 * ((best_i & 1) * scale) + t0 * (((best_i & 2) >> 1) * scale)),
	v2 = o1p + (q1 * ((best_j & 1) * scale) + t1 * (((best_j & 2) >> 1) * scale));
}

__global__ 
void cudaUpdateOrientation(int* phase, int num_phases, glm::dvec3* N, glm::dvec3* Q, Link* adj, int* adjOffset, int num_adj) {
	int pi = blockIdx.x * blockDim.x + threadIdx.x;

//	for (int pi = 0; pi < num_phases; ++pi) {
		if (pi >= num_phases)
			return;
		int i = phase[pi];
		glm::dvec3 n_i = N[i];
		double weight_sum = 0.0f;
		glm::dvec3 sum = Q[i];

		for (int l = adjOffset[i]; l < adjOffset[i + 1]; ++l) {
			Link link = adj[l];
			const int j = link.id;
			const double weight = link.weight;
			if (weight == 0)
				continue;
			glm::dvec3 n_j = N[j];
			glm::dvec3 q_j = Q[j];
			glm::dvec3 value1, value2;
			compat_orientation_extrinsic_4(sum, n_i, q_j, n_j, value1, value2);
			sum = value1 * weight_sum + value2 * weight;
			sum -= n_i*glm::dot(n_i, sum);
			weight_sum += weight;

			double norm = glm::length(sum);
			if (norm > 2.93873587705571876e-39f)
				sum /= norm;
		}

		if (weight_sum > 0) {
			Q[i] = sum;
		}
//	}
}

__global__
void cudaPropagateOrientationUpper(glm::dvec3* srcField, glm::ivec2* toUpper, glm::dvec3* N, glm::dvec3* destField, int num_orientation) {
	int i = blockIdx.x * blockDim.x + threadIdx.x;
//	for (int i = 0; i < num_orientation; ++i) {
		if (i >= num_orientation)
			return;
		for (int k = 0; k < 2; ++k) {
			int dest = toUpper[i][k];
			if (dest == -1)
				continue;
			glm::dvec3 q = srcField[i];
			glm::dvec3 n = N[dest];
			destField[dest] = q - n * glm::dot(n, q);
		}
//	}
}

__global__
void cudaPropagateOrientationLower(glm::ivec2* toUpper, glm::dvec3* Q, glm::dvec3* N, glm::dvec3* Q_next, glm::dvec3* N_next, int num_toUpper) {
	int i = blockIdx.x * blockDim.x + threadIdx.x;
//	for (int i = 0; i < num_toUpper; ++i) {
		if (i >= num_toUpper)
			return;
		glm::ivec2 upper = toUpper[i];
		glm::dvec3 q0 = Q[upper[0]];
		glm::dvec3 n0 = N[upper[0]];

		glm::dvec3 q, q1, n1, value1, value2;
		if (upper[1] != -1) {
			q1 = Q[upper[1]];
			n1 = N[upper[1]];
			compat_orientation_extrinsic_4(q0, n0, q1, n1, value1, value2);
			q = value1 + value2;
		}
		else {
			q = q0;
		}
		glm::dvec3 n = N_next[i];
		q -= glm::dot(n, q) * n;

		double len = q.x * q.x + q.y * q.y + q.z * q.z;
		if (len > 2.93873587705571876e-39f)
			q /= sqrt(len);
		Q_next[i] = q;
//	}
}


__global__ 
void cudaUpdatePosition(int* phase, int num_phases, glm::dvec3* N, glm::dvec3* Q, Link* adj, int* adjOffset, int num_adj, glm::dvec3* V, glm::dvec3* O, double scale) {
	int pi = blockIdx.x * blockDim.x + threadIdx.x;

//	for (int pi = 0; pi < num_phases; ++pi) {
	if (pi >= num_phases)
		return;
		int i = phase[pi];
		glm::dvec3 n_i = N[i], v_i = V[i];
		glm::dvec3 q_i = Q[i];
		glm::dvec3 sum = O[i];
		double weight_sum = 0.0f;

		for (int l = adjOffset[i]; l < adjOffset[i + 1]; ++l) {
			Link link = adj[l];
			int j = link.id;
			const double weight = link.weight;
			if (weight == 0)
				continue;

			glm::dvec3 n_j = N[j], v_j = V[j];
			glm::dvec3 q_j = Q[j], o_j = O[j];
			glm::dvec3 v1, v2;
			compat_position_extrinsic_4(
				v_i, n_i, q_i, sum, v_j, n_j, q_j, o_j, scale, v1, v2);

			sum = v1*weight_sum +v2*weight;
			weight_sum += weight;
			if (weight_sum > 2.93873587705571876e-39f)
				sum /= weight_sum;
			sum -= glm::dot(n_i, sum - v_i)*n_i;
		}

		if (weight_sum > 0) {
			O[i] = position_round_4(sum, q_i, n_i, v_i, scale);
		}
//	}
}

__global__
void cudaPropagatePositionUpper(glm::dvec3* srcField, glm::ivec2* toUpper, glm::dvec3* N, glm::dvec3* V, glm::dvec3* destField, int num_position) {
	int i = blockIdx.x * blockDim.x + threadIdx.x;
//	for (int i = 0; i < num_position; ++i) {
	if (i >= num_position)
		return;
		for (int k = 0; k < 2; ++k) {
			int dest = toUpper[i][k];
			if (dest == -1)
				continue;
			glm::dvec3 o = srcField[i], n = N[dest], v = V[dest];
			o -= n * glm::dot(n, o - v);
			destField[dest] = o;
		}
//	}
}


void UpdateOrientation(int* phase, int num_phases, glm::dvec3* N, glm::dvec3* Q, Link* adj, int* adjOffset, int num_adj) {
	cudaUpdateOrientation << <(num_phases + 255) / 256, 256 >> >(phase, num_phases, N, Q, adj, adjOffset, num_adj);
//	cudaUpdateOrientation(phase, num_phases, N, Q, adj, adjOffset, num_adj);
}

void PropagateOrientationUpper(glm::dvec3* srcField, int num_orientation, glm::ivec2* toUpper, glm::dvec3* N, glm::dvec3* destField) {
	cudaPropagateOrientationUpper << <(num_orientation + 255) / 256, 256 >> >(srcField, toUpper, N, destField, num_orientation);
//	cudaPropagateOrientationUpper(srcField, toUpper, N, destField, num_orientation);
}

void PropagateOrientationLower(glm::ivec2* toUpper, glm::dvec3* Q, glm::dvec3* N, glm::dvec3* Q_next, glm::dvec3* N_next, int num_toUpper) {
	cudaPropagateOrientationLower << <(num_toUpper + 255) / 256, 256 >> >(toUpper, Q, N, Q_next, N_next, num_toUpper);
//	cudaPropagateOrientationLower(toUpper, Q, N, Q_next, N_next, num_toUpper);
}


void UpdatePosition(int* phase, int num_phases, glm::dvec3* N, glm::dvec3* Q, Link* adj, int* adjOffset, int num_adj, glm::dvec3* V, glm::dvec3* O, double scale) {
	cudaUpdatePosition << <(num_phases + 255) / 256, 256 >> >(phase, num_phases, N, Q, adj, adjOffset, num_adj, V, O, scale);
//	cudaUpdatePosition(phase, num_phases, N, Q, adj, adjOffset, num_adj, V, O, scale);
}

void PropagatePositionUpper(glm::dvec3* srcField, int num_position, glm::ivec2* toUpper, glm::dvec3* N, glm::dvec3* V, glm::dvec3* destField) {
	cudaPropagatePositionUpper << <(num_position + 255) / 256, 256 >> >(srcField, toUpper, N, V, destField, num_position);
//	cudaPropagatePositionUpper(srcField, toUpper, N, V, destField, num_position);
}
