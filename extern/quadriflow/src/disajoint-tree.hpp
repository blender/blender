#ifndef DISAJOINT_TREE_H_
#define DISAJOINT_TREE_H_

#include <vector>

namespace qflow {

class DisajointTree {
   public:
    DisajointTree() {}
    DisajointTree(int n) {
        parent.resize(n);
        rank.resize(n, 1);
        for (int i = 0; i < n; ++i) parent[i] = i;
    }
    int Parent(int x) {
        if (x == parent[x]) return x;
        int y = Parent(parent[x]);
        parent[x] = y;
        return y;
    }
    int Index(int x) { return indices[x]; }
    int IndexToParent(int x) {return indices_to_parent[x]; };
    void MergeFromTo(int x, int y) {
        int px = Parent(x);
        int py = Parent(y);
        if (px == py) return;
        rank[py] += rank[px];
        parent[px] = py;
    }
    void Merge(int x, int y) {
        int px = Parent(x);
        int py = Parent(y);
        if (px == py) return;
        if (rank[px] < rank[py]) {
            rank[py] += rank[px];
            parent[px] = py;
        } else {
            rank[px] += rank[py];
            parent[py] = px;
        }
    }

    // renumber the root so that it is consecutive.
    void BuildCompactParent() {
        std::vector<int> compact_parent;
        compact_parent.resize(parent.size());
        compact_num = 0;
        for (int i = 0; i < parent.size(); ++i) {
            if (parent[i] == i) {
                compact_parent[i] = compact_num++;
                indices_to_parent.push_back(i);
            }
        }
        indices.resize(parent.size());
        for (int i = 0; i < parent.size(); ++i) {
            indices[i] = compact_parent[Parent(i)];
        }
    }

    int CompactNum() { return compact_num; }

    int compact_num;
    std::vector<int> parent;
    std::vector<int> indices, indices_to_parent;
    std::vector<int> rank;
};

class DisajointOrientTree {
   public:
    DisajointOrientTree() {}
    DisajointOrientTree(int n) {
        parent.resize(n);
        rank.resize(n, 1);
        for (int i = 0; i < n; ++i) parent[i] = std::make_pair(i, 0);
    }
    int Parent(int j) {
        if (j == parent[j].first) return j;
        int k = Parent(parent[j].first);
        parent[j].second = (parent[j].second + parent[parent[j].first].second) % 4;
        parent[j].first = k;
        return k;
    }
    int Orient(int j) {
        if (j == parent[j].first) return parent[j].second;
        return (parent[j].second + Orient(parent[j].first)) % 4;
    }
    int Index(int x) { return indices[x]; }
    void MergeFromTo(int v0, int v1, int orient0, int orient1) {
        int p0 = Parent(v0);
        int p1 = Parent(v1);
        if (p0 == p1) return;
        int orientp0 = Orient(v0);
        int orientp1 = Orient(v1);

        if (p0 == p1) {
            return;
        }
        rank[p1] += rank[p0];
        parent[p0].first = p1;
        parent[p0].second = (orient0 - orient1 + orientp1 - orientp0 + 8) % 4;
    }

    void Merge(int v0, int v1, int orient0, int orient1) {
        int p0 = Parent(v0);
        int p1 = Parent(v1);
        if (p0 == p1) {
            return;
        }
        int orientp0 = Orient(v0);
        int orientp1 = Orient(v1);

        if (p0 == p1) {
            return;
        }
        if (rank[p1] < rank[p0]) {
            rank[p0] += rank[p1];
            parent[p1].first = p0;
            parent[p1].second = (orient1 - orient0 + orientp0 - orientp1 + 8) % 4;
        } else {
            rank[p1] += rank[p0];
            parent[p0].first = p1;
            parent[p0].second = (orient0 - orient1 + orientp1 - orientp0 + 8) % 4;
        }
    }
    void BuildCompactParent() {
        std::vector<int> compact_parent;
        compact_parent.resize(parent.size());
        compact_num = 0;
        for (int i = 0; i < parent.size(); ++i) {
            if (parent[i].first == i) {
                compact_parent[i] = compact_num++;
            }
        }
        indices.resize(parent.size());
        for (int i = 0; i < parent.size(); ++i) {
            indices[i] = compact_parent[Parent(i)];
        }
    }

    int CompactNum() { return compact_num; }

    int compact_num;
    std::vector<std::pair<int, int>> parent;
    std::vector<int> indices;
    std::vector<int> rank;
};

} // namespace qflow

#endif
