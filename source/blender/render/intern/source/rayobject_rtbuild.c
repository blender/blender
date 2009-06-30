#include "rayobject_rtbuild.h"
#include "MEM_guardedalloc.h"
#include "BLI_arithb.h"
#include "BKE_utildefines.h"

static int partition_nth_element(RTBuilder *b, int _begin, int _end, int n);
static void split_leafs(RTBuilder *b, int *nth, int partitions, int split_axis);


static void RayObject_rtbuild_init(RTBuilder *b, RayObject **begin, RayObject **end)
{
	int i;

	b->begin = begin;
	b->end   = end;
	b->split_axis = 0;
	
	for(i=0; i<MAX_CHILDS; i++)
		b->child[i] = 0;
}

RTBuilder* RayObject_rtbuild_create(int size)
{
	RTBuilder *builder  = (RTBuilder*) MEM_mallocN( sizeof(RTBuilder), "RTBuilder" );
	RayObject **memblock= (RayObject**)MEM_mallocN( sizeof(RayObject*),"RTBuilder.objects");
	RayObject_rtbuild_init(builder, memblock, memblock);
	return builder;
}

void RayObject_rtbuild_free(RTBuilder *b)
{
	MEM_freeN(b->begin);
	MEM_freeN(b);
}

void RayObject_rtbuild_add(RTBuilder *b, RayObject *o)
{
	*(b->end++) = o;
}

RTBuilder* rtbuild_get_child(RTBuilder *b, int child, RTBuilder *tmp)
{
	RayObject_rtbuild_init( tmp, b->child[child], b->child[child+1] );
	return tmp;
}

int RayObject_rtbuild_size(RTBuilder *b)
{
	return b->end - b->begin;
}

/* Split methods */
static void merge_bb(RTBuilder *b, float *min, float *max)
{
	RayObject **index = b->begin;

	for(; index != b->end; index++)
		RE_rayobject_merge_bb(*index, min, max);
}

static int calc_largest_axis(RTBuilder *b)
{
	float min[3], max[3], sub[3];

	INIT_MINMAX(min, max);
	merge_bb( b, min, max);
		
	VECSUB(sub, max, min);
	if(sub[0] > sub[1])
	{
		if(sub[0] > sub[2])
			return 0;
		else
			return 2;
	}
	else
	{
		if(sub[1] > sub[2])
			return 1;
		else
			return 2;
	}	
}


//Unballanced mean
//TODO better balance nodes
//TODO suport for variable number of partitions (its hardcoded in 2)
void rtbuild_mean_split(RTBuilder *b, int nchilds, int axis)
{
	int nth[3] = {0, (b->end - b->begin)/2, b->end-b->begin};
	split_leafs(b, nth, 2, axis);
}
	
	
void rtbuild_mean_split_largest_axis(RTBuilder *b, int nchilds)
{
	int axis = calc_largest_axis(b);
	rtbuild_mean_split(b, nchilds, axis);
}


/*
 * Helper code
 * PARTITION code / used on mean-split
 * basicly this a std::nth_element (like on C++ STL algorithm)
 */
static void sort_swap(RTBuilder *b, int i, int j)
{
	SWAP(RayObject*, b->begin[i], b->begin[j]);
}
 
static int sort_get_value(RTBuilder *b, int i)
{
	float min[3], max[3];
	RE_rayobject_merge_bb(b->begin[i], min, max);
	return max[i];
}
 
static int medianof3(RTBuilder *d, int a, int b, int c)
{
	float fa = sort_get_value( d, a );
	float fb = sort_get_value( d, b );
	float fc = sort_get_value( d, c );

	if(fb < fa)
	{
		if(fc < fb)
			return b;
		else
		{
			if(fc < fa)
				return c;
			else
				return a;
		}
	}
	else
	{
		if(fc > fb)
			return b;
		else
		{
			if(fc > fa)
				return c;
			else
				return a;
		}
	}
}

static void insertionsort(RTBuilder *b, int lo, int hi)
{
	int i;
	for(i=lo; i<hi; i++)
	{
		RayObject *t = b->begin[i];
		float tv= sort_get_value(b, i);
		int j=i;
		
		while( j != lo && tv < sort_get_value(b, j-1))
		{
			b->begin[j] = b->begin[j-1];
			j--;
		}
		b->begin[j] = t;
	}
}

static int partition(RTBuilder *b, int lo, int mid, int hi)
{
	float x = sort_get_value( b, mid );
	
	int i=lo, j=hi;
	while (1)
	{
		while (sort_get_value(b,i) < x) i++;
		j--;
		while (x < sort_get_value(b,j)) j--;
		if(!(i < j))
			return i;

		sort_swap(b, i, j);
		i++;
	}
}

//
// PARTITION code / used on mean-split
// basicly this is an adapted std::nth_element (C++ STL <algorithm>)
//
// after a call to this function you can expect one of:
//      every node to left of a[n] are smaller or equal to it
//      every node to the right of a[n] are greater or equal to it
static int partition_nth_element(RTBuilder *b, int _begin, int _end, int n)
{
	int begin = _begin, end = _end, cut;
	while(end-begin > 3)
	{
		cut = partition(b, begin, medianof3(b, begin, begin+(end-begin)/2, end-1), end);
		if(cut <= n)
			begin = cut;
		else
			end = cut;
	}
	insertionsort(b, begin, end);

	return n;
}

static void split_leafs(RTBuilder *b, int *nth, int partitions, int split_axis)
{
	int i;
	b->split_axis = split_axis;
	for(i=0; i < partitions-1; i++)
	{
		if(nth[i] >= nth[partitions])
			break;

		partition_nth_element(b, nth[i],  nth[i+1], nth[partitions] );
	}
}
