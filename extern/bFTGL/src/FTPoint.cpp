#include "FTPoint.h"


bool operator == ( const FTPoint &a, const FTPoint &b) 
{
    return((a.x == b.x) && (a.y == b.y) && (a.z == b.z));
}

bool operator != ( const FTPoint &a, const FTPoint &b) 
{
    return((a.x != b.x) || (a.y != b.y) || (a.z != b.z));
}


