/* SPDX-FileCopyrightText: 2011 Morten S. Mikkelsen
 * SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

/** \file
 * \ingroup mikktspace
 */

#include <algorithm>
#include <cassert>
#include <unordered_map>

#ifdef WITH_TBB
#  include <tbb/parallel_for.h>
#endif

#include "mikk_atomic_hash_set.hh"
#include "mikk_float3.hh"
#include "mikk_util.hh"

namespace mikk {

static constexpr uint UNSET_ENTRY = 0xffffffffu;

template<typename Mesh> class Mikktspace {
  struct Triangle {
    /* Stores neighboring triangle for group assignment. */
    std::array<uint, 3> neighbor;
    /* Stores assigned group of each vertex. */
    std::array<uint, 3> group;
    /* Stores vertex indices that make up the triangle. */
    std::array<uint, 3> vertices;

    /* Computed face tangent, will be accumulated into group. */
    float3 tangent;

    /* Index of the face that this triangle belongs to. */
    uint faceIdx;
    /* Index of the first of this triangle's vertices' TSpaces. */
    uint tSpaceIdx;

    /* Stores mapping from this triangle's vertices to the original
     * face's vertices (relevant for quads). */
    std::array<uint8_t, 3> faceVertex;

    // flags
    bool markDegenerate : 1;
    bool quadOneDegenTri : 1;
    bool groupWithAny : 1;
    bool orientPreserving : 1;

    Triangle(uint faceIdx_, uint tSpaceIdx_)
        : tangent{0.0f},
          faceIdx{faceIdx_},
          tSpaceIdx{tSpaceIdx_},
          markDegenerate{false},
          quadOneDegenTri{false},
          groupWithAny{true},
          orientPreserving{false}
    {
      neighbor.fill(UNSET_ENTRY);
      group.fill(UNSET_ENTRY);
    }

    void setVertices(uint8_t i0, uint8_t i1, uint8_t i2)
    {
      faceVertex[0] = i0;
      faceVertex[1] = i1;
      faceVertex[2] = i2;
      vertices[0] = pack_index(faceIdx, i0);
      vertices[1] = pack_index(faceIdx, i1);
      vertices[2] = pack_index(faceIdx, i2);
    }
  };

  struct Group {
    float3 tangent;
    uint vertexRepresentative;
    bool orientPreserving;

    Group(uint vertexRepresentative_, bool orientPreserving_)
        : tangent{0.0f},
          vertexRepresentative{vertexRepresentative_},
          orientPreserving{orientPreserving_}
    {
    }

    void normalizeTSpace()
    {
      tangent = tangent.normalize();
    }

    void accumulateTSpaceAtomic(float3 v_tangent)
    {
      float_add_atomic(&tangent.x, v_tangent.x);
      float_add_atomic(&tangent.y, v_tangent.y);
      float_add_atomic(&tangent.z, v_tangent.z);
    }

    void accumulateTSpace(float3 v_tangent)
    {
      tangent += v_tangent;
    }
  };

  struct TSpace {
    float3 tangent = float3(1.0f, 0.0f, 0.0f);
    uint counter = 0;
    bool orientPreserving = false;

    void accumulateGroup(const Group &group)
    {
      assert(counter < 2);

      if (counter == 0) {
        tangent = group.tangent;
      }
      else if (tangent == group.tangent) {
        // this if is important. Due to floating point precision
        // averaging when ts0==ts1 will cause a slight difference
        // which results in tangent space splits later on, so do nothing
      }
      else {
        tangent = (tangent + group.tangent).normalize();
      }

      counter++;
      orientPreserving = group.orientPreserving;
    }
  };

  Mesh &mesh;

  std::vector<Triangle> triangles;
  std::vector<TSpace> tSpaces;
  std::vector<Group> groups;

  uint nrTSpaces, nrFaces, nrTriangles, totalTriangles;

  int nrThreads;
  bool isParallel;

 public:
  Mikktspace(Mesh &mesh_) : mesh(mesh_) {}

  void genTangSpace()
  {
    nrFaces = uint(mesh.GetNumFaces());

#ifdef WITH_TBB
    nrThreads = tbb::this_task_arena::max_concurrency();
    isParallel = (nrThreads > 1) && (nrFaces > 10000);
#else
    nrThreads = 1;
    isParallel = false;
#endif

    // make an initial triangle --> face index list
    generateInitialVerticesIndexList();

    if (nrTriangles == 0) {
      return;
    }

    // make a welded index list of identical positions and attributes (pos, norm, texc)
    generateSharedVerticesIndexList();

    // mark all triangle pairs that belong to a quad with only one
    // good triangle. These need special treatment in degenEpilogue().
    // Additionally, move all good triangles to the start of
    // triangles[] without changing order and
    // put the degenerate triangles last.
    degenPrologue();

    if (nrTriangles == 0) {
      // No point in building tangents if there are no non-degenerate triangles, so just zero them
      tSpaces.resize(nrTSpaces);
    }
    else {
      // evaluate triangle level attributes and neighbor list
      initTriangle();

      // match up edge pairs
      buildNeighbors();

      // based on the 4 rules, identify groups based on connectivity
      build4RuleGroups();

      // make tspaces, each group is split up into subgroups.
      // Finally a tangent space is made for every resulting subgroup
      generateTSpaces();

      // degenerate quads with one good triangle will be fixed by copying a space from
      // the good triangle to the coinciding vertex.
      // all other degenerate triangles will just copy a space from any good triangle
      // with the same welded index in vertices[].
      degenEpilogue();
    }

    uint index = 0;
    for (uint f = 0; f < nrFaces; f++) {
      const uint verts = mesh.GetNumVerticesOfFace(f);
      if (verts != 3 && verts != 4) {
        continue;
      }

      // set data
      for (uint i = 0; i < verts; i++) {
        const TSpace &tSpace = tSpaces[index++];
        mesh.SetTangentSpace(f, i, tSpace.tangent, tSpace.orientPreserving);
      }
    }
  }

 protected:
  template<typename F> void runParallel(uint start, uint end, F func)
  {
#ifdef WITH_TBB
    if (isParallel) {
      tbb::parallel_for(start, end, func);
    }
    else
#endif
    {
      for (uint i = start; i < end; i++) {
        func(i);
      }
    }
  }

  ///////////////////////////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////////////////////////////

  float3 getPosition(uint vertexID)
  {
    uint f, v;
    unpack_index(f, v, vertexID);
    return mesh.GetPosition(f, v);
  }

  float3 getNormal(uint vertexID)
  {
    uint f, v;
    unpack_index(f, v, vertexID);
    return mesh.GetNormal(f, v);
  }

  float3 getTexCoord(uint vertexID)
  {
    uint f, v;
    unpack_index(f, v, vertexID);
    return mesh.GetTexCoord(f, v);
  }

  ///////////////////////////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////////////////////////////

  void generateInitialVerticesIndexList()
  {
    nrTriangles = 0;
    for (uint f = 0; f < nrFaces; f++) {
      const uint verts = mesh.GetNumVerticesOfFace(f);
      if (verts == 3) {
        nrTriangles += 1;
      }
      else if (verts == 4) {
        nrTriangles += 2;
      }
    }

    triangles.reserve(nrTriangles);

    nrTSpaces = 0;
    for (uint f = 0; f < nrFaces; f++) {
      const uint verts = mesh.GetNumVerticesOfFace(f);
      if (verts != 3 && verts != 4) {
        continue;
      }

      uint tA = uint(triangles.size());
      triangles.emplace_back(f, nrTSpaces);
      Triangle &triA = triangles[tA];

      if (verts == 3) {
        triA.setVertices(0, 1, 2);
      }
      else {
        uint tB = uint(triangles.size());
        triangles.emplace_back(f, nrTSpaces);
        Triangle &triB = triangles[tB];

        // need an order independent way to evaluate
        // tspace on quads. This is done by splitting
        // along the shortest diagonal.
        float distSQ_02 = (mesh.GetTexCoord(f, 2) - mesh.GetTexCoord(f, 0)).length_squared();
        float distSQ_13 = (mesh.GetTexCoord(f, 3) - mesh.GetTexCoord(f, 1)).length_squared();
        bool quadDiagIs_02;
        if (distSQ_02 != distSQ_13) {
          quadDiagIs_02 = (distSQ_02 < distSQ_13);
        }
        else {
          distSQ_02 = (mesh.GetPosition(f, 2) - mesh.GetPosition(f, 0)).length_squared();
          distSQ_13 = (mesh.GetPosition(f, 3) - mesh.GetPosition(f, 1)).length_squared();
          quadDiagIs_02 = !(distSQ_13 < distSQ_02);
        }

        if (quadDiagIs_02) {
          triA.setVertices(0, 1, 2);
          triB.setVertices(0, 2, 3);
        }
        else {
          triA.setVertices(0, 1, 3);
          triB.setVertices(1, 2, 3);
        }
      }

      nrTSpaces += verts;
    }
  }

  struct VertexHash {
    Mikktspace<Mesh> *mikk;
    inline uint operator()(const uint &k) const
    {
      return hash_float3x3(mikk->getPosition(k), mikk->getNormal(k), mikk->getTexCoord(k));
    }
  };

  struct VertexEqual {
    Mikktspace<Mesh> *mikk;
    inline bool operator()(const uint &kA, const uint &kB) const
    {
      return mikk->getTexCoord(kA) == mikk->getTexCoord(kB) &&
             mikk->getNormal(kA) == mikk->getNormal(kB) &&
             mikk->getPosition(kA) == mikk->getPosition(kB);
    }
  };

  /* Merge identical vertices.
   * To find vertices with identical position, normal and texcoord, we calculate a hash of the 9
   * values. Then, by sorting based on that hash, identical elements (having identical hashes) will
   * be moved next to each other. Since there might be hash collisions, the elements of each block
   * are then compared with each other and duplicates are merged.
   */
  template<bool isAtomic> void generateSharedVerticesIndexList_impl()
  {
    uint numVertices = nrTriangles * 3;
    AtomicHashSet<uint, isAtomic, VertexHash, VertexEqual> set(numVertices, {this}, {this});
    runParallel(0u, nrTriangles, [&](uint t) {
      for (uint i = 0; i < 3; i++) {
        auto res = set.emplace(triangles[t].vertices[i]);
        if (!res.second) {
          triangles[t].vertices[i] = res.first;
        }
      }
    });
  }
  void generateSharedVerticesIndexList()
  {
    if (isParallel) {
      generateSharedVerticesIndexList_impl<true>();
    }
    else {
      generateSharedVerticesIndexList_impl<false>();
    }
  }

  /////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////// Degenerate triangles ////////////////////////////////////

  void degenPrologue()
  {
    // Mark all degenerate triangles
    totalTriangles = nrTriangles;
    std::atomic<uint> degenTriangles(0);
    runParallel(0u, totalTriangles, [&](uint t) {
      const float3 p0 = getPosition(triangles[t].vertices[0]);
      const float3 p1 = getPosition(triangles[t].vertices[1]);
      const float3 p2 = getPosition(triangles[t].vertices[2]);
      if (p0 == p1 || p0 == p2 || p1 == p2)  // degenerate
      {
        triangles[t].markDegenerate = true;
        degenTriangles.fetch_add(1);
      }
    });
    nrTriangles -= degenTriangles.load();

    if (totalTriangles == nrTriangles) {
      return;
    }

    // locate quads with only one good triangle
    runParallel(0u, totalTriangles - 1, [&](uint t) {
      Triangle &triangleA = triangles[t], &triangleB = triangles[t + 1];
      if (triangleA.faceIdx != triangleB.faceIdx) {
        /* Individual triangle, skip. */
        return;
      }
      if (triangleA.markDegenerate != triangleB.markDegenerate) {
        triangleA.quadOneDegenTri = true;
        triangleB.quadOneDegenTri = true;
      }
    });

    std::stable_partition(triangles.begin(), triangles.end(), [](const Triangle &tri) {
      return !tri.markDegenerate;
    });
  }

  void degenEpilogue()
  {
    if (nrTriangles == totalTriangles) {
      return;
    }

    std::unordered_map<uint, uint> goodTriangleMap;
    for (uint t = 0; t < nrTriangles; t++) {
      for (uint i = 0; i < 3; i++) {
        goodTriangleMap.emplace(triangles[t].vertices[i], pack_index(t, i));
      }
    }

    // deal with degenerate triangles
    // punishment for degenerate triangles is O(nrTriangles) extra memory.
    for (uint t = nrTriangles; t < totalTriangles; t++) {
      // degenerate triangles on a quad with one good triangle are skipped
      // here but processed in the next loop
      if (triangles[t].quadOneDegenTri) {
        continue;
      }

      for (uint i = 0; i < 3; i++) {
        const auto entry = goodTriangleMap.find(triangles[t].vertices[i]);
        if (entry == goodTriangleMap.end()) {
          // Matching vertex from good triangle is not found.
          continue;
        }

        uint tSrc, iSrc;
        unpack_index(tSrc, iSrc, entry->second);
        const uint iSrcVert = triangles[tSrc].faceVertex[iSrc];
        const uint iSrcOffs = triangles[tSrc].tSpaceIdx;
        const uint iDstVert = triangles[t].faceVertex[i];
        const uint iDstOffs = triangles[t].tSpaceIdx;
        // copy tspace
        tSpaces[iDstOffs + iDstVert] = tSpaces[iSrcOffs + iSrcVert];
      }
    }

    // deal with degenerate quads with one good triangle
    for (uint t = 0; t < nrTriangles; t++) {
      // this triangle belongs to a quad where the
      // other triangle is degenerate
      if (!triangles[t].quadOneDegenTri) {
        continue;
      }
      uint vertFlag = (1u << triangles[t].faceVertex[0]) | (1u << triangles[t].faceVertex[1]) |
                      (1u << triangles[t].faceVertex[2]);
      uint missingFaceVertex = 0;
      if ((vertFlag & 2) == 0) {
        missingFaceVertex = 1;
      }
      else if ((vertFlag & 4) == 0) {
        missingFaceVertex = 2;
      }
      else if ((vertFlag & 8) == 0) {
        missingFaceVertex = 3;
      }

      uint faceIdx = triangles[t].faceIdx;
      float3 dstP = mesh.GetPosition(faceIdx, missingFaceVertex);
      bool found = false;
      for (uint i = 0; i < 3; i++) {
        const uint faceVertex = triangles[t].faceVertex[i];
        const float3 srcP = mesh.GetPosition(faceIdx, faceVertex);
        if (srcP == dstP) {
          const uint offset = triangles[t].tSpaceIdx;
          tSpaces[offset + missingFaceVertex] = tSpaces[offset + faceVertex];
          found = true;
          break;
        }
      }
      assert(found);
      (void)found;
    }
  }

  ///////////////////////////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////////////////////////////

  // returns the texture area times 2
  float calcTexArea(uint tri)
  {
    const float3 t1 = getTexCoord(triangles[tri].vertices[0]);
    const float3 t2 = getTexCoord(triangles[tri].vertices[1]);
    const float3 t3 = getTexCoord(triangles[tri].vertices[2]);

    const float t21x = t2.x - t1.x;
    const float t21y = t2.y - t1.y;
    const float t31x = t3.x - t1.x;
    const float t31y = t3.y - t1.y;

    const float signedAreaSTx2 = t21x * t31y - t21y * t31x;
    return fabsf(signedAreaSTx2);
  }

  void initTriangle()
  {
    // triangles[f].iFlag is cleared in generateInitialVerticesIndexList()
    // which is called before this function.

    // evaluate first order derivatives
    runParallel(0u, nrTriangles, [&](uint t) {
      Triangle &triangle = triangles[t];

      // initial values
      const float3 v1 = getPosition(triangle.vertices[0]);
      const float3 v2 = getPosition(triangle.vertices[1]);
      const float3 v3 = getPosition(triangle.vertices[2]);
      const float3 t1 = getTexCoord(triangle.vertices[0]);
      const float3 t2 = getTexCoord(triangle.vertices[1]);
      const float3 t3 = getTexCoord(triangle.vertices[2]);

      const float t21x = t2.x - t1.x;
      const float t21y = t2.y - t1.y;
      const float t31x = t3.x - t1.x;
      const float t31y = t3.y - t1.y;
      const float3 d1 = v2 - v1, d2 = v3 - v1;

      const float signedAreaSTx2 = t21x * t31y - t21y * t31x;
      const float3 vOs = (t31y * d1) - (t21y * d2);   // eq 18
      const float3 vOt = (-t31x * d1) + (t21x * d2);  // eq 19

      triangle.orientPreserving = (signedAreaSTx2 > 0);

      if (not_zero(signedAreaSTx2)) {
        const float lenOs2 = vOs.length_squared();
        const float lenOt2 = vOt.length_squared();
        const float fS = triangle.orientPreserving ? 1.0f : (-1.0f);
        if (not_zero(lenOs2)) {
          triangle.tangent = vOs * (fS / sqrtf(lenOs2));
        }

        // if this is a good triangle
        if (not_zero(lenOs2) && not_zero(lenOt2)) {
          triangle.groupWithAny = false;
        }
      }
    });

    // force otherwise healthy quads to a fixed orientation
    runParallel(0u, nrTriangles - 1, [&](uint t) {
      Triangle &triangleA = triangles[t], &triangleB = triangles[t + 1];
      if (triangleA.faceIdx != triangleB.faceIdx) {
        // this is not a quad
        return;
      }

      // bad triangles should already have been removed by
      // degenPrologue(), but just in case check that neither are degenerate
      if (!(triangleA.markDegenerate || triangleB.markDegenerate)) {
        // if this happens the quad has extremely bad mapping!!
        if (triangleA.orientPreserving != triangleB.orientPreserving) {
          bool chooseOrientFirstTri = false;
          if (triangleB.groupWithAny) {
            chooseOrientFirstTri = true;
          }
          else if (calcTexArea(t) >= calcTexArea(t + 1)) {
            chooseOrientFirstTri = true;
          }

          // force match
          const uint t0 = chooseOrientFirstTri ? t : (t + 1);
          const uint t1 = chooseOrientFirstTri ? (t + 1) : t;
          triangles[t1].orientPreserving = triangles[t0].orientPreserving;
        }
      }
    });
  }

  /////////////////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////// Edges ///////////////////////////////////////////

  struct NeighborShard {
    struct Entry {
      Entry(uint32_t key_, uint data_) : key(key_), data(data_) {}
      uint key, data;
    };
    std::vector<Entry> entries;

    NeighborShard(size_t capacity)
    {
      entries.reserve(capacity);
    }

    void buildNeighbors(Mikktspace<Mesh> *mikk)
    {
      /* Entries are added by iterating over t, so by using a stable sort,
       * we don't have to compare based on t as well. */
      {
        std::vector<Entry> tempEntries(entries.size(), {0, 0});
        radixsort(entries, tempEntries, [](const Entry &e) { return e.key; });
      }

      for (uint i = 0; i < entries.size(); i++) {
        const Entry &a = entries[i];
        uint tA, iA;
        unpack_index(tA, iA, a.data);
        Mikktspace<Mesh>::Triangle &triA = mikk->triangles[tA];

        if (triA.neighbor[iA] != UNSET_ENTRY) {
          continue;
        }

        uint i0A = triA.vertices[iA], i1A = triA.vertices[(iA != 2) ? (iA + 1) : 0];
        for (uint j = i + 1; j < entries.size(); j++) {
          const Entry &b = entries[j];
          uint tB, iB;
          unpack_index(tB, iB, b.data);
          Mikktspace<Mesh>::Triangle &triB = mikk->triangles[tB];

          if (b.key != a.key)
            break;

          if (triB.neighbor[iB] != UNSET_ENTRY) {
            continue;
          }

          uint i1B = triB.vertices[iB], i0B = triB.vertices[(iB != 2) ? (iB + 1) : 0];
          if (i0A == i0B && i1A == i1B) {
            triA.neighbor[iA] = tB;
            triB.neighbor[iB] = tA;
            break;
          }
        }
      }
    }
  };

  void buildNeighbors()
  {
    /* In order to parallelize the processing, we divide the vertices into shards.
     * Since only vertex pairs with the same key will be checked, we can process
     * shards independently as long as we ensure that all vertices with the same
     * key go into the same shard.
     * This is done by hashing the key to get the shard index of each vertex.
     */
    // TODO: Two-step filling that first counts and then fills? Could be parallel then.
    uint targetNrShards = isParallel ? uint(4 * nrThreads) : 1;
    uint nrShards = 1, hashShift = 32;
    while (nrShards < targetNrShards) {
      nrShards *= 2;
      hashShift -= 1;
    }

    /* Reserve 25% extra to account for variation due to hashing. */
    size_t reserveSize = size_t(double(3 * nrTriangles) * 1.25 / nrShards);
    std::vector<NeighborShard> shards(nrShards, {reserveSize});

    for (uint t = 0; t < nrTriangles; t++) {
      Triangle &triangle = triangles[t];
      for (uint i = 0; i < 3; i++) {
        const uint i0 = triangle.vertices[i];
        const uint i1 = triangle.vertices[(i != 2) ? (i + 1) : 0];
        const uint high = std::max(i0, i1), low = std::min(i0, i1);
        const uint hash = hash_uint3(high, low, 0);
        /* TODO: Reusing the hash here means less hash space inside each shard.
         * Computing a second hash with a different seed it probably not worth it? */
        const uint shard = isParallel ? (hash >> hashShift) : 0;
        shards[shard].entries.emplace_back(hash, pack_index(t, i));
      }
    }

    runParallel(0u, nrShards, [&](uint s) { shards[s].buildNeighbors(this); });
  }

  ///////////////////////////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////////////////////////////

  void assignRecur(const uint t, uint groupId)
  {
    if (t == UNSET_ENTRY) {
      return;
    }

    Triangle &triangle = triangles[t];
    Group &group = groups[groupId];

    // track down vertex
    const uint vertRep = group.vertexRepresentative;
    uint i = 3;
    if (triangle.vertices[0] == vertRep) {
      i = 0;
    }
    else if (triangle.vertices[1] == vertRep) {
      i = 1;
    }
    else if (triangle.vertices[2] == vertRep) {
      i = 2;
    }
    assert(i < 3);

    // early out
    if (triangle.group[i] != UNSET_ENTRY) {
      return;
    }

    if (triangle.groupWithAny) {
      // first to group with a group-with-anything triangle
      // determines its orientation.
      // This is the only existing order dependency in the code!!
      if (triangle.group[0] == UNSET_ENTRY && triangle.group[1] == UNSET_ENTRY &&
          triangle.group[2] == UNSET_ENTRY)
      {
        triangle.orientPreserving = group.orientPreserving;
      }
    }

    if (triangle.orientPreserving != group.orientPreserving) {
      return;
    }

    triangle.group[i] = groupId;

    const uint t_L = triangle.neighbor[i];
    const uint t_R = triangle.neighbor[i > 0 ? (i - 1) : 2];
    assignRecur(t_L, groupId);
    assignRecur(t_R, groupId);
  }

  void build4RuleGroups()
  {
    /* NOTE: This could be parallelized by grouping all [t, i] pairs into
     * shards by hash(triangles[t].vertices[i]). This way, each shard can be processed
     * independently and in parallel.
     * However, the `groupWithAny` logic needs special handling (e.g. lock a mutex when
     * encountering a `groupWithAny` triangle, then sort it out, then unlock and proceed). */
    for (uint t = 0; t < nrTriangles; t++) {
      Triangle &triangle = triangles[t];
      for (uint i = 0; i < 3; i++) {
        // if not assigned to a group
        if (triangle.groupWithAny || triangle.group[i] != UNSET_ENTRY) {
          continue;
        }

        const uint newGroupId = uint(groups.size());
        triangle.group[i] = newGroupId;

        groups.emplace_back(triangle.vertices[i], bool(triangle.orientPreserving));

        const uint t_L = triangle.neighbor[i];
        const uint t_R = triangle.neighbor[i > 0 ? (i - 1) : 2];
        assignRecur(t_L, newGroupId);
        assignRecur(t_R, newGroupId);
      }
    }
  }

  ///////////////////////////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////////////////////////////

  template<bool atomic> void accumulateTSpaces(uint t)
  {
    const Triangle &triangle = triangles[t];
    // only valid triangles get to add their contribution
    if (triangle.groupWithAny) {
      return;
    }

    /* TODO: Vectorize?
     * Also: Could add special case for flat shading, when all normals are equal half of the fCos
     * projections and two of the three tangent projections are unnecessary. */
    std::array<float3, 3> n, p;
    for (uint i = 0; i < 3; i++) {
      n[i] = getNormal(triangle.vertices[i]);
      p[i] = getPosition(triangle.vertices[i]);
    }

    std::array<float, 3> fCos = {dot(project(n[0], p[1] - p[0]), project(n[0], p[2] - p[0])),
                                 dot(project(n[1], p[2] - p[1]), project(n[1], p[0] - p[1])),
                                 dot(project(n[2], p[0] - p[2]), project(n[2], p[1] - p[2]))};

    for (uint i = 0; i < 3; i++) {
      uint groupId = triangle.group[i];
      if (groupId != UNSET_ENTRY) {
        float3 tangent = project(n[i], triangle.tangent) *
                         fast_acosf(std::clamp(fCos[i], -1.0f, 1.0f));
        if constexpr (atomic) {
          groups[groupId].accumulateTSpaceAtomic(tangent);
        }
        else {
          groups[groupId].accumulateTSpace(tangent);
        }
      }
    }
  }

  void generateTSpaces()
  {
    if (isParallel) {
      runParallel(0u, nrTriangles, [&](uint t) { accumulateTSpaces<true>(t); });
    }
    else {
      for (uint t = 0; t < nrTriangles; t++) {
        accumulateTSpaces<false>(t);
      }
    }

    /* TODO: Worth parallelizing? Probably not. */
    for (Group &group : groups) {
      group.normalizeTSpace();
    }

    tSpaces.resize(nrTSpaces);

    for (uint t = 0; t < nrTriangles; t++) {
      Triangle &triangle = triangles[t];
      for (uint i = 0; i < 3; i++) {
        uint groupId = triangle.group[i];
        if (groupId == UNSET_ENTRY) {
          continue;
        }
        const Group group = groups[groupId];
        assert(triangle.orientPreserving == group.orientPreserving);

        // output tspace
        const uint offset = triangle.tSpaceIdx;
        const uint faceVertex = triangle.faceVertex[i];
        tSpaces[offset + faceVertex].accumulateGroup(group);
      }
    }
  }
};

}  // namespace mikk
