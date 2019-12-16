

// DO NOT EDIT !
// This file is generated using the MantaFlow preprocessor (prep generate).

/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Multigrid solver
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Copyright 2016, by Florian Ferstl (florian.ferstl.ff@gmail.com)
 *
 * This is an implementation of the solver developed by Dick et al. [1]
 * without topology awareness (= vertex duplication on coarser levels). This
 * simplification allows us to use regular grids for all levels of the multigrid
 * hierarchy and works well for moderately complex domains.
 *
 * [1] Solving the Fluid Pressure Poisson Equation Using Multigrid-Evaluation
 *     and Improvements, C. Dick, M. Rogowsky, R. Westermann, IEEE TVCG 2015
 *
 ******************************************************************************/

#include "multigrid.h"

#define FOR_LVL(IDX, LVL) for (int IDX = 0; IDX < mb[LVL].size(); IDX++)

#define FOR_VEC_MINMAX(VEC, MIN, MAX) \
  Vec3i VEC; \
  const Vec3i VEC##__min = (MIN), VEC##__max = (MAX); \
  for (VEC.z = VEC##__min.z; VEC.z <= VEC##__max.z; VEC.z++) \
    for (VEC.y = VEC##__min.y; VEC.y <= VEC##__max.y; VEC.y++) \
      for (VEC.x = VEC##__min.x; VEC.x <= VEC##__max.x; VEC.x++)

#define FOR_VECLIN_MINMAX(VEC, LIN, MIN, MAX) \
  Vec3i VEC; \
  int LIN = 0; \
  const Vec3i VEC##__min = (MIN), VEC##__max = (MAX); \
  for (VEC.z = VEC##__min.z; VEC.z <= VEC##__max.z; VEC.z++) \
    for (VEC.y = VEC##__min.y; VEC.y <= VEC##__max.y; VEC.y++) \
      for (VEC.x = VEC##__min.x; VEC.x <= VEC##__max.x; VEC.x++, LIN++)

#define MG_TIMINGS(X)
//#define MG_TIMINGS(X) X

using namespace std;
namespace Manta {

// Helper class for calling mantaflow kernels with a specific number of threads
class ThreadSize {
  IndexInt s;

 public:
  ThreadSize(IndexInt _s)
  {
    s = _s;
  }
  IndexInt size()
  {
    return s;
  }
};

// ----------------------------------------------------------------------------
// Efficient min heap for <ID, key> pairs with 0<=ID<N and 0<=key<K
// (elements are stored in K buckets, where each bucket is a doubly linked list).
// - if K<<N, all ops are O(1) on avg (worst case O(K)).
// - memory usage O(K+N): (K+N) * 3 * sizeof(int).
class NKMinHeap {
 private:
  struct Entry {
    int key, prev, next;
    Entry() : key(-1), prev(-1), next(-1)
    {
    }
  };

  int mN, mK, mSize, mMinKey;

  // Double linked lists of IDs, one for each bucket/key.
  // The first K entries are the buckets' head pointers,
  // and the last N entries correspond to the IDs.
  std::vector<Entry> mEntries;

 public:
  NKMinHeap(int N, int K) : mN(N), mK(K), mSize(0), mMinKey(-1), mEntries(N + K)
  {
  }

  int size()
  {
    return mSize;
  }
  int getKey(int ID)
  {
    return mEntries[mK + ID].key;
  }

  // Insert, decrease or increase key (or delete by setting key to -1)
  void setKey(int ID, int key);

  // peek min key (returns ID/key pair)
  std::pair<int, int> peekMin();

  // pop min key (returns ID/key pair)
  std::pair<int, int> popMin();

  void print();  // for debugging
};

void NKMinHeap::setKey(int ID, int key)
{
  assertMsg(0 <= ID && ID < mN, "NKMinHeap::setKey: ID out of range");
  assertMsg(-1 <= key && key < mK, "NKMinHeap::setKey: key out of range");

  const int kid = mK + ID;

  if (mEntries[kid].key == key)
    return;  // nothing changes

  // remove from old key-list if ID existed previously
  if (mEntries[kid].key != -1) {
    int pred = mEntries[kid].prev;
    int succ = mEntries[kid].next;  // can be -1

    mEntries[pred].next = succ;
    if (succ != -1)
      mEntries[succ].prev = pred;

    // if removed key was minimum key, mMinKey may need to be updated
    int removedKey = mEntries[kid].key;
    if (removedKey == mMinKey) {
      if (mSize == 1) {
        mMinKey = -1;
      }
      else {
        for (; mMinKey < mK; mMinKey++) {
          if (mEntries[mMinKey].next != -1)
            break;
        }
      }
    }

    mSize--;
  }

  // set new key of ID
  mEntries[kid].key = key;

  if (key == -1) {
    // finished if key was set to -1
    mEntries[kid].next = mEntries[kid].prev = -1;
    return;
  }

  // add key
  mSize++;
  if (mMinKey == -1)
    mMinKey = key;
  else
    mMinKey = std::min(mMinKey, key);

  // insert into new key-list (headed by mEntries[key])
  int tmp = mEntries[key].next;

  mEntries[key].next = kid;
  mEntries[kid].prev = key;

  mEntries[kid].next = tmp;
  if (tmp != -1)
    mEntries[tmp].prev = kid;
}

std::pair<int, int> NKMinHeap::peekMin()
{
  if (mSize == 0)
    return std::pair<int, int>(-1, -1);  // error

  const int ID = mEntries[mMinKey].next - mK;
  return std::pair<int, int>(ID, mMinKey);
}

std::pair<int, int> NKMinHeap::popMin()
{
  if (mSize == 0)
    return std::pair<int, int>(-1, -1);  // error

  const int kid = mEntries[mMinKey].next;
  const int ID = kid - mK;
  const int key = mMinKey;

  // remove from key-list
  int pred = mEntries[kid].prev;
  int succ = mEntries[kid].next;  // can be -1

  mEntries[pred].next = succ;
  if (succ != -1)
    mEntries[succ].prev = pred;

  // remove entry
  mEntries[kid] = Entry();
  mSize--;

  // update mMinKey
  if (mSize == 0) {
    mMinKey = -1;
  }
  else {
    for (; mMinKey < mK; mMinKey++) {
      if (mEntries[mMinKey].next != -1)
        break;
    }
  }

  // return result
  return std::pair<int, int>(ID, key);
}

void NKMinHeap::print()
{
  std::cout << "Size: " << mSize << ", MinKey: " << mMinKey << std::endl;
  for (int key = 0; key < mK; key++) {
    if (mEntries[key].next != -1) {
      std::cout << "Key " << key << ": ";
      int kid = mEntries[key].next;
      while (kid != -1) {
        std::cout << kid - mK << " ";
        kid = mEntries[kid].next;
      }
      std::cout << std::endl;
    }
  }
  std::cout << std::endl;
}

// ----------------------------------------------------------------------------
// GridMg methods
//
// Illustration of 27-point stencil indices
// y     | z = -1    z = 0      z = 1
// ^     | 6  7  8,  15 16 17,  24 25 26
// |     | 3  4  5,  12 13 14,  21 22 23
// o-> x | 0  1  2,   9 10 11,  18 19 20
//
// Symmetric storage with only 14 entries per vertex
// y     | z = -1    z = 0      z = 1
// ^     | -  -  -,   2  3  4,  11 12 13
// |     | -  -  -,   -  0  1,   8  9 10
// o-> x | -  -  -,   -  -  -,   5  6  7

GridMg::GridMg(const Vec3i &gridSize)
    : mNumPreSmooth(1),
      mNumPostSmooth(1),
      mCoarsestLevelAccuracy(Real(1E-8)),
      mTrivialEquationScale(Real(1E-6)),
      mIsASet(false),
      mIsRhsSet(false)
{
  MG_TIMINGS(MuTime time;)

  // 2D or 3D mode
  mIs3D = (gridSize.z > 1);
  mDim = mIs3D ? 3 : 2;
  mStencilSize = mIs3D ? 14 : 5;  // A has a full 27-point stencil on levels > 0
  mStencilSize0 = mIs3D ? 4 : 3;  // A has a 7-point stencil on level 0
  mStencilMin = Vec3i(-1, -1, mIs3D ? -1 : 0);
  mStencilMax = Vec3i(1, 1, mIs3D ? 1 : 0);

  // Create level 0 (=original grid)
  mSize.push_back(gridSize);
  mPitch.push_back(Vec3i(1, mSize.back().x, mSize.back().x * mSize.back().y));
  int n = mSize.back().x * mSize.back().y * mSize.back().z;

  mA.push_back(std::vector<Real>(n * mStencilSize0));
  mx.push_back(std::vector<Real>(n));
  mb.push_back(std::vector<Real>(n));
  mr.push_back(std::vector<Real>(n));
  mType.push_back(std::vector<VertexType>(n));
  mCGtmp1.push_back(std::vector<double>());
  mCGtmp2.push_back(std::vector<double>());
  mCGtmp3.push_back(std::vector<double>());
  mCGtmp4.push_back(std::vector<double>());

  debMsg("GridMg::GridMg level 0: " << mSize[0].x << " x " << mSize[0].y << " x " << mSize[0].z
                                    << " x ",
         2);

  // Create coarse levels >0
  for (int l = 1; l <= 100; l++) {
    if (mSize[l - 1].x <= 5 && mSize[l - 1].y <= 5 && mSize[l - 1].z <= 5)
      break;
    if (n <= 1000)
      break;

    mSize.push_back((mSize[l - 1] + 2) / 2);
    mPitch.push_back(Vec3i(1, mSize.back().x, mSize.back().x * mSize.back().y));
    n = mSize.back().x * mSize.back().y * mSize.back().z;

    mA.push_back(std::vector<Real>(n * mStencilSize));
    mx.push_back(std::vector<Real>(n));
    mb.push_back(std::vector<Real>(n));
    mr.push_back(std::vector<Real>(n));
    mType.push_back(std::vector<VertexType>(n));
    mCGtmp1.push_back(std::vector<double>());
    mCGtmp2.push_back(std::vector<double>());
    mCGtmp3.push_back(std::vector<double>());
    mCGtmp4.push_back(std::vector<double>());

    debMsg("GridMg::GridMg level " << l << ": " << mSize[l].x << " x " << mSize[l].y << " x "
                                   << mSize[l].z << " x ",
           2);
  }

  // Additional memory for CG on coarsest level
  mCGtmp1.back() = std::vector<double>(n);
  mCGtmp2.back() = std::vector<double>(n);
  mCGtmp3.back() = std::vector<double>(n);
  mCGtmp4.back() = std::vector<double>(n);

  MG_TIMINGS(debMsg("GridMg: Allocation done in " << time.update(), 1);)

  // Precalculate coarsening paths:
  // (V) <--restriction-- (U) <--A_{l-1}-- (W) <--interpolation-- (N)
  Vec3i p7stencil[7] = {Vec3i(0, 0, 0),
                        Vec3i(-1, 0, 0),
                        Vec3i(1, 0, 0),
                        Vec3i(0, -1, 0),
                        Vec3i(0, 1, 0),
                        Vec3i(0, 0, -1),
                        Vec3i(0, 0, 1)};
  Vec3i V(1, 1, 1);  // reference coarse grid vertex at (1,1,1)
  FOR_VEC_MINMAX(U, V * 2 + mStencilMin, V * 2 + mStencilMax)
  {
    for (int i = 0; i < 1 + 2 * mDim; i++) {
      Vec3i W = U + p7stencil[i];
      FOR_VEC_MINMAX(N, W / 2, (W + 1) / 2)
      {
        int s = dot(N, Vec3i(1, 3, 9));

        if (s >= 13) {
          CoarseningPath path;
          path.N = N - 1;                  // offset of N on coarse grid
          path.U = U - V * 2;              // offset of U on fine grid
          path.W = W - V * 2;              // offset of W on fine grid
          path.sc = s - 13;                // stencil index corresponding to V<-N on coarse grid
          path.sf = (i + 1) / 2;           // stencil index corresponding to U<-W on coarse grid
          path.inUStencil = (i % 2 == 0);  // fine grid stencil entry stored at U or W?
          path.rw = Real(1) /
                    Real(1 << ((U.x % 2) + (U.y % 2) + (U.z % 2)));  // restriction weight V<-U
          path.iw = Real(1) /
                    Real(1 << ((W.x % 2) + (W.y % 2) + (W.z % 2)));  // interpolation weight W<-N
          mCoarseningPaths0.push_back(path);
        }
      }
    }
  }

  auto pathLess = [](const GridMg::CoarseningPath &p1, const GridMg::CoarseningPath &p2) {
    if (p1.sc == p2.sc)
      return dot(p1.U + 1, Vec3i(1, 3, 9)) < dot(p2.U + 1, Vec3i(1, 3, 9));
    return p1.sc < p2.sc;
  };
  std::sort(mCoarseningPaths0.begin(), mCoarseningPaths0.end(), pathLess);
}

void GridMg::analyzeStencil(int v,
                            bool is3D,
                            bool &isStencilSumNonZero,
                            bool &isEquationTrivial) const
{
  Vec3i V = vecIdx(v, 0);

  // collect stencil entries
  Real A[7];
  A[0] = mA[0][v * mStencilSize0 + 0];
  A[1] = mA[0][v * mStencilSize0 + 1];
  A[2] = mA[0][v * mStencilSize0 + 2];
  A[3] = is3D ? mA[0][v * mStencilSize0 + 3] : Real(0);
  A[4] = V.x != 0 ? mA[0][(v - mPitch[0].x) * mStencilSize0 + 1] : Real(0);
  A[5] = V.y != 0 ? mA[0][(v - mPitch[0].y) * mStencilSize0 + 2] : Real(0);
  A[6] = V.z != 0 && is3D ? mA[0][(v - mPitch[0].z) * mStencilSize0 + 3] : Real(0);

  // compute sum of stencil entries
  Real stencilMax = Real(0), stencilSum = Real(0);
  for (int i = 0; i < 7; i++) {
    stencilSum += A[i];
    stencilMax = max(stencilMax, std::abs(A[i]));
  }

  // check if sum is numerically zero
  isStencilSumNonZero = std::abs(stencilSum / stencilMax) > Real(1E-6);

  // check for trivial equation (exact comparisons)
  isEquationTrivial = A[0] == Real(1) && A[1] == Real(0) && A[2] == Real(0) && A[3] == Real(0) &&
                      A[4] == Real(0) && A[5] == Real(0) && A[6] == Real(0);
}

struct knCopyA : public KernelBase {
  knCopyA(std::vector<Real> &sizeRef,
          std::vector<Real> &A0,
          int stencilSize0,
          bool is3D,
          const Grid<Real> *pA0,
          const Grid<Real> *pAi,
          const Grid<Real> *pAj,
          const Grid<Real> *pAk)
      : KernelBase(sizeRef.size()),
        sizeRef(sizeRef),
        A0(A0),
        stencilSize0(stencilSize0),
        is3D(is3D),
        pA0(pA0),
        pAi(pAi),
        pAj(pAj),
        pAk(pAk)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 std::vector<Real> &sizeRef,
                 std::vector<Real> &A0,
                 int stencilSize0,
                 bool is3D,
                 const Grid<Real> *pA0,
                 const Grid<Real> *pAi,
                 const Grid<Real> *pAj,
                 const Grid<Real> *pAk) const
  {
    A0[idx * stencilSize0 + 0] = (*pA0)[idx];
    A0[idx * stencilSize0 + 1] = (*pAi)[idx];
    A0[idx * stencilSize0 + 2] = (*pAj)[idx];
    if (is3D)
      A0[idx * stencilSize0 + 3] = (*pAk)[idx];
  }
  inline std::vector<Real> &getArg0()
  {
    return sizeRef;
  }
  typedef std::vector<Real> type0;
  inline std::vector<Real> &getArg1()
  {
    return A0;
  }
  typedef std::vector<Real> type1;
  inline int &getArg2()
  {
    return stencilSize0;
  }
  typedef int type2;
  inline bool &getArg3()
  {
    return is3D;
  }
  typedef bool type3;
  inline const Grid<Real> *getArg4()
  {
    return pA0;
  }
  typedef Grid<Real> type4;
  inline const Grid<Real> *getArg5()
  {
    return pAi;
  }
  typedef Grid<Real> type5;
  inline const Grid<Real> *getArg6()
  {
    return pAj;
  }
  typedef Grid<Real> type6;
  inline const Grid<Real> *getArg7()
  {
    return pAk;
  }
  typedef Grid<Real> type7;
  void runMessage()
  {
    debMsg("Executing kernel knCopyA ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, sizeRef, A0, stencilSize0, is3D, pA0, pAi, pAj, pAk);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  std::vector<Real> &sizeRef;
  std::vector<Real> &A0;
  int stencilSize0;
  bool is3D;
  const Grid<Real> *pA0;
  const Grid<Real> *pAi;
  const Grid<Real> *pAj;
  const Grid<Real> *pAk;
};

struct knActivateVertices : public KernelBase {
  knActivateVertices(std::vector<GridMg::VertexType> &type_0,
                     std::vector<Real> &A0,
                     bool &nonZeroStencilSumFound,
                     bool &trivialEquationsFound,
                     const GridMg &mg)
      : KernelBase(type_0.size()),
        type_0(type_0),
        A0(A0),
        nonZeroStencilSumFound(nonZeroStencilSumFound),
        trivialEquationsFound(trivialEquationsFound),
        mg(mg)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 std::vector<GridMg::VertexType> &type_0,
                 std::vector<Real> &A0,
                 bool &nonZeroStencilSumFound,
                 bool &trivialEquationsFound,
                 const GridMg &mg) const
  {
    // active vertices on level 0 are vertices with non-zero diagonal entry in A
    type_0[idx] = GridMg::vtInactive;

    if (mg.mA[0][idx * mg.mStencilSize0 + 0] != Real(0)) {
      type_0[idx] = GridMg::vtActive;

      bool isStencilSumNonZero = false, isEquationTrivial = false;
      mg.analyzeStencil(int(idx), mg.mIs3D, isStencilSumNonZero, isEquationTrivial);

      // Note: nonZeroStencilSumFound and trivialEquationsFound are only
      // changed from false to true, and hence there are no race conditions.
      if (isStencilSumNonZero)
        nonZeroStencilSumFound = true;

      // scale down trivial equations
      if (isEquationTrivial) {
        type_0[idx] = GridMg::vtActiveTrivial;
        A0[idx * mg.mStencilSize0 + 0] *= mg.mTrivialEquationScale;
        trivialEquationsFound = true;
      };
    }
  }
  inline std::vector<GridMg::VertexType> &getArg0()
  {
    return type_0;
  }
  typedef std::vector<GridMg::VertexType> type0;
  inline std::vector<Real> &getArg1()
  {
    return A0;
  }
  typedef std::vector<Real> type1;
  inline bool &getArg2()
  {
    return nonZeroStencilSumFound;
  }
  typedef bool type2;
  inline bool &getArg3()
  {
    return trivialEquationsFound;
  }
  typedef bool type3;
  inline const GridMg &getArg4()
  {
    return mg;
  }
  typedef GridMg type4;
  void runMessage()
  {
    debMsg("Executing kernel knActivateVertices ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, type_0, A0, nonZeroStencilSumFound, trivialEquationsFound, mg);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  std::vector<GridMg::VertexType> &type_0;
  std::vector<Real> &A0;
  bool &nonZeroStencilSumFound;
  bool &trivialEquationsFound;
  const GridMg &mg;
};

void GridMg::setA(const Grid<Real> *pA0,
                  const Grid<Real> *pAi,
                  const Grid<Real> *pAj,
                  const Grid<Real> *pAk)
{
  MG_TIMINGS(MuTime time;)

  // Copy level 0
  knCopyA(mx[0], mA[0], mStencilSize0, mIs3D, pA0, pAi, pAj, pAk);

  // Determine active vertices and scale trivial equations
  bool nonZeroStencilSumFound = false;
  bool trivialEquationsFound = false;

  knActivateVertices(mType[0], mA[0], nonZeroStencilSumFound, trivialEquationsFound, *this);

  if (trivialEquationsFound)
    debMsg("GridMg::setA: Found at least one trivial equation", 2);

  // Sanity check: if all rows of A sum up to 0 --> A doesn't have full rank (opposite direction
  // isn't necessarily true)
  if (!nonZeroStencilSumFound)
    debMsg(
        "GridMg::setA: Found constant mode: A*1=0! A does not have full rank and multigrid may "
        "not converge. (forgot to fix a pressure value?)",
        1);

  // Create coarse grids and operators on levels >0
  for (int l = 1; l < mA.size(); l++) {
    MG_TIMINGS(time.get();)
    genCoarseGrid(l);
    MG_TIMINGS(debMsg("GridMg: Generated level " << l << " in " << time.update(), 1);)
    genCoraseGridOperator(l);
    MG_TIMINGS(debMsg("GridMg: Generated operator " << l << " in " << time.update(), 1);)
  }

  mIsASet = true;
  mIsRhsSet = false;  // invalidate rhs
}

struct knSetRhs : public KernelBase {
  knSetRhs(std::vector<Real> &b, const Grid<Real> &rhs, const GridMg &mg)
      : KernelBase(b.size()), b(b), rhs(rhs), mg(mg)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, std::vector<Real> &b, const Grid<Real> &rhs, const GridMg &mg) const
  {
    b[idx] = rhs[idx];

    // scale down trivial equations
    if (mg.mType[0][idx] == GridMg::vtActiveTrivial) {
      b[idx] *= mg.mTrivialEquationScale;
    };
  }
  inline std::vector<Real> &getArg0()
  {
    return b;
  }
  typedef std::vector<Real> type0;
  inline const Grid<Real> &getArg1()
  {
    return rhs;
  }
  typedef Grid<Real> type1;
  inline const GridMg &getArg2()
  {
    return mg;
  }
  typedef GridMg type2;
  void runMessage()
  {
    debMsg("Executing kernel knSetRhs ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, b, rhs, mg);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  std::vector<Real> &b;
  const Grid<Real> &rhs;
  const GridMg &mg;
};

void GridMg::setRhs(const Grid<Real> &rhs)
{
  assertMsg(mIsASet, "GridMg::setRhs Error: A has not been set.");

  knSetRhs(mb[0], rhs, *this);

  mIsRhsSet = true;
}

template<class T> struct knSet : public KernelBase {
  knSet(std::vector<T> &data, T value) : KernelBase(data.size()), data(data), value(value)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, std::vector<T> &data, T value) const
  {
    data[idx] = value;
  }
  inline std::vector<T> &getArg0()
  {
    return data;
  }
  typedef std::vector<T> type0;
  inline T &getArg1()
  {
    return value;
  }
  typedef T type1;
  void runMessage()
  {
    debMsg("Executing kernel knSet ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, data, value);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  std::vector<T> &data;
  T value;
};

template<class T> struct knCopyToVector : public KernelBase {
  knCopyToVector(std::vector<T> &dst, const Grid<T> &src)
      : KernelBase(dst.size()), dst(dst), src(src)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, std::vector<T> &dst, const Grid<T> &src) const
  {
    dst[idx] = src[idx];
  }
  inline std::vector<T> &getArg0()
  {
    return dst;
  }
  typedef std::vector<T> type0;
  inline const Grid<T> &getArg1()
  {
    return src;
  }
  typedef Grid<T> type1;
  void runMessage()
  {
    debMsg("Executing kernel knCopyToVector ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, dst, src);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  std::vector<T> &dst;
  const Grid<T> &src;
};

template<class T> struct knCopyToGrid : public KernelBase {
  knCopyToGrid(const std::vector<T> &src, Grid<T> &dst)
      : KernelBase(src.size()), src(src), dst(dst)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, const std::vector<T> &src, Grid<T> &dst) const
  {
    dst[idx] = src[idx];
  }
  inline const std::vector<T> &getArg0()
  {
    return src;
  }
  typedef std::vector<T> type0;
  inline Grid<T> &getArg1()
  {
    return dst;
  }
  typedef Grid<T> type1;
  void runMessage()
  {
    debMsg("Executing kernel knCopyToGrid ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, src, dst);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  const std::vector<T> &src;
  Grid<T> &dst;
};

template<class T> struct knAddAssign : public KernelBase {
  knAddAssign(std::vector<T> &dst, const std::vector<T> &src)
      : KernelBase(dst.size()), dst(dst), src(src)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, std::vector<T> &dst, const std::vector<T> &src) const
  {
    dst[idx] += src[idx];
  }
  inline std::vector<T> &getArg0()
  {
    return dst;
  }
  typedef std::vector<T> type0;
  inline const std::vector<T> &getArg1()
  {
    return src;
  }
  typedef std::vector<T> type1;
  void runMessage()
  {
    debMsg("Executing kernel knAddAssign ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, dst, src);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  std::vector<T> &dst;
  const std::vector<T> &src;
};

Real GridMg::doVCycle(Grid<Real> &dst, const Grid<Real> *src)
{
  MG_TIMINGS(MuTime timeSmooth; MuTime timeCG; MuTime timeI; MuTime timeR; MuTime timeTotal;
             MuTime time;)
  MG_TIMINGS(timeSmooth.clear(); timeCG.clear(); timeI.clear(); timeR.clear();)

  assertMsg(mIsASet && mIsRhsSet, "GridMg::doVCycle Error: A and/or rhs have not been set.");

  const int maxLevel = int(mA.size()) - 1;

  if (src) {
    knCopyToVector<Real>(mx[0], *src);
  }
  else {
    knSet<Real>(mx[0], Real(0));
  }

  for (int l = 0; l < maxLevel; l++) {
    MG_TIMINGS(time.update();)
    for (int i = 0; i < mNumPreSmooth; i++) {
      smoothGS(l, false);
    }

    MG_TIMINGS(timeSmooth += time.update();)

    calcResidual(l);
    restrict(l + 1, mr[l], mb[l + 1]);

    knSet<Real>(mx[l + 1], Real(0));

    MG_TIMINGS(timeR += time.update();)
  }

  MG_TIMINGS(time.update();)
  solveCG(maxLevel);
  MG_TIMINGS(timeCG += time.update();)

  for (int l = maxLevel - 1; l >= 0; l--) {
    MG_TIMINGS(time.update();)
    interpolate(l, mx[l + 1], mr[l]);

    knAddAssign<Real>(mx[l], mr[l]);

    MG_TIMINGS(timeI += time.update();)

    for (int i = 0; i < mNumPostSmooth; i++) {
      smoothGS(l, true);
    }
    MG_TIMINGS(timeSmooth += time.update();)
  }

  calcResidual(0);
  Real res = calcResidualNorm(0);

  knCopyToGrid<Real>(mx[0], dst);

  MG_TIMINGS(debMsg("GridMg: Finished VCycle in "
                        << timeTotal.update() << " (smoothing: " << timeSmooth
                        << ", CG: " << timeCG << ", R: " << timeR << ", I: " << timeI << ")",
                    1);)

  return res;
}

struct knActivateCoarseVertices : public KernelBase {
  knActivateCoarseVertices(std::vector<GridMg::VertexType> &type, int unused)
      : KernelBase(type.size()), type(type), unused(unused)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, std::vector<GridMg::VertexType> &type, int unused) const
  {
    // set all remaining 'free' vertices to 'removed',
    if (type[idx] == GridMg::vtFree)
      type[idx] = GridMg::vtRemoved;

    // then convert 'zero' vertices to 'active' and 'removed' vertices to 'inactive'
    if (type[idx] == GridMg::vtZero)
      type[idx] = GridMg::vtActive;
    if (type[idx] == GridMg::vtRemoved)
      type[idx] = GridMg::vtInactive;
  }
  inline std::vector<GridMg::VertexType> &getArg0()
  {
    return type;
  }
  typedef std::vector<GridMg::VertexType> type0;
  inline int &getArg1()
  {
    return unused;
  }
  typedef int type1;
  void runMessage()
  {
    debMsg("Executing kernel knActivateCoarseVertices ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, type, unused);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  std::vector<GridMg::VertexType> &type;
  int unused;
};

// Determine active cells on coarse level l from active cells on fine level l-1
// while ensuring a full-rank interpolation operator (see Section 3.3 in [1]).
void GridMg::genCoarseGrid(int l)
{
  // AF_Free: unused/untouched vertices
  // AF_Zero: vertices selected for coarser level
  // AF_Removed: vertices removed from coarser level
  enum activeFlags : char { AF_Removed = 0, AF_Zero = 1, AF_Free = 2 };

  // initialize all coarse vertices with 'free'
  knSet<VertexType>(mType[l], vtFree);

  // initialize min heap of (ID: fine grid vertex, key: #free interpolation vertices) pairs
  NKMinHeap heap(int(mb[l - 1].size()),
                 mIs3D ? 9 : 5);  // max 8 (or 4 in 2D) free interpolation vertices

  FOR_LVL(v, l - 1)
  {
    if (mType[l - 1][v] != vtInactive) {
      Vec3i V = vecIdx(v, l - 1);
      int fiv = 1 << ((V.x % 2) + (V.y % 2) + (V.z % 2));
      heap.setKey(v, fiv);
    }
  }

  // process fine vertices in heap consecutively, always choosing the vertex with
  // the currently smallest number of free interpolation vertices
  while (heap.size() > 0) {
    int v = heap.popMin().first;
    Vec3i V = vecIdx(v, l - 1);

    // loop over associated interpolation vertices of V on coarse level l:
    // the first encountered 'free' vertex is set to 'zero',
    // all remaining 'free' vertices are set to 'removed'.
    bool vdone = false;

    FOR_VEC_MINMAX(I, V / 2, (V + 1) / 2)
    {
      int i = linIdx(I, l);

      if (mType[l][i] == vtFree) {
        if (vdone) {
          mType[l][i] = vtRemoved;
        }
        else {
          mType[l][i] = vtZero;
          vdone = true;
        }

        // update #free interpolation vertices in heap:
        // loop over all associated restriction vertices of I on fine level l-1
        FOR_VEC_MINMAX(R, vmax(0, I * 2 - 1), vmin(mSize[l - 1] - 1, I * 2 + 1))
        {
          int r = linIdx(R, l - 1);
          int key = heap.getKey(r);

          if (key > 1) {
            heap.setKey(r, key - 1);
          }  // decrease key of r
          else if (key > -1) {
            heap.setKey(r, -1);
          }  // removes r from heap
        }
      }
    }
  }

  knActivateCoarseVertices(mType[l], 0);
}

struct knGenCoarseGridOperator : public KernelBase {
  knGenCoarseGridOperator(std::vector<Real> &sizeRef,
                          std::vector<Real> &A,
                          int l,
                          const GridMg &mg)
      : KernelBase(sizeRef.size()), sizeRef(sizeRef), A(A), l(l), mg(mg)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 std::vector<Real> &sizeRef,
                 std::vector<Real> &A,
                 int l,
                 const GridMg &mg) const
  {
    if (mg.mType[l][idx] == GridMg::vtInactive)
      return;

    for (int i = 0; i < mg.mStencilSize; i++) {
      A[idx * mg.mStencilSize + i] = Real(0);
    }  // clear stencil

    Vec3i V = mg.vecIdx(int(idx), l);

    // Calculate the stencil of A_l at V by considering all vertex paths of the form:
    // (V) <--restriction-- (U) <--A_{l-1}-- (W) <--interpolation-- (N)
    // V and N are vertices on the coarse grid level l,
    // U and W are vertices on the fine grid level l-1.

    if (l == 1) {
      // loop over precomputed paths
      for (auto it = mg.mCoarseningPaths0.begin(); it != mg.mCoarseningPaths0.end(); it++) {
        Vec3i N = V + it->N;
        int n = mg.linIdx(N, l);
        if (!mg.inGrid(N, l) || mg.mType[l][n] == GridMg::vtInactive)
          continue;

        Vec3i U = V * 2 + it->U;
        int u = mg.linIdx(U, l - 1);
        if (!mg.inGrid(U, l - 1) || mg.mType[l - 1][u] == GridMg::vtInactive)
          continue;

        Vec3i W = V * 2 + it->W;
        int w = mg.linIdx(W, l - 1);
        if (!mg.inGrid(W, l - 1) || mg.mType[l - 1][w] == GridMg::vtInactive)
          continue;

        if (it->inUStencil) {
          A[idx * mg.mStencilSize + it->sc] += it->rw *
                                               mg.mA[l - 1][u * mg.mStencilSize0 + it->sf] *
                                               it->iw;
        }
        else {
          A[idx * mg.mStencilSize + it->sc] += it->rw *
                                               mg.mA[l - 1][w * mg.mStencilSize0 + it->sf] *
                                               it->iw;
        }
      }
    }
    else {
      // l > 1:
      // loop over restriction vertices U on level l-1 associated with V
      FOR_VEC_MINMAX(U, vmax(0, V * 2 - 1), vmin(mg.mSize[l - 1] - 1, V * 2 + 1))
      {
        int u = mg.linIdx(U, l - 1);
        if (mg.mType[l - 1][u] == GridMg::vtInactive)
          continue;

        // restriction weight
        Real rw = Real(1) / Real(1 << ((U.x % 2) + (U.y % 2) + (U.z % 2)));

        // loop over all stencil neighbors N of V on level l that can be reached via restriction to
        // U
        FOR_VEC_MINMAX(N, (U - 1) / 2, vmin(mg.mSize[l] - 1, (U + 2) / 2))
        {
          int n = mg.linIdx(N, l);
          if (mg.mType[l][n] == GridMg::vtInactive)
            continue;

          // stencil entry at V associated to N (coarse grid level l)
          Vec3i SC = N - V + mg.mStencilMax;
          int sc = SC.x + 3 * SC.y + 9 * SC.z;
          if (sc < mg.mStencilSize - 1)
            continue;

          // loop over all vertices W which are in the stencil of A_{l-1} at U
          // and which interpolate from N
          FOR_VEC_MINMAX(W,
                         vmax(0, vmax(U - 1, N * 2 - 1)),
                         vmin(mg.mSize[l - 1] - 1, vmin(U + 1, N * 2 + 1)))
          {
            int w = mg.linIdx(W, l - 1);
            if (mg.mType[l - 1][w] == GridMg::vtInactive)
              continue;

            // stencil entry at U associated to W (fine grid level l-1)
            Vec3i SF = W - U + mg.mStencilMax;
            int sf = SF.x + 3 * SF.y + 9 * SF.z;

            Real iw = Real(1) /
                      Real(1 << ((W.x % 2) + (W.y % 2) + (W.z % 2)));  // interpolation weight

            if (sf < mg.mStencilSize) {
              A[idx * mg.mStencilSize + sc - mg.mStencilSize + 1] +=
                  rw * mg.mA[l - 1][w * mg.mStencilSize + mg.mStencilSize - 1 - sf] * iw;
            }
            else {
              A[idx * mg.mStencilSize + sc - mg.mStencilSize + 1] +=
                  rw * mg.mA[l - 1][u * mg.mStencilSize + sf - mg.mStencilSize + 1] * iw;
            }
          }
        }
      }
    }
  }
  inline std::vector<Real> &getArg0()
  {
    return sizeRef;
  }
  typedef std::vector<Real> type0;
  inline std::vector<Real> &getArg1()
  {
    return A;
  }
  typedef std::vector<Real> type1;
  inline int &getArg2()
  {
    return l;
  }
  typedef int type2;
  inline const GridMg &getArg3()
  {
    return mg;
  }
  typedef GridMg type3;
  void runMessage()
  {
    debMsg("Executing kernel knGenCoarseGridOperator ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, sizeRef, A, l, mg);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  std::vector<Real> &sizeRef;
  std::vector<Real> &A;
  int l;
  const GridMg &mg;
};

// Calculate A_l on coarse level l from A_{l-1} on fine level l-1 using
// Galerkin-based coarsening, i.e., compute A_l = R * A_{l-1} * I.
void GridMg::genCoraseGridOperator(int l)
{
  // for each coarse grid vertex V
  knGenCoarseGridOperator(mx[l], mA[l], l, *this);
}

struct knSmoothColor : public KernelBase {
  knSmoothColor(ThreadSize &numBlocks,
                std::vector<Real> &x,
                const Vec3i &blockSize,
                const std::vector<Vec3i> &colorOffs,
                int l,
                const GridMg &mg)
      : KernelBase(numBlocks.size()),
        numBlocks(numBlocks),
        x(x),
        blockSize(blockSize),
        colorOffs(colorOffs),
        l(l),
        mg(mg)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 ThreadSize &numBlocks,
                 std::vector<Real> &x,
                 const Vec3i &blockSize,
                 const std::vector<Vec3i> &colorOffs,
                 int l,
                 const GridMg &mg) const
  {
    Vec3i blockOff(int(idx) % blockSize.x,
                   (int(idx) % (blockSize.x * blockSize.y)) / blockSize.x,
                   int(idx) / (blockSize.x * blockSize.y));

    for (int off = 0; off < colorOffs.size(); off++) {

      Vec3i V = blockOff * 2 + colorOffs[off];
      if (!mg.inGrid(V, l))
        continue;

      const int v = mg.linIdx(V, l);
      if (mg.mType[l][v] == GridMg::vtInactive)
        continue;

      Real sum = mg.mb[l][v];

      if (l == 0) {
        int n;
        for (int d = 0; d < mg.mDim; d++) {
          if (V[d] > 0) {
            n = v - mg.mPitch[0][d];
            sum -= mg.mA[0][n * mg.mStencilSize0 + d + 1] * mg.mx[0][n];
          }
          if (V[d] < mg.mSize[0][d] - 1) {
            n = v + mg.mPitch[0][d];
            sum -= mg.mA[0][v * mg.mStencilSize0 + d + 1] * mg.mx[0][n];
          }
        }

        x[v] = sum / mg.mA[0][v * mg.mStencilSize0 + 0];
      }
      else {
        FOR_VECLIN_MINMAX(S, s, mg.mStencilMin, mg.mStencilMax)
        {
          if (s == mg.mStencilSize - 1)
            continue;

          Vec3i N = V + S;
          int n = mg.linIdx(N, l);

          if (mg.inGrid(N, l) && mg.mType[l][n] != GridMg::vtInactive) {
            if (s < mg.mStencilSize) {
              sum -= mg.mA[l][n * mg.mStencilSize + mg.mStencilSize - 1 - s] * mg.mx[l][n];
            }
            else {
              sum -= mg.mA[l][v * mg.mStencilSize + s - mg.mStencilSize + 1] * mg.mx[l][n];
            }
          }
        }

        x[v] = sum / mg.mA[l][v * mg.mStencilSize + 0];
      }
    }
  }
  inline ThreadSize &getArg0()
  {
    return numBlocks;
  }
  typedef ThreadSize type0;
  inline std::vector<Real> &getArg1()
  {
    return x;
  }
  typedef std::vector<Real> type1;
  inline const Vec3i &getArg2()
  {
    return blockSize;
  }
  typedef Vec3i type2;
  inline const std::vector<Vec3i> &getArg3()
  {
    return colorOffs;
  }
  typedef std::vector<Vec3i> type3;
  inline int &getArg4()
  {
    return l;
  }
  typedef int type4;
  inline const GridMg &getArg5()
  {
    return mg;
  }
  typedef GridMg type5;
  void runMessage()
  {
    debMsg("Executing kernel knSmoothColor ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, numBlocks, x, blockSize, colorOffs, l, mg);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  ThreadSize &numBlocks;
  std::vector<Real> &x;
  const Vec3i &blockSize;
  const std::vector<Vec3i> &colorOffs;
  int l;
  const GridMg &mg;
};

void GridMg::smoothGS(int l, bool reversedOrder)
{
  // Multicolor Gauss-Seidel with two colors for the 5/7-point stencil on level 0
  // and with four/eight colors for the 9/27-point stencil on levels > 0
  std::vector<std::vector<Vec3i>> colorOffs;
  const Vec3i a[8] = {Vec3i(0, 0, 0),
                      Vec3i(1, 0, 0),
                      Vec3i(0, 1, 0),
                      Vec3i(1, 1, 0),
                      Vec3i(0, 0, 1),
                      Vec3i(1, 0, 1),
                      Vec3i(0, 1, 1),
                      Vec3i(1, 1, 1)};
  if (mIs3D) {
    if (l == 0)
      colorOffs = {{a[0], a[3], a[5], a[6]}, {a[1], a[2], a[4], a[7]}};
    else
      colorOffs = {{a[0]}, {a[1]}, {a[2]}, {a[3]}, {a[4]}, {a[5]}, {a[6]}, {a[7]}};
  }
  else {
    if (l == 0)
      colorOffs = {{a[0], a[3]}, {a[1], a[2]}};
    else
      colorOffs = {{a[0]}, {a[1]}, {a[2]}, {a[3]}};
  }

  // Divide grid into 2x2 blocks for parallelization
  Vec3i blockSize = (mSize[l] + 1) / 2;
  ThreadSize numBlocks(blockSize.x * blockSize.y * blockSize.z);

  for (int c = 0; c < colorOffs.size(); c++) {
    int color = reversedOrder ? int(colorOffs.size()) - 1 - c : c;

    knSmoothColor(numBlocks, mx[l], blockSize, colorOffs[color], l, *this);
  }
}

struct knCalcResidual : public KernelBase {
  knCalcResidual(std::vector<Real> &r, int l, const GridMg &mg)
      : KernelBase(r.size()), r(r), l(l), mg(mg)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, std::vector<Real> &r, int l, const GridMg &mg) const
  {
    if (mg.mType[l][idx] == GridMg::vtInactive)
      return;

    Vec3i V = mg.vecIdx(int(idx), l);

    Real sum = mg.mb[l][idx];

    if (l == 0) {
      int n;
      for (int d = 0; d < mg.mDim; d++) {
        if (V[d] > 0) {
          n = int(idx) - mg.mPitch[0][d];
          sum -= mg.mA[0][n * mg.mStencilSize0 + d + 1] * mg.mx[0][n];
        }
        if (V[d] < mg.mSize[0][d] - 1) {
          n = int(idx) + mg.mPitch[0][d];
          sum -= mg.mA[0][idx * mg.mStencilSize0 + d + 1] * mg.mx[0][n];
        }
      }
      sum -= mg.mA[0][idx * mg.mStencilSize0 + 0] * mg.mx[0][idx];
    }
    else {
      FOR_VECLIN_MINMAX(S, s, mg.mStencilMin, mg.mStencilMax)
      {
        Vec3i N = V + S;
        int n = mg.linIdx(N, l);

        if (mg.inGrid(N, l) && mg.mType[l][n] != GridMg::vtInactive) {
          if (s < mg.mStencilSize) {
            sum -= mg.mA[l][n * mg.mStencilSize + mg.mStencilSize - 1 - s] * mg.mx[l][n];
          }
          else {
            sum -= mg.mA[l][idx * mg.mStencilSize + s - mg.mStencilSize + 1] * mg.mx[l][n];
          }
        }
      }
    }

    r[idx] = sum;
  }
  inline std::vector<Real> &getArg0()
  {
    return r;
  }
  typedef std::vector<Real> type0;
  inline int &getArg1()
  {
    return l;
  }
  typedef int type1;
  inline const GridMg &getArg2()
  {
    return mg;
  }
  typedef GridMg type2;
  void runMessage()
  {
    debMsg("Executing kernel knCalcResidual ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, r, l, mg);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  std::vector<Real> &r;
  int l;
  const GridMg &mg;
};

void GridMg::calcResidual(int l)
{
  knCalcResidual(mr[l], l, *this);
}

struct knResidualNormSumSqr : public KernelBase {
  knResidualNormSumSqr(const vector<Real> &r, int l, const GridMg &mg)
      : KernelBase(r.size()), r(r), l(l), mg(mg), result(Real(0))
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx, const vector<Real> &r, int l, const GridMg &mg, Real &result)
  {
    if (mg.mType[l][idx] == GridMg::vtInactive)
      return;

    result += r[idx] * r[idx];
  }
  inline operator Real()
  {
    return result;
  }
  inline Real &getRet()
  {
    return result;
  }
  inline const vector<Real> &getArg0()
  {
    return r;
  }
  typedef vector<Real> type0;
  inline int &getArg1()
  {
    return l;
  }
  typedef int type1;
  inline const GridMg &getArg2()
  {
    return mg;
  }
  typedef GridMg type2;
  void runMessage()
  {
    debMsg("Executing kernel knResidualNormSumSqr ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r)
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, r, l, mg, result);
  }
  void run()
  {
    tbb::parallel_reduce(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  knResidualNormSumSqr(knResidualNormSumSqr &o, tbb::split)
      : KernelBase(o), r(o.r), l(o.l), mg(o.mg), result(Real(0))
  {
  }
  void join(const knResidualNormSumSqr &o)
  {
    result += o.result;
  }
  const vector<Real> &r;
  int l;
  const GridMg &mg;
  Real result;
};
;

Real GridMg::calcResidualNorm(int l)
{
  Real res = knResidualNormSumSqr(mr[l], l, *this);

  return std::sqrt(res);
}

// Standard conjugate gradients with Jacobi preconditioner
// Notes: Always run at double precision. Not parallelized since
//        coarsest level is assumed to be small.
void GridMg::solveCG(int l)
{
  auto applyAStencil = [this](int v, int l, const std::vector<double> &vec) -> double {
    Vec3i V = vecIdx(v, l);

    double sum = 0;

    if (l == 0) {
      int n;
      for (int d = 0; d < mDim; d++) {
        if (V[d] > 0) {
          n = v - mPitch[0][d];
          sum += mA[0][n * mStencilSize0 + d + 1] * vec[n];
        }
        if (V[d] < mSize[0][d] - 1) {
          n = v + mPitch[0][d];
          sum += mA[0][v * mStencilSize0 + d + 1] * vec[n];
        }
      }
      sum += mA[0][v * mStencilSize0 + 0] * vec[v];
    }
    else {
      FOR_VECLIN_MINMAX(S, s, mStencilMin, mStencilMax)
      {
        Vec3i N = V + S;
        int n = linIdx(N, l);

        if (inGrid(N, l) && mType[l][n] != vtInactive) {
          if (s < mStencilSize) {
            sum += mA[l][n * mStencilSize + mStencilSize - 1 - s] * vec[n];
          }
          else {
            sum += mA[l][v * mStencilSize + s - mStencilSize + 1] * vec[n];
          }
        }
      }
    }

    return sum;
  };

  std::vector<double> &z = mCGtmp1[l];
  std::vector<double> &p = mCGtmp2[l];
  std::vector<double> &x = mCGtmp3[l];
  std::vector<double> &r = mCGtmp4[l];

  // Initialization:
  double alphaTop = 0;
  double initialResidual = 0;

  FOR_LVL(v, l)
  {
    x[v] = mx[l][v];
  }

  FOR_LVL(v, l)
  {
    if (mType[l][v] == vtInactive)
      continue;

    r[v] = mb[l][v] - applyAStencil(v, l, x);
    if (l == 0) {
      z[v] = r[v] / mA[0][v * mStencilSize0 + 0];
    }
    else {
      z[v] = r[v] / mA[l][v * mStencilSize + 0];
    }

    initialResidual += r[v] * r[v];
    p[v] = z[v];
    alphaTop += r[v] * z[v];
  }

  initialResidual = std::sqrt(initialResidual);

  int iter = 0;
  const int maxIter = 10000;
  double residual = -1;

  // CG iterations
  for (; iter < maxIter && initialResidual > 1E-12; iter++) {
    double alphaBot = 0;

    FOR_LVL(v, l)
    {
      if (mType[l][v] == vtInactive)
        continue;

      z[v] = applyAStencil(v, l, p);
      alphaBot += p[v] * z[v];
    }

    double alpha = alphaTop / alphaBot;

    double alphaTopNew = 0;
    residual = 0;

    FOR_LVL(v, l)
    {
      if (mType[l][v] == vtInactive)
        continue;

      x[v] += alpha * p[v];
      r[v] -= alpha * z[v];
      residual += r[v] * r[v];
      if (l == 0)
        z[v] = r[v] / mA[0][v * mStencilSize0 + 0];
      else
        z[v] = r[v] / mA[l][v * mStencilSize + 0];
      alphaTopNew += r[v] * z[v];
    }

    residual = std::sqrt(residual);

    if (residual / initialResidual < mCoarsestLevelAccuracy)
      break;

    double beta = alphaTopNew / alphaTop;
    alphaTop = alphaTopNew;

    FOR_LVL(v, l)
    {
      p[v] = z[v] + beta * p[v];
    }
    debMsg("GridMg::solveCG i=" << iter << " rel-residual=" << (residual / initialResidual), 5);
  }

  FOR_LVL(v, l)
  {
    mx[l][v] = Real(x[v]);
  }

  if (iter == maxIter) {
    debMsg("GridMg::solveCG Warning: Reached maximum number of CG iterations", 1);
  }
  else {
    debMsg("GridMg::solveCG Info: Reached residual " << residual << " in " << iter
                                                     << " iterations",
           2);
  }
}

struct knRestrict : public KernelBase {
  knRestrict(std::vector<Real> &dst, const std::vector<Real> &src, int l_dst, const GridMg &mg)
      : KernelBase(dst.size()), dst(dst), src(src), l_dst(l_dst), mg(mg)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 std::vector<Real> &dst,
                 const std::vector<Real> &src,
                 int l_dst,
                 const GridMg &mg) const
  {
    if (mg.mType[l_dst][idx] == GridMg::vtInactive)
      return;

    const int l_src = l_dst - 1;

    // Coarse grid vertex
    Vec3i V = mg.vecIdx(int(idx), l_dst);

    Real sum = Real(0);

    FOR_VEC_MINMAX(R, vmax(0, V * 2 - 1), vmin(mg.mSize[l_src] - 1, V * 2 + 1))
    {
      int r = mg.linIdx(R, l_src);
      if (mg.mType[l_src][r] == GridMg::vtInactive)
        continue;

      // restriction weight
      Real rw = Real(1) / Real(1 << ((R.x % 2) + (R.y % 2) + (R.z % 2)));

      sum += rw * src[r];
    }

    dst[idx] = sum;
  }
  inline std::vector<Real> &getArg0()
  {
    return dst;
  }
  typedef std::vector<Real> type0;
  inline const std::vector<Real> &getArg1()
  {
    return src;
  }
  typedef std::vector<Real> type1;
  inline int &getArg2()
  {
    return l_dst;
  }
  typedef int type2;
  inline const GridMg &getArg3()
  {
    return mg;
  }
  typedef GridMg type3;
  void runMessage()
  {
    debMsg("Executing kernel knRestrict ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, dst, src, l_dst, mg);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  std::vector<Real> &dst;
  const std::vector<Real> &src;
  int l_dst;
  const GridMg &mg;
};

void GridMg::restrict(int l_dst, const std::vector<Real> &src, std::vector<Real> &dst) const
{
  knRestrict(dst, src, l_dst, *this);
}

struct knInterpolate : public KernelBase {
  knInterpolate(std::vector<Real> &dst, const std::vector<Real> &src, int l_dst, const GridMg &mg)
      : KernelBase(dst.size()), dst(dst), src(src), l_dst(l_dst), mg(mg)
  {
    runMessage();
    run();
  }
  inline void op(IndexInt idx,
                 std::vector<Real> &dst,
                 const std::vector<Real> &src,
                 int l_dst,
                 const GridMg &mg) const
  {
    if (mg.mType[l_dst][idx] == GridMg::vtInactive)
      return;

    const int l_src = l_dst + 1;

    Vec3i V = mg.vecIdx(int(idx), l_dst);

    Real sum = Real(0);

    FOR_VEC_MINMAX(I, V / 2, (V + 1) / 2)
    {
      int i = mg.linIdx(I, l_src);
      if (mg.mType[l_src][i] != GridMg::vtInactive)
        sum += src[i];
    }

    // interpolation weight
    Real iw = Real(1) / Real(1 << ((V.x % 2) + (V.y % 2) + (V.z % 2)));

    dst[idx] = iw * sum;
  }
  inline std::vector<Real> &getArg0()
  {
    return dst;
  }
  typedef std::vector<Real> type0;
  inline const std::vector<Real> &getArg1()
  {
    return src;
  }
  typedef std::vector<Real> type1;
  inline int &getArg2()
  {
    return l_dst;
  }
  typedef int type2;
  inline const GridMg &getArg3()
  {
    return mg;
  }
  typedef GridMg type3;
  void runMessage()
  {
    debMsg("Executing kernel knInterpolate ", 3);
    debMsg("Kernel range"
               << " size " << size << " ",
           4);
  };
  void operator()(const tbb::blocked_range<IndexInt> &__r) const
  {
    for (IndexInt idx = __r.begin(); idx != (IndexInt)__r.end(); idx++)
      op(idx, dst, src, l_dst, mg);
  }
  void run()
  {
    tbb::parallel_for(tbb::blocked_range<IndexInt>(0, size), *this);
  }
  std::vector<Real> &dst;
  const std::vector<Real> &src;
  int l_dst;
  const GridMg &mg;
};

void GridMg::interpolate(int l_dst, const std::vector<Real> &src, std::vector<Real> &dst) const
{
  knInterpolate(dst, src, l_dst, *this);
}

};  // namespace Manta
