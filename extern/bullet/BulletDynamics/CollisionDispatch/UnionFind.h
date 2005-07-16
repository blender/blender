#ifndef UNION_FIND_H
#define UNION_FIND_H

///UnionFind calculates connected subsets
class UnionFind
  {
    private:
      int *id, *sz;
	  int m_N;

    public:
      int find(int x);
		UnionFind(int N);
	  void	reset();

      int find(int p, int q);
      void unite(int p, int q);
  };


#endif //UNION_FIND_H
