/*
    hierarchy.cpp: Code to generate unstructured multi-resolution hierarchies
    from meshes or point clouds

    This file is part of the implementation of

        Instant Field-Aligned Meshes
        Wenzel Jakob, Daniele Panozzo, Marco Tarini, and Olga Sorkine-Hornung
        In ACM Transactions on Graphics (Proc. SIGGRAPH Asia 2015)

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include "hierarchy.h"
#include "serializer.h"
#include "dedge.h"
#include "field.h"
#include <parallel_stable_sort.h>
#include <pcg32.h>

AdjacencyMatrix downsample_graph(const AdjacencyMatrix adj, const MatrixXf &V,
                                 const MatrixXf &N, const VectorXf &A,
                                 MatrixXf &V_p, MatrixXf &N_p, VectorXf &A_p,
                                 MatrixXu &to_upper, VectorXu &to_lower,
                                 bool deterministic,
                                 const ProgressCallback &progress) {
    struct Entry {
        uint32_t i, j;
        float order;
        inline Entry() { };
        inline Entry(uint32_t i, uint32_t j, float order) : i(i), j(j), order(order) { }
        inline bool operator<(const Entry &e) const { return order > e.order; }
    };

    uint32_t nLinks = adj[V.cols()] - adj[0];
    Entry *entries = new Entry[nLinks];
    Timer<> timer;
    cout << "  Collapsing .. ";
    cout.flush();

    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, (uint32_t) V.cols(), GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t> &range) {
            for (uint32_t i = range.begin(); i != range.end(); ++i) {
                uint32_t nNeighbors = adj[i + 1] - adj[i];
                uint32_t base = adj[i] - adj[0];
                for (uint32_t j = 0; j < nNeighbors; ++j) {
                    uint32_t k = adj[i][j].id;
                    Float dp = N.col(i).dot(N.col(k));
                    Float ratio = A[i]>A[k] ? (A[i]/A[k]) : (A[k]/A[i]);
                    entries[base + j] = Entry(i, k, dp * ratio);
                }
            }
            SHOW_PROGRESS_RANGE(range, V.cols(), "Downsampling graph (1/6)");
        }
    );

    if (progress)
        progress("Downsampling graph (2/6)", 0.0f);

    if (deterministic)
        pss::parallel_stable_sort(entries, entries + nLinks, std::less<Entry>());
    else
        tbb::parallel_sort(entries, entries + nLinks, std::less<Entry>());

    std::vector<bool> mergeFlag(V.cols(), false);

    uint32_t nCollapsed = 0;
    for (uint32_t i=0; i<nLinks; ++i) {
        const Entry &e = entries[i];
        if (mergeFlag[e.i] || mergeFlag[e.j])
            continue;
        mergeFlag[e.i] = mergeFlag[e.j] = true;
        entries[nCollapsed++] = entries[i];
    }
    uint32_t vertexCount = V.cols() - nCollapsed;

    /* Allocate memory for coarsened graph */
    V_p.resize(3, vertexCount);
    N_p.resize(3, vertexCount);
    A_p.resize(vertexCount);
    to_upper.resize(2, vertexCount);
    to_lower.resize(V.cols());

    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, (uint32_t) nCollapsed, GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t> &range) {
            for (uint32_t i = range.begin(); i != range.end(); ++i) {
                const Entry &e = entries[i];
                const Float area1 = A[e.i], area2 = A[e.j], surfaceArea = area1+area2;
                if (surfaceArea > RCPOVERFLOW)
                    V_p.col(i) = (V.col(e.i) * area1 + V.col(e.j) * area2) / surfaceArea;
                else
                    V_p.col(i) = (V.col(e.i) + V.col(e.j)) * 0.5f;
                Vector3f normal = N.col(e.i) * area1 + N.col(e.j) * area2;
                Float norm = normal.norm();
                N_p.col(i) = norm > RCPOVERFLOW ? Vector3f(normal / norm)
                                                : Vector3f::UnitX();
                A_p[i] = surfaceArea;
                to_upper.col(i) << e.i, e.j;
                to_lower[e.i] = i; to_lower[e.j] = i;
            }
            SHOW_PROGRESS_RANGE(range, nCollapsed, "Downsampling graph (3/6)");
        }
    );

    delete[] entries;

    std::atomic<int> offset(nCollapsed);
    tbb::blocked_range<uint32_t> range(0u, (uint32_t) V.cols(), GRAIN_SIZE);

    auto copy_uncollapsed = [&](const tbb::blocked_range<uint32_t> &range) {
        for (uint32_t i = range.begin(); i != range.end(); ++i) {
            if (!mergeFlag[i]) {
                uint32_t idx = offset++;
                V_p.col(idx) = V.col(i);
                N_p.col(idx) = N.col(i);
                A_p[idx] = A[i];
                to_upper.col(idx) << i, INVALID;
                to_lower[i] = idx;
            }
        }
        SHOW_PROGRESS_RANGE(range, V.cols(), "Downsampling graph (4/6)");
    };

    if (deterministic)
        copy_uncollapsed(range);
    else
        tbb::parallel_for(range, copy_uncollapsed);

    VectorXu neighborhoodSize(V_p.cols() + 1);

    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, (uint32_t) V_p.cols(), GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t> &range) {
            std::vector<Link> scratch;
            for (uint32_t i = range.begin(); i != range.end(); ++i) {
                scratch.clear();

                for (int j=0; j<2; ++j) {
                    uint32_t upper = to_upper(j, i);
                    if (upper == INVALID)
                        continue;
                    for (Link *link = adj[upper]; link != adj[upper+1]; ++link)
                        scratch.push_back(Link(to_lower[link->id], link->weight));
                }

                std::sort(scratch.begin(), scratch.end());
                uint32_t id = INVALID, size = 0;
                for (const auto &link : scratch) {
                    if (id != link.id && link.id != i) {
                        id = link.id;
                        ++size;
                    }
                }
                neighborhoodSize[i+1] = size;
            }
            SHOW_PROGRESS_RANGE(range, V_p.cols(), "Downsampling graph (5/6)");
        }
    );

    neighborhoodSize[0] = 0;
    for (uint32_t i=0; i<neighborhoodSize.size()-1; ++i)
        neighborhoodSize[i+1] += neighborhoodSize[i];

    uint32_t nLinks_p = neighborhoodSize[neighborhoodSize.size()-1];
    AdjacencyMatrix adj_p = new Link*[V_p.size() + 1];
    Link *links = new Link[nLinks_p];
    for (uint32_t i=0; i<neighborhoodSize.size(); ++i)
        adj_p[i] = links + neighborhoodSize[i];

    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, (uint32_t) V_p.cols(), GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t> &range) {
            std::vector<Link> scratch;
            for (uint32_t i = range.begin(); i != range.end(); ++i) {
                scratch.clear();

                for (int j=0; j<2; ++j) {
                    uint32_t upper = to_upper(j, i);
                    if (upper == INVALID)
                        continue;
                    for (Link *link = adj[upper]; link != adj[upper+1]; ++link)
                        scratch.push_back(Link(to_lower[link->id], link->weight));
                }
                std::sort(scratch.begin(), scratch.end());
                Link *dest = adj_p[i];
                uint32_t id = INVALID;
                for (const auto &link : scratch) {
                    if (link.id != i) {
                        if (id != link.id) {
                            *dest++ = link;
                            id = link.id;
                        } else {
                            dest[-1].weight += link.weight;
                        }
                    }
                }
            }
            SHOW_PROGRESS_RANGE(range, V_p.cols(), "Downsampling graph (6/6)");
        }
    );
    cout << "done. (" << V.cols() << " -> " << V_p.cols() << " vertices, took "
         << timeString(timer.value()) << ")" << endl;
    return adj_p;
}

void generate_graph_coloring_deterministic(const AdjacencyMatrix &adj, uint32_t size,
                             std::vector<std::vector<uint32_t> > &phases,
                             const ProgressCallback &progress) {
    if (progress)
        progress("Graph coloring", 0.0f);
    phases.clear();
    cout << "    Coloring .. ";
    cout.flush();
    Timer<> timer;

    std::vector<uint32_t> perm(size);
    for (uint32_t i=0; i<size; ++i)
        perm[i] = i;
    pcg32 rng;
    rng.shuffle(perm.begin(), perm.end());

    std::vector<int> color(size, -1);
    std::vector<uint8_t> possible_colors;
    std::vector<int> size_per_color;
    int ncolors = 0;

    for (uint32_t i=0; i<size; ++i) {
        uint32_t ip = perm[i];
        SHOW_PROGRESS(i, size, "Graph coloring");

        std::fill(possible_colors.begin(), possible_colors.end(), 1);

        for (const Link *link = adj[ip]; link != adj[ip+1]; ++link) {
            int c = color[link->id];
            if (c >= 0)
                possible_colors[c] = 0;
        }

        int chosen_color = -1;
        for (uint32_t j=0; j<possible_colors.size(); ++j) {
            if (possible_colors[j]) {
                chosen_color = j;
                break;
            }
        }

        if (chosen_color < 0) {
            chosen_color = ncolors++;
            possible_colors.resize(ncolors);
            size_per_color.push_back(0);
        }

        color[ip] = chosen_color;
        size_per_color[chosen_color]++;
    }
    phases.resize(ncolors);
    for (int i=0; i<ncolors; ++i)
        phases[i].reserve(size_per_color[i]);
    for (uint32_t i=0; i<size; ++i)
        phases[color[i]].push_back(i);

    cout << "done. (" << phases.size() << " colors, took "
         << timeString(timer.value()) << ")" << endl;
}

void generate_graph_coloring(const AdjacencyMatrix &adj, uint32_t size,
                             std::vector<std::vector<uint32_t> > &phases,
                             const ProgressCallback &progress) {
    struct ColorData {
        uint8_t nColors;
        uint32_t nNodes[256];
        ColorData() : nColors(0) { }
    };

    const uint8_t INVALID_COLOR = 0xFF;
    if (progress)
        progress("Graph coloring", 0.0f);
    phases.clear();
    cout << "    Coloring .. ";
    cout.flush();

    Timer<> timer;

    /* Generate a permutation */
    std::vector<uint32_t> perm(size);
    std::vector<tbb::spin_mutex> mutex(size);
    for (uint32_t i=0; i<size; ++i)
        perm[i] = i;

    tbb::parallel_for(
        tbb::blocked_range<uint32_t>(0u, size, GRAIN_SIZE),
        [&](const tbb::blocked_range<uint32_t> &range) {
            pcg32 rng;
            rng.advance(range.begin());
            for (uint32_t i = range.begin(); i != range.end(); ++i) {
                uint32_t j = i, k =
                    rng.nextUInt(size - i) + i;
                if (j == k)
                    continue;
                if (j > k)
                    std::swap(j, k);
                tbb::spin_mutex::scoped_lock l0(mutex[j]);
                tbb::spin_mutex::scoped_lock l1(mutex[k]);
                std::swap(perm[j], perm[k]);
            }
        }
    );

    std::vector<uint8_t> color(size, INVALID_COLOR);
    ColorData colorData = tbb::parallel_reduce(
        tbb::blocked_range<uint32_t>(0u, size, GRAIN_SIZE),
        ColorData(),
        [&](const tbb::blocked_range<uint32_t> &range, ColorData colorData) -> ColorData {
            std::vector<uint32_t> neighborhood;
            bool possible_colors[256];

            for (uint32_t pidx = range.begin(); pidx != range.end(); ++pidx) {
                uint32_t i = perm[pidx];

                neighborhood.clear();
                neighborhood.push_back(i);
                for (const Link *link = adj[i]; link != adj[i+1]; ++link)
                    neighborhood.push_back(link->id);
                std::sort(neighborhood.begin(), neighborhood.end());
                for (uint32_t j: neighborhood)
                    mutex[j].lock();

                std::fill(possible_colors, possible_colors + colorData.nColors, true);

                for (const Link *link = adj[i]; link != adj[i+1]; ++link) {
                    uint8_t c = color[link->id];
                    if (c != INVALID_COLOR) {
                        while (c >= colorData.nColors) {
                            possible_colors[colorData.nColors] = true;
                            colorData.nNodes[colorData.nColors] = 0;
                            colorData.nColors++;
                        }
                        possible_colors[c] = false;
                    }
                }

                uint8_t chosen_color = INVALID_COLOR;
                for (uint8_t j=0; j<colorData.nColors; ++j) {
                    if (possible_colors[j]) {
                        chosen_color = j;
                        break;
                    }
                }
                if (chosen_color == INVALID_COLOR) {
                    if (colorData.nColors == INVALID_COLOR-1)
                        throw std::runtime_error("Ran out of colors during graph coloring! "
                            "The input mesh is very likely corrupt.");
                    colorData.nNodes[colorData.nColors] = 1;
                    color[i] = colorData.nColors++;
                } else {
                    colorData.nNodes[chosen_color]++;
                    color[i] = chosen_color;
                }

                for (uint32_t j: neighborhood)
                    mutex[j].unlock();
            }
            SHOW_PROGRESS_RANGE(range, size, "Graph coloring");
            return colorData;
        },
        [](ColorData c1, ColorData c2) -> ColorData {
            ColorData result;
            result.nColors = std::max(c1.nColors, c2.nColors);
            memset(result.nNodes, 0, sizeof(uint32_t) * result.nColors);
            for (uint8_t i=0; i<c1.nColors; ++i)
                result.nNodes[i] += c1.nNodes[i];
            for (uint8_t i=0; i<c2.nColors; ++i)
                result.nNodes[i] += c2.nNodes[i];
            return result;
        }
    );

    phases.resize(colorData.nColors);
    for (int i=0; i<colorData.nColors; ++i)
        phases[i].reserve(colorData.nNodes[i]);

    for (uint32_t i=0; i<size; ++i)
        phases[color[i]].push_back(i);

    cout << "done. (" << phases.size() << " colors, took "
         << timeString(timer.value()) << ")" << endl;
}

MultiResolutionHierarchy::MultiResolutionHierarchy() {
    if (sizeof(Link) != 12)
        throw std::runtime_error("Adjacency matrix entries are not packed! Investigate compiler settings.");
    mA.reserve(MAX_DEPTH+1);
    mV.reserve(MAX_DEPTH+1);
    mN.reserve(MAX_DEPTH+1);
    mQ.reserve(MAX_DEPTH+1);
    mO.reserve(MAX_DEPTH+1);
    mCQ.reserve(MAX_DEPTH+1);
    mCQw.reserve(MAX_DEPTH+1);
    mCO.reserve(MAX_DEPTH+1);
    mCOw.reserve(MAX_DEPTH+1);
    mAdj.reserve(MAX_DEPTH+1);
    mToUpper.reserve(MAX_DEPTH);
    mToLower.reserve(MAX_DEPTH);
    mIterationsQ = mIterationsO = -1;
    mScale = 0;
    mTotalSize = 0;
    mFrozenO = mFrozenQ = false;
}

void MultiResolutionHierarchy::build(bool deterministic, const ProgressCallback &progress) {
    std::vector<std::vector<uint32_t>> phases;
    cout << "Processing level 0 .." << endl;
    if (deterministic)
        generate_graph_coloring_deterministic(mAdj[0], mV[0].cols(), phases, progress);
    else
        generate_graph_coloring(mAdj[0], mV[0].cols(), phases, progress);
    mPhases.push_back(phases);
    
    mTotalSize = mV[0].cols();
    mCO.push_back(MatrixXf());
    mCOw.push_back(VectorXf());
    mCQ.push_back(MatrixXf());
    mCQw.push_back(VectorXf());

    cout << "Building multiresolution hierarchy .." << endl;
    Timer<> timer;
    for (int i=0; i<MAX_DEPTH; ++i) {
        std::vector<std::vector<uint32_t>> phases_p;
        MatrixXf N_p, V_p;
        VectorXf A_p;
        MatrixXu toUpper;
        VectorXu toLower;

        AdjacencyMatrix adj_p =
            downsample_graph(mAdj[i], mV[i], mN[i], mA[i], V_p, N_p, A_p,
                             toUpper, toLower, deterministic, progress);

        if (deterministic)
            generate_graph_coloring_deterministic(adj_p, V_p.cols(), phases_p, progress);
        else
            generate_graph_coloring(adj_p, V_p.cols(), phases_p, progress);

        mTotalSize += V_p.cols();
        mPhases.push_back(std::move(phases_p));
        mAdj.push_back(std::move(adj_p));
        mV.push_back(std::move(V_p));
        mN.push_back(std::move(N_p));
        mA.push_back(std::move(A_p));
        mToUpper.push_back(std::move(toUpper));
        mToLower.push_back(std::move(toLower));
        mCO.push_back(MatrixXf());
        mCOw.push_back(VectorXf());
        mCQ.push_back(MatrixXf());
        mCQw.push_back(VectorXf());
        if (mV[mV.size()-1].cols() == 1)
            break;
    }
    mIterationsQ = mIterationsO = -1;
    mFrozenO = mFrozenQ = false;
    cout << "Hierarchy construction took " << timeString(timer.value()) << "." << endl;
}

void init_random_tangent(const MatrixXf &N, MatrixXf &Q) {
    Q.resize(N.rows(), N.cols());
    tbb::parallel_for(tbb::blocked_range<uint32_t>(0u, (uint32_t) N.cols()),
        [&](const tbb::blocked_range<uint32_t> &range) {
            pcg32 rng;
            rng.advance(range.begin());
            for (uint32_t i = range.begin(); i != range.end(); ++i) {
                Vector3f s, t;
                coordinate_system(N.col(i), s, t);
                float angle = rng.nextFloat() * 2 * M_PI;
                Q.col(i) = s * std::cos(angle) + t * std::sin(angle);
            }
        }
    );
}

void init_random_position(const MatrixXf &P, const MatrixXf &N, MatrixXf &O, Float scale) {
    O.resize(N.rows(), N.cols());
    tbb::parallel_for(tbb::blocked_range<uint32_t>(0u, (uint32_t) N.cols()),
            [&](const tbb::blocked_range<uint32_t> &range) {
            pcg32 rng;
            rng.advance(2*range.begin());
            for (uint32_t i = range.begin(); i != range.end(); ++i) {
                Vector3f s, t;
                coordinate_system(N.col(i), s, t);
                float x = rng.nextFloat() * 2.f - 1.f,
                      y = rng.nextFloat() * 2.f - 1.f;
                O.col(i) = P.col(i) + (s*x + t*y)*scale;
            }
        }
    );
}

void MultiResolutionHierarchy::resetSolution() {
    cout << "Setting to random solution .. ";
    cout.flush();
    Timer<> timer;
    if (mQ.size() != mV.size()) {
        mQ.resize(mV.size());
        mO.resize(mV.size());
    }
    for (size_t i=0; i<mV.size(); ++i) {
        init_random_tangent(mN[i], mQ[i]);
        init_random_position(mV[i], mN[i], mO[i], mScale);
    }
    mFrozenO = mFrozenQ = false;
    cout << "done. (took " << timeString(timer.value()) << ")" << endl;
}

void MultiResolutionHierarchy::free() {
    for (size_t i=0; i<mAdj.size(); ++i) {
        delete[] mAdj[i][0];
        delete[] mAdj[i];
    }
    mAdj.clear(); mV.clear(); mQ.clear();
    mO.clear(); mN.clear(); mA.clear();
    mCQ.clear(); mCO.clear();
    mCQw.clear(); mCOw.clear();
    mToUpper.clear(); mToLower.clear();
    mPhases.clear();
    mF.resize(0, 0);
    mE2E.resize(0);
    mTotalSize = 0;
}

void MultiResolutionHierarchy::save(Serializer &serializer) {
    serializer.set("levels", (uint32_t) mV.size());
    serializer.set("iterations_O", mIterationsO);
    serializer.set("iterations_Q", mIterationsQ);
    serializer.set("totalSize", mTotalSize);
    serializer.set("scale", mScale);
    serializer.set("F", mF);
    serializer.set("E2E", mE2E);
    serializer.set("frozenO", mFrozenO);
    serializer.set("frozenQ", mFrozenQ);
    for (uint32_t i=0; i<mV.size(); ++i) {
        serializer.pushPrefix(std::to_string(i));
        serializer.set("V", mV[i]);
        serializer.set("N", mN[i]);
        serializer.set("A", mA[i]);
        serializer.set("O", mO[i]);
        serializer.set("Q", mQ[i]);
        if (i < mV.size() - 1) {
            serializer.set("toLower", mToLower[i]);
            serializer.set("toUpper", mToUpper[i]);
        }
        serializer.set("CQ", mCQ[i]);
        serializer.set("CQw", mCQw[i]);
        serializer.set("CO", mCO[i]);
        serializer.set("COw", mCOw[i]);
        const AdjacencyMatrix &adj = mAdj[i];
        std::vector<std::vector<uint32_t>> link_id(size(i));
        std::vector<std::vector<uint32_t>> link_ivar(size(i));
        std::vector<std::vector<Float>> link_weight(size(i));
        for (uint32_t j=0; j<size(i); ++j) {
            size_t size = adj[j+1] - adj[j];
            link_id[j].reserve(size);
            link_weight[j].reserve(size);
            link_ivar[j].reserve(size);
            for (const Link *link = adj[j]; link != adj[j+1]; ++link) {
                link_id[j].push_back(link->id);
                link_weight[j].push_back(link->weight);
                link_ivar[j].push_back(link->ivar_uint32);
            }
        }
        serializer.set("phases", mPhases[i]);
        serializer.set("adj_id", link_id);
        serializer.set("adj_ivar", link_ivar);
        serializer.set("adj_weight", link_weight);
        serializer.popPrefix();
    }
}

void MultiResolutionHierarchy::load(const Serializer &serializer) {
    free();
    uint32_t levels;
    serializer.get("levels", levels);
    serializer.get("iterations_O", mIterationsO);
    serializer.get("iterations_Q", mIterationsQ);
    serializer.get("totalSize", mTotalSize);
    serializer.get("scale", mScale);
    serializer.get("F", mF);
    serializer.get("E2E", mE2E);
    serializer.get("frozenO", mFrozenO);
    serializer.get("frozenQ", mFrozenQ);
    for (uint32_t i=0; i<levels; ++i) {
        serializer.pushPrefix(std::to_string(i));
        MatrixXf V, N, O, Q, CQ, CO;
        VectorXf A, CQw, COw;
        std::vector<std::vector<uint32_t>> phases;

        serializer.get("V", V);
        serializer.get("N", N);
        serializer.get("A", A);
        serializer.get("O", O);
        serializer.get("Q", Q);
        serializer.get("phases", phases);

        serializer.get("CQ", CQ);
        serializer.get("CQw", CQw);
        serializer.get("CO", CO);
        serializer.get("COw", COw);

        mV.push_back(std::move(V));
        mN.push_back(std::move(N));
        mA.push_back(std::move(A));
        mO.push_back(std::move(O));
        mQ.push_back(std::move(Q));
        mCQ.push_back(std::move(CQ));
        mCO.push_back(std::move(CO));
        mCQw.push_back(std::move(CQw));
        mCOw.push_back(std::move(COw));
        mPhases.push_back(std::move(phases));

        if (i < levels - 1) {
            MatrixXu toUpper;
            VectorXu toLower;
            serializer.get("toLower", toLower);
            serializer.get("toUpper", toUpper);
            mToLower.push_back(std::move(toLower));
            mToUpper.push_back(std::move(toUpper));
        }

        std::vector<std::vector<uint32_t>> adj_id;
        std::vector<std::vector<uint32_t>> adj_ivar;
        std::vector<std::vector<Float>> adj_weight;
        serializer.get("adj_id", adj_id);
        serializer.get("adj_ivar", adj_ivar);
        serializer.get("adj_weight", adj_weight);

        if (adj_id.size() != adj_ivar.size() || adj_ivar.size() != adj_weight.size())
            throw std::runtime_error("Could not unserialize data");
        uint32_t linkCount = 0;

        for (uint32_t j=0; j<adj_id.size(); ++j) {
            if (adj_id[j].size() != adj_ivar[j].size() || adj_ivar[j].size() != adj_weight[j].size())
                throw std::runtime_error("Could not unserialize data");
            linkCount += adj_id[j].size();
        }

        AdjacencyMatrix adj = new Link*[adj_id.size() + 1];
        adj[0] = new Link[linkCount];
        for (uint32_t j=0; j<adj_id.size(); ++j)
            adj[j+1] = adj[j] + adj_id[j].size();
        for (uint32_t j=0; j<adj_id.size(); ++j) {
            for (uint32_t k=0; k<adj_id[j].size(); ++k) {
                adj[j][k].id = adj_id[j][k];
                adj[j][k].ivar_uint32 = adj_ivar[j][k];
                adj[j][k].weight = adj_weight[j][k];
            }
        }
        mAdj.push_back(adj);
        serializer.popPrefix();
    }
}

void MultiResolutionHierarchy::clearConstraints() {
    if (levels() == 0)
        return;
    if (mCQ[0].size() == 0)
        cout << "Allocating memory for constraints .." << endl;
    for (int i=0; i<levels(); ++i) {
        mCQ[i].resize(3, size(i));
        mCO[i].resize(3, size(i));
        mCQw[i].resize(size(i));
        mCOw[i].resize(size(i));
        mCQw[i].setZero();
        mCOw[i].setZero();
    }
}

void MultiResolutionHierarchy::propagateSolution(int rosy) {
    auto compat_orient = compat_orientation_extrinsic_2;
    if (rosy == 2)
        ;
    else if (rosy == 4)
        compat_orient = compat_orientation_extrinsic_4;
    else if (rosy == 6)
        compat_orient = compat_orientation_extrinsic_6;
    else
        throw std::runtime_error("Unsupported symmetry!");

    cout << "Propagating updated solution.. ";
    cout.flush();
    Timer<> timer;
    for (int l=0; l<levels()-1; ++l)  {
        const MatrixXf &N = mN[l];
        const MatrixXf &N_next = mN[l+1];
        const MatrixXf &Q = mQ[l];
        MatrixXf &Q_next = mQ[l+1];

        tbb::parallel_for(
            tbb::blocked_range<uint32_t>(0u, size(l+1), GRAIN_SIZE),
            [&](const tbb::blocked_range<uint32_t> &range) {
                for (uint32_t i=range.begin(); i != range.end(); ++i) {
                    Vector2u upper = toUpper(l).col(i);
                    Vector3f q0 = Q.col(upper[0]);
                    Vector3f n0 = N.col(upper[0]);
                    Vector3f q;

                    if (upper[1] != INVALID) {
                        Vector3f q1 = Q.col(upper[1]);
                        Vector3f n1 = N.col(upper[1]);
                        auto result = compat_orient(q0, n0, q1, n1);
                        q = result.first + result.second;
                    } else {
                        q = q0;
                    }
                    Vector3f n = N_next.col(i);
                    q -= n.dot(q) * n;
                    if (q.squaredNorm() > RCPOVERFLOW)
                        q.normalize();

                    Q_next.col(i) = q;
                }
            }
        );
    }
    cout << "done. (took " << timeString(timer.value()) << ")" << endl;
}

void MultiResolutionHierarchy::propagateConstraints(int rosy, int posy) {
    if (levels() == 0)
        return;
    cout << "Propagating constraints .. ";
    cout.flush();
    Timer<> timer;

    auto compat_orient = compat_orientation_extrinsic_2;
    if (rosy == 2)
        ;
    else if (rosy == 4)
        compat_orient = compat_orientation_extrinsic_4;
    else if (rosy == 6)
        compat_orient = compat_orientation_extrinsic_6;
    else
        throw std::runtime_error("Unsupported symmetry!");

    auto compat_pos = compat_position_extrinsic_4;
    if (posy == 4)
        ;
    else if (posy == 3)
        compat_pos = compat_position_extrinsic_3;
    else
        throw std::runtime_error("Unsupported symmetry!");

    Float scale = mScale, inv_scale = 1/mScale;

    for (int l=0; l<levels()-1; ++l)  {
        const MatrixXf &N = mN[l];
        const MatrixXf &N_next = mN[l+1];
        const MatrixXf &V = mV[l];
        const MatrixXf &V_next = mV[l+1];
        const MatrixXf &CQ = mCQ[l];
        MatrixXf &CQ_next = mCQ[l+1];
        const VectorXf &CQw = mCQw[l];
        VectorXf &CQw_next = mCQw[l+1];
        const MatrixXf &CO = mCO[l];
        MatrixXf &CO_next = mCO[l+1];
        const VectorXf &COw = mCOw[l];
        VectorXf &COw_next = mCOw[l+1];

        tbb::parallel_for(
            tbb::blocked_range<uint32_t>(0u, size(l+1), GRAIN_SIZE),
            [&](const tbb::blocked_range<uint32_t> &range) {
                for (uint32_t i=range.begin(); i != range.end(); ++i) {
                    Vector2u upper = toUpper(l).col(i);
                    Vector3f cq = Vector3f::Zero(), co = Vector3f::Zero();
                    Float cqw = 0.0f, cow = 0.0f;

                    bool has_cq0 = CQw[upper[0]] != 0;
                    bool has_cq1 = upper[1] != INVALID && CQw[upper[1]] != 0;
                    bool has_co0 = COw[upper[0]] != 0;
                    bool has_co1 = upper[1] != INVALID && COw[upper[1]] != 0;

                    if (has_cq0 && !has_cq1) {
                        cq = CQ.col(upper[0]);
                        cqw = CQw[upper[0]];
                    } else if (has_cq1 && !has_cq0) {
                        cq = CQ.col(upper[1]);
                        cqw = CQw[upper[1]];
                    } else if (has_cq1 && has_cq0) {
                        auto result = compat_orient(CQ.col(upper[0]), N.col(upper[0]), CQ.col(upper[1]), N.col(upper[1]));
                        cq = result.first * CQw[upper[0]] + result.second * CQw[upper[1]];
                        cqw = (CQw[upper[0]] + CQw[upper[1]]);
                    }
                    if (cq != Vector3f::Zero()) {
                        Vector3f n = N_next.col(i);
                        cq -= n.dot(cq) * n;
                        if (cq.squaredNorm() > RCPOVERFLOW)
                            cq.normalize();
                    }

                    if (has_co0 && !has_co1) {
                        co = CO.col(upper[0]);
                        cow = COw[upper[0]];
                    } else if (has_co1 && !has_co0) {
                        co = CO.col(upper[1]);
                        cow = COw[upper[1]];
                    } else if (has_co1 && has_co0) {
                        auto result = compat_pos(
                            V.col(upper[0]), N.col(upper[0]), CQ.col(upper[0]), CO.col(upper[0]), 
                            V.col(upper[1]), N.col(upper[1]), CQ.col(upper[1]), CO.col(upper[1]),
                            scale, inv_scale
                        );
                        cow = COw[upper[0]] + COw[upper[1]];
                        co = (result.first * COw[upper[0]] + result.second * COw[upper[1]]) / cow;
                    }
                    if (co != Vector3f::Zero()) {
                        Vector3f n = N_next.col(i), v = V_next.col(i);
                        co -= n.dot(cq - v) * n;
                    }
                    #if 0
                        cqw *= 0.5f;
                        cow *= 0.5f;
                    #else
                        if (cqw > 0)
                            cqw = 1;
                        if (cow > 0)
                            cow = 1;
                    #endif

                    CQw_next[i] = cqw;
                    COw_next[i] = cow;
                    CQ_next.col(i) = cq;
                    CO_next.col(i) = co;
                }
            }
        );
    }
    cout << "done. (took " << timeString(timer.value()) << ")" << endl;
}

void MultiResolutionHierarchy::printStatistics() const {
    if (levels() == 0)
        return;
    std::ostringstream oss;
    size_t field_s = 0, V_s = 0, N_s = 0, A_s = 0, adj_s = 0, tree_s = 0,
           phases_s = 0, cedge_s = 0, cvertex_s = 0;
    for (int i=0; i<levels(); ++i) {
        field_s += sizeInBytes(mQ[i]) + sizeInBytes(mO[i]);
        V_s += sizeInBytes(mV[i]);
        N_s += sizeInBytes(mN[i]);
        A_s += sizeInBytes(mA[i]);
        adj_s += (mAdj[i][mV[i].cols()] - mAdj[i][0]) * sizeof(Link) + mV[i].cols() * sizeof(Link *);
        phases_s += mPhases[i].size() * sizeof(std::vector<uint32_t>) + mV[i].cols() * sizeof(uint32_t);
    }
    for (int i=0; i<levels()-1; ++i) {
        tree_s += sizeInBytes(mToUpper[i]);
        tree_s += sizeInBytes(mToLower[i]);
    }
    cvertex_s = sizeInBytes(mF);
    cedge_s = sizeInBytes(mE2E);

    cout << endl;
    cout << "Multiresolution hierarchy statistics:" << endl;
    cout << "    Field data          : " << memString(field_s) << endl;
    cout << "    Vertex data         : " << memString(V_s + N_s + A_s) << " (level 0: "
        << memString(sizeInBytes(mV[0]) + sizeInBytes(mN[0]) + sizeInBytes(mA[0])) << ")" << endl;
    cout << "    Adjacency matrices  : " << memString(adj_s) << " (level 0: "
        << memString((mAdj[0][mV[0].cols()] - mAdj[0][0]) * sizeof(Link)) << ")" << endl;
    cout << "    Tree connectivity   : " << memString(tree_s) << endl;
    cout << "    Vertex indices      : " << memString(cvertex_s) << endl;
    cout << "    Edge connectivity   : " << memString(cedge_s) << endl;
    cout << "    Parallel phases     : " << memString(phases_s) << endl;
    cout << "    Total               : "
         << memString(field_s + V_s + N_s + A_s + adj_s + tree_s + cedge_s + cvertex_s + phases_s) << endl;
}
