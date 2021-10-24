# Lock-free parallel disjoint set data structure

This is a small self-contained C++11 implementation of the UNION-FIND data
structure with path compression and union by rank and a few extras It supports
concurrent `find()`, `same()` and `unite()` calls as described in the paper

*Wait-free Parallel Algorithms for the Union-Find Problem*
by Richard J. Anderson and Heather Woll

In addition, this class supports optimistic locking (`try_lock()`/`unlock()`)
of disjoint sets and a *combined* unite+unlock operation for pairs of sets.
