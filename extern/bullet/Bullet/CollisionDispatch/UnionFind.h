#ifndef UNION_FIND_H
#define UNION_FIND_H

///UnionFind calculates connected subsets
class UnionFind
  {
    private:
		int*	m_id;
		int*	m_sz;
		int m_N;

    public:
		int find(int x);
	  
		UnionFind();
		~UnionFind();

	  void	reset(int N);

      int find(int p, int q);
      void unite(int p, int q);

	  void	Allocate(int N);
	  void	Free();

  };


#endif //UNION_FIND_H
