#ifndef CSG_INDEXDEFS_H
#define CSG_INDEXDEFS_H

//typdefs for lists and things in the CSG library

#include <vector>

typedef std::vector<int> PIndexList;
typedef PIndexList::iterator PIndexIt;
typedef PIndexList::const_iterator const_PIndexIt;

typedef std::vector<int> VIndexList;
typedef VIndexList::iterator VIndexIt;
typedef VIndexList::const_iterator const_VIndexIt;

typedef std::vector< PIndexList > OverlapTable;



#endif


