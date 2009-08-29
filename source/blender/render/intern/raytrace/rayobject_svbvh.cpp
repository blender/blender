#include "vbvh.h"
#include "svbvh.h"
#include "reorganize.h"

#define DFS_STACK_SIZE	256

struct SVBVHTree
{
	RayObject rayobj;

	SVBVHNode *root;
	MemArena *node_arena;

	float cost;
	RTBuilder *builder;
};


template<>
void bvh_done<SVBVHTree>(SVBVHTree *obj)
{
	rtbuild_done(obj->builder);
	
	//TODO find a away to exactly calculate the needed memory
	MemArena *arena1 = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE);
					   BLI_memarena_use_malloc(arena1);

	//Build and optimize the tree
	VBVHNode *root = BuildBinaryVBVH(arena1).transform(obj->builder);

	reorganize(root);
	remove_useless(root, &root);
	bvh_refit(root);
	
	pushup(root);
	pushdown(root);
	pushup_simd<VBVHNode,4>(root);

	MemArena *arena2 = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE);
	BLI_memarena_use_malloc(arena2);
	BLI_memarena_use_align(arena2, 16);
	obj->root = Reorganize_SVBVH<VBVHNode>(arena2).transform(root);
	
	BLI_memarena_free(arena1);
	
	obj->node_arena = arena2;
	obj->cost = 1.0;
	
	
	rtbuild_free( obj->builder );
	obj->builder = NULL;
}

template<int StackSize>
int intersect(SVBVHTree *obj, Isect* isec)
{
	//TODO renable hint support
	if(RE_rayobject_isAligned(obj->root))
		return bvh_node_stack_raycast<SVBVHNode,StackSize,false>( obj->root, isec);
	else
		return RE_rayobject_intersect( (RayObject*) obj->root, isec );
}

template<class Tree>
void bvh_hint_bb(Tree *tree, LCTSHint *hint, float *min, float *max)
{
	//TODO renable hint support
	{
	 	hint->size = 0;
	 	hint->stack[hint->size++] = (RayObject*)tree->root;
	}
}
/* the cast to pointer function is needed to workarround gcc bug: http://gcc.gnu.org/bugzilla/show_bug.cgi?id=11407 */
template<class Tree, int STACK_SIZE>
RayObjectAPI make_api()
{
	static RayObjectAPI api = 
	{
		(RE_rayobject_raycast_callback) ((int(*)(Tree*,Isect*)) &intersect<STACK_SIZE>),
		(RE_rayobject_add_callback)     ((void(*)(Tree*,RayObject*)) &bvh_add<Tree>),
		(RE_rayobject_done_callback)    ((void(*)(Tree*))       &bvh_done<Tree>),
		(RE_rayobject_free_callback)    ((void(*)(Tree*))       &bvh_free<Tree>),
		(RE_rayobject_merge_bb_callback)((void(*)(Tree*,float*,float*)) &bvh_bb<Tree>),
		(RE_rayobject_cost_callback)	((float(*)(Tree*))      &bvh_cost<Tree>),
		(RE_rayobject_hint_bb_callback)	((void(*)(Tree*,LCTSHint*,float*,float*)) &bvh_hint_bb<Tree>)
	};
	
	return api;
}

template<class Tree>
RayObjectAPI* bvh_get_api(int maxstacksize)
{
	static RayObjectAPI bvh_api256 = make_api<Tree,1024>();
	
	if(maxstacksize <= 1024) return &bvh_api256;
	assert(maxstacksize <= 256);
	return 0;
}

RayObject *RE_rayobject_svbvh_create(int size)
{
	return bvh_create_tree<SVBVHTree,DFS_STACK_SIZE>(size);
}
