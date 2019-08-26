#include "config.hpp"
#include "adjacent-matrix.hpp"
#include "dedge.hpp"
#include <fstream>

namespace qflow {

void generate_adjacency_matrix_uniform(
	const MatrixXi &F, const VectorXi &V2E, const VectorXi &E2E,
	const VectorXi &nonManifold, AdjacentMatrix& adj) {
	adj.resize(V2E.size());
#ifdef WITH_OMP
#pragma omp parallel for
#endif
	for (int i = 0; i < adj.size(); ++i) {
		int start = V2E[i];
		int edge = start;
		if (start == -1)
			continue;
		do {
			int base = edge % 3, f = edge / 3;
			int opp = E2E[edge], next = dedge_next_3(opp);
			if (adj[i].empty())
				adj[i].push_back(Link(F((base + 2) % 3, f)));
			if (opp == -1 || next != start) {
				adj[i].push_back(Link(F((base + 1) % 3, f)));
				if (opp == -1)
					break;
			}
			edge = next;
		} while (edge != start);
	}
}

} // namespace qflow
