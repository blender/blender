
/* bvh tree generics */
template<class Tree> static int bvh_intersect(Tree *obj, Isect *isec);

template<class Tree> static void bvh_add(Tree *obj, RayObject *ob)
{
	rtbuild_add( obj->builder, ob );
}

template<class Tree> static void bvh_done(Tree *obj);

template<class Tree>
static void bvh_free(Tree *obj)
{
	if(obj->builder)
		rtbuild_free(obj->builder);

	if(obj->node_arena)
		BLI_memarena_free(obj->node_arena);

	MEM_freeN(obj);
}

template<class Tree>
static void bvh_bb(Tree *obj, float *min, float *max)
{
	bvh_node_merge_bb(obj->root, min, max);
}


template<class Tree>
static float bvh_cost(Tree *obj)
{
	assert(obj->cost >= 0.0);
	return obj->cost;
}



/* bvh tree nodes generics */
template<class Node> static inline int bvh_node_hit_test(Node *node, Isect *isec)
{
	return RE_rayobject_bb_intersect(isec, (const float*)node->bb) != FLT_MAX;
}


template<class Node>
static void bvh_node_merge_bb(Node *node, float *min, float *max)
{
	if(RayObject_isAligned(node))
	{
		DO_MIN(node->bb  , min);
		DO_MAX(node->bb+3, max);
	}
	else
	{
		RE_rayobject_merge_bb( (RayObject*)node, min, max);
	}
}



/*
 * recursivly transverse a BVH looking for a rayhit using a local stack
 */
template<class Node> static inline void bvh_node_push_childs(Node *node, Isect *isec, Node **stack, int &stack_pos);

template<class Node,int MAX_STACK_SIZE>
static int bvh_node_stack_raycast(Node *root, Isect *isec)
{
	Node *stack[MAX_STACK_SIZE];
	int hit = 0, stack_pos = 0;
		
	stack[stack_pos++] = root;
	while(stack_pos)
	{
		Node *node = stack[--stack_pos];
		if(RayObject_isAligned(node))
		{
			if(bvh_node_hit_test(node,isec))
			{
				bvh_node_push_childs(node, isec, stack, stack_pos);
				assert(stack_pos <= MAX_STACK_SIZE);
			}
		}
		else
		{
			hit |= RE_rayobject_intersect( (RayObject*)node, isec);
			if(hit && isec->mode == RE_RAY_SHADOW) return hit;
		}
	}
	return hit;

}

/*
 * recursively transverse a BVH looking for a rayhit using system stack
 */
/*
template<class Node>
static int bvh_node_raycast(Node *node, Isect *isec)
{
	int hit = 0;
	if(bvh_test_node(node, isec))
	{
		if(isec->idot_axis[node->split_axis] > 0.0f)
		{
			int i;
			for(i=0; i<BVH_NCHILDS; i++)
				if(RayObject_isAligned(node->child[i]))
				{
					if(node->child[i] == 0) break;
					
					hit |= bvh_node_raycast(node->child[i], isec);
					if(hit && isec->mode == RE_RAY_SHADOW) return hit;
				}
				else
				{
					hit |= RE_rayobject_intersect( (RayObject*)node->child[i], isec);
					if(hit && isec->mode == RE_RAY_SHADOW) return hit;
				}
		}
		else
		{
			int i;
			for(i=BVH_NCHILDS-1; i>=0; i--)
				if(RayObject_isAligned(node->child[i]))
				{
					if(node->child[i])
					{
						hit |= dfs_raycast(node->child[i], isec);
						if(hit && isec->mode == RE_RAY_SHADOW) return hit;
					}
				}
				else
				{
					hit |= RE_rayobject_intersect( (RayObject*)node->child[i], isec);
					if(hit && isec->mode == RE_RAY_SHADOW) return hit;
				}
		}
	}
	return hit;
}
*/

/* bvh tree generics */
template<class Tree> static int bvh_intersect(Tree *obj, Isect *isec);

template<class Tree> static void bvh_add(Tree *obj, RayObject *ob)
{
	rtbuild_add( obj->builder, ob );
}

template<class Tree> static void bvh_done(Tree *obj);

template<class Tree>
static void bvh_free(Tree *obj)
{
	if(obj->builder)
		rtbuild_free(obj->builder);

	if(obj->node_arena)
		BLI_memarena_free(obj->node_arena);

	MEM_freeN(obj);
}

template<class Tree>
static void bvh_bb(Tree *obj, float *min, float *max)
{
	bvh_node_merge_bb(obj->root, min, max);
}


template<class Tree>
static float bvh_cost(Tree *obj)
{
	assert(obj->cost >= 0.0);
	return obj->cost;
}



/* bvh tree nodes generics */
template<class Node> static inline int bvh_node_hit_test(Node *node, Isect *isec)
{
	return RE_rayobject_bb_intersect(isec, (const float*)node->bb) != FLT_MAX;
}


template<class Node>
static void bvh_node_merge_bb(Node *node, float *min, float *max)
{
	if(RayObject_isAligned(node))
	{
		DO_MIN(node->bb  , min);
		DO_MAX(node->bb+3, max);
	}
	else
	{
		RE_rayobject_merge_bb( (RayObject*)node, min, max);
	}
}



/*
 * recursivly transverse a BVH looking for a rayhit using a local stack
 */
template<class Node> static inline void bvh_node_push_childs(Node *node, Isect *isec, Node **stack, int &stack_pos);

template<class Node,int MAX_STACK_SIZE>
static int bvh_node_stack_raycast(Node *root, Isect *isec)
{
	Node *stack[MAX_STACK_SIZE];
	int hit = 0, stack_pos = 0;
		
	stack[stack_pos++] = root;
	while(stack_pos)
	{
		Node *node = stack[--stack_pos];
		if(RayObject_isAligned(node))
		{
			if(bvh_node_hit_test(node,isec))
			{
				bvh_node_push_childs(node, isec, stack, stack_pos);
				assert(stack_pos <= MAX_STACK_SIZE);
			}
		}
		else
		{
			hit |= RE_rayobject_intersect( (RayObject*)node, isec);
			if(hit && isec->mode == RE_RAY_SHADOW) return hit;
		}
	}
	return hit;

}

/*
 * recursively transverse a BVH looking for a rayhit using system stack
 */
/*
template<class Node>
static int bvh_node_raycast(Node *node, Isect *isec)
{
	int hit = 0;
	if(bvh_test_node(node, isec))
	{
		if(isec->idot_axis[node->split_axis] > 0.0f)
		{
			int i;
			for(i=0; i<BVH_NCHILDS; i++)
				if(RayObject_isAligned(node->child[i]))
				{
					if(node->child[i] == 0) break;
					
					hit |= bvh_node_raycast(node->child[i], isec);
					if(hit && isec->mode == RE_RAY_SHADOW) return hit;
				}
				else
				{
					hit |= RE_rayobject_intersect( (RayObject*)node->child[i], isec);
					if(hit && isec->mode == RE_RAY_SHADOW) return hit;
				}
		}
		else
		{
			int i;
			for(i=BVH_NCHILDS-1; i>=0; i--)
				if(RayObject_isAligned(node->child[i]))
				{
					if(node->child[i])
					{
						hit |= dfs_raycast(node->child[i], isec);
						if(hit && isec->mode == RE_RAY_SHADOW) return hit;
					}
				}
				else
				{
					hit |= RE_rayobject_intersect( (RayObject*)node->child[i], isec);
					if(hit && isec->mode == RE_RAY_SHADOW) return hit;
				}
		}
	}
	return hit;
}
*/
