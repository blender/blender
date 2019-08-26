#include "merge-vertex.hpp"

#include "compare-key.hpp"

#include <map>
#include <vector>

namespace qflow {

void merge_close(MatrixXd& V, MatrixXi& F, double threshold)
{
	std::map<Key3f, int> vid_maps;
	std::vector<int> vid_compress(V.cols());
	for (int i = 0; i < V.cols(); ++i) {
		Key3f key(V(0, i), V(1, i), V(2, i), threshold);
		if (vid_maps.count(key)) {
			vid_compress[i] = vid_maps[key];
		}
		else {
			V.col(vid_maps.size()) = V.col(i);
			vid_compress[i] = vid_maps.size();
			vid_maps[key] = vid_compress[i];
		}
	}
	printf("Compress Vertex from %d to %d...\n", (int)V.cols(), (int)vid_maps.size());
	MatrixXd newV(3, vid_maps.size());
	memcpy(newV.data(), V.data(), sizeof(double) * 3 * vid_maps.size());
	V = std::move(newV);
	int f_num = 0;
	for (int i = 0; i < F.cols(); ++i) {
		for (int j = 0; j < 3; ++j) {
			F(j, f_num) = vid_compress[F(j, i)];
		}
		if (F(0, f_num) != F(1, f_num) && F(0, f_num) != F(2, f_num) && F(1, f_num) != F(2, f_num)) {
			f_num++;
		}
	}
	printf("Compress Face from %d to %d...\n", (int)F.cols(), f_num);
	MatrixXi newF(3, f_num);
	memcpy(newF.data(), F.data(), sizeof(int) * 3 * f_num);
	F = std::move(newF);
}

} // namespace qflow
