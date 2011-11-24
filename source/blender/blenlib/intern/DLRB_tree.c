/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 *
 * Contributor(s): Joshua Leung (original author)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/DLRB_tree.c
 *  \ingroup bli
 */


#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_dlrbTree.h"

/* *********************************************** */
/* Tree API */

/* Create a new tree, and initialise as necessary */
DLRBT_Tree *BLI_dlrbTree_new (void)
{
	/* just allocate for now */
	return MEM_callocN(sizeof(DLRBT_Tree), "DLRBT_Tree");
}

/* Just zero out the pointers used */
void BLI_dlrbTree_init (DLRBT_Tree *tree) 
{
	if (tree == NULL)
		return;
		
	tree->first= tree->last= tree->root= NULL;
}

/* Helper for traversing tree and freeing sub-nodes */
static void recursive_tree_free_nodes (DLRBT_Node *node)
{
	/* sanity check */
	if (node == NULL)
		return;
	
	/* free child nodes + subtrees */
	recursive_tree_free_nodes(node->left);
	recursive_tree_free_nodes(node->right);
	
	/* free self */
	MEM_freeN(node);
}

/* Free the given tree's data but not the tree itself */
void BLI_dlrbTree_free (DLRBT_Tree *tree)
{
	if (tree == NULL)
		return;
	
	/* if the list-base stuff is set, just use that (and assume its set), 
	 * otherwise, we'll need to traverse the tree...
	 */
	if (tree->first) {
		/* free list */
		BLI_freelistN((ListBase *)tree);
	}
	else {
		/* traverse tree, freeing sub-nodes */
		recursive_tree_free_nodes(tree->root);
	}
	
	/* clear pointers */
	tree->first= tree->last= tree->root= NULL;
}

/* ------- */

/* Helper function - used for traversing down the tree from the root to add nodes in order */
static void linkedlist_sync_add_node (DLRBT_Tree *tree, DLRBT_Node *node)
{
	/* sanity checks */
	if ((tree == NULL) || (node == NULL))
		return;
	
	/* add left-node (and its subtree) */
	linkedlist_sync_add_node(tree, node->left);
	
	/* now add self
	 *	- must remove detach from other links first
	 *	  (for now, only clear own pointers)
	 */
	node->prev= node->next= NULL;
	BLI_addtail((ListBase *)tree, (Link *)node);
	
	/* finally, add right node (and its subtree) */
	linkedlist_sync_add_node(tree, node->right);
}

/* Make sure the tree's Double-Linked list representation is valid */
void BLI_dlrbTree_linkedlist_sync (DLRBT_Tree *tree)
{
	/* sanity checks */
	if (tree == NULL)
		return;
		
	/* clear list-base pointers so that the new list can be added properly */
	tree->first= tree->last= NULL;
	
	/* start adding items from the root */
	linkedlist_sync_add_node(tree, tree->root);
}

/* *********************************************** */
/* Tree Search Utilities */

/* Find the node which matches or is the closest to the requested node */
DLRBT_Node *BLI_dlrbTree_search (DLRBT_Tree *tree, DLRBT_Comparator_FP cmp_cb, void *search_data)
{
	DLRBT_Node *node = (tree) ? tree->root : NULL;
	short found= 0;
	
	/* check that there is a comparator to use */
	// TODO: if no comparator is supplied, try using the one supplied with the tree...
	if (cmp_cb == NULL)
		return NULL;
	
	/* iteratively perform this search */
	while (node && found==0) 
	{
		/* check if traverse further or not 
		 * NOTE: it is assumed that the values will be unit values only
		 */
		switch (cmp_cb(node, search_data)) {
			case -1: 	/* data less than node */
				if (node->left)
					node= node->left;
				else
					found= 1;
				break;
			
			case 1: 	/* data greater than node */
				if (node->right)
					node= node->right;
				else
					found= 1;
				break;
			
			default: 	/* data equals node */
				found= 1;
				break;
		}
	}
	
	/* return the nearest matching node */
	return node;
} 

/* Find the node which exactly matches the required data */
DLRBT_Node *BLI_dlrbTree_search_exact (DLRBT_Tree *tree, DLRBT_Comparator_FP cmp_cb, void *search_data)
{
	DLRBT_Node *node = (tree) ? tree->root : NULL;
	short found= 0;
	
	/* check that there is a comparator to use */
	// TODO: if no comparator is supplied, try using the one supplied with the tree...
	if (cmp_cb == NULL)
		return NULL;
	
	/* iteratively perform this search */
	while (node && found==0) 
	{
		/* check if traverse further or not 
		 * NOTE: it is assumed that the values will be unit values only
		 */
		switch (cmp_cb(node, search_data)) {
			case -1: 	/* data less than node */
				if (node->left)
					node= node->left;
				else
					found= -1;
				break;
			
			case 1: 	/* data greater than node */
				if (node->right)
					node= node->right;
				else
					found= -1;
				break;
			
			default: 	/* data equals node */
				found= 1;
				break;
		}
	}
	
	/* return the exactly matching node */
	return (found == 1) ? (node) : (NULL);
}

/* Find the node which occurs immediately before the best matching node */
DLRBT_Node *BLI_dlrbTree_search_prev (DLRBT_Tree *tree, DLRBT_Comparator_FP cmp_cb, void *search_data)
{
	DLRBT_Node *node;
	
	/* check that there is a comparator to use */
	// TODO: if no comparator is supplied, try using the one supplied with the tree...
	if (cmp_cb == NULL)
		return NULL;
	
	/* get the node which best matches this description */
	node= BLI_dlrbTree_search(tree, cmp_cb, search_data);
	
	if (node) {
		/* if the item we're searching for is greater than the node found, we've found the match */
		if (cmp_cb(node, search_data) > 0)
			return node;
		
		/* return the previous node otherwise */
		// NOTE: what happens if there is no previous node?
		return node->prev;
	}
	
	/* nothing matching was found */
	return NULL;
}

/* Find the node which occurs immediately after the best matching node */
DLRBT_Node *BLI_dlrbTree_search_next (DLRBT_Tree *tree, DLRBT_Comparator_FP cmp_cb, void *search_data)
{
	DLRBT_Node *node;
	
	/* check that there is a comparator to use */
	// TODO: if no comparator is supplied, try using the one supplied with the tree...
	if (cmp_cb == NULL)
		return NULL;
	
	/* get the node which best matches this description */
	node= BLI_dlrbTree_search(tree, cmp_cb, search_data);
	
	if (node) {
		/* if the item we're searching for is less than the node found, we've found the match */
		if (cmp_cb(node, search_data) < 0)
			return node;
		
		/* return the previous node otherwise */
		// NOTE: what happens if there is no previous node?
		return node->next;
	}
	
	/* nothing matching was found */
	return NULL;
}


/* Check whether there is a node matching the requested node */
short BLI_dlrbTree_contains(DLRBT_Tree *tree, DLRBT_Comparator_FP cmp_cb, void *search_data)
{
	/* check if an exact search throws up anything... */
	return (BLI_dlrbTree_search_exact(tree, cmp_cb, search_data) != NULL);
}

/* *********************************************** */
/* Tree Relationships Utilities */

/* get the 'grandparent' - the parent of the parent - of the given node */
static DLRBT_Node *get_grandparent (DLRBT_Node *node)
{
	if (node && node->parent)
		return node->parent->parent;
	else
		return NULL;
}

/* get the sibling node (e.g. if node is left child of parent, return right child of parent) */
static DLRBT_Node *get_sibling(DLRBT_Node *node)
{
	if (node && node->parent) {
		if (node == node->parent->left)
			return node->parent->right;
		else
			return node->parent->left;
	}

	/* sibling not found */
	return NULL;
}

/* get the 'uncle' - the sibling of the parent - of the given node */
static DLRBT_Node *get_uncle (DLRBT_Node *node)
{
	if (node)
		/* return the child of the grandparent which isn't the node's parent */
		return get_sibling(node->parent);
	
	/* uncle not found */
	return NULL;
}

/* *********************************************** */
/* Tree Rotation Utilities */

/* make right child of 'root' the new root */
static void rotate_left (DLRBT_Tree *tree, DLRBT_Node *root)
{
	DLRBT_Node **root_slot, *pivot;
	
	/* pivot is simply the root's right child, to become the root's parent */
	pivot= root->right;
	if (pivot == NULL)
		return;
	
	if (root->parent) {
		if (root == root->parent->left)
			root_slot= &root->parent->left;
		else
			root_slot= &root->parent->right;
	}
	else
		root_slot= ((DLRBT_Node**)&tree->root);//&((DLRBT_Node*)tree->root);
		
	/* - pivot's left child becomes root's right child
	 * - root now becomes pivot's left child  
	 */
	root->right= pivot->left;	
	if (pivot->left) pivot->left->parent= root;
	
	pivot->left= root;
	pivot->parent= root->parent;
	root->parent= pivot;
	
	/* make the pivot the new root */
	if (root_slot)
		*root_slot= pivot;
}

/* make the left child of the 'root' the new root */
static void rotate_right (DLRBT_Tree *tree, DLRBT_Node *root)
{
	DLRBT_Node **root_slot, *pivot;
	
	/* pivot is simply the root's left child, to become the root's parent */
	pivot= root->left;
	if (pivot == NULL)
		return;
	
	if (root->parent) {
		if (root == root->parent->left)
			root_slot= &root->parent->left;
		else
			root_slot= &root->parent->right;
	}
	else
		root_slot= ((DLRBT_Node**)&tree->root);//&((DLRBT_Node*)tree->root);
		
	/* - pivot's right child becomes root's left child
	 * - root now becomes pivot's right child  
	 */
	root->left= pivot->right;	
	if (pivot->right) pivot->right->parent= root;
	
	pivot->right= root;
	pivot->parent= root->parent;
	root->parent= pivot;
	
	/* make the pivot the new root */
	if (root_slot)
		*root_slot= pivot;
}

/* *********************************************** */
/* Post-Insertion Balancing  */

/* forward defines for insertion checks */
static void insert_check_1(DLRBT_Tree *tree, DLRBT_Node *node);
static void insert_check_2(DLRBT_Tree *tree, DLRBT_Node *node);
static void insert_check_3(DLRBT_Tree *tree, DLRBT_Node *node);

/* ----- */

/* W. 1) Root must be black (so that the 2nd-generation can have a black parent) */
static void insert_check_1 (DLRBT_Tree *tree, DLRBT_Node *node)
{
	if (node) {
		/* if this is the root, just ensure that it is black */
		if (node->parent == NULL)
			node->tree_col= DLRBT_BLACK;
		else
			insert_check_2(tree, node);
	}
}

/* W. 2+3) Parent of node must be black, otherwise recolor and flush */
static void insert_check_2 (DLRBT_Tree *tree, DLRBT_Node *node)
{
	/* if the parent is not black, we need to change that... */
	if (node && node->parent && node->parent->tree_col) {
		DLRBT_Node *unc= get_uncle(node);
		
		/* if uncle and parent are both red, need to change them to black and make 
		 * the parent black in order to satisfy the criteria of each node having the
		 * same number of black nodes to its leaves
		 */
		if (unc && unc->tree_col) {
			DLRBT_Node *gp= get_grandparent(node);
			
			/* make the n-1 generation nodes black */
			node->parent->tree_col= unc->tree_col= DLRBT_BLACK;
			
			/* - make the grandparent red, so that we maintain alternating red/black property 
			 *  (it must exist, so no need to check for NULL here),
			 * - as the grandparent may now cause inconsistencies with the rest of the tree, 
			 *   we must flush up the tree and perform checks/rebalancing/repainting, using the 
			 * 	grandparent as the node of interest
			 */
			gp->tree_col= DLRBT_RED;
			insert_check_1(tree, gp);
		}
		else {
			/* we've got an unbalanced branch going down the grandparent to the parent,
			 * so need to perform some rotations to re-balance the tree
			 */
			insert_check_3(tree, node);
		}
	}
}

/* W. 4+5) Perform rotation on sub-tree containing the 'new' node, then do any  */
static void insert_check_3 (DLRBT_Tree *tree, DLRBT_Node *node)
{
	DLRBT_Node *gp= get_grandparent(node);
	
	/* check that grandparent and node->parent exist (jut in case... really shouldn't happen on a good tree) */
	if (node && node->parent && gp) {
		/* a left rotation will switch the roles of node and its parent, assuming that
		 * the parent is the left child of the grandparent... otherwise, rotation direction
		 * should be swapped
		 */
		if ((node == node->parent->right) && (node->parent == gp->left)) {
			rotate_left(tree, node);
			node= node->left;
		}
		else if ((node == node->parent->left) && (node->parent == gp->right)) {
			rotate_right(tree, node); 
			node= node->right;
		}
		
		/* fix old parent's color-tagging, and perform rotation on the old parent in the 
		 * opposite direction if needed for the current situation
		 * NOTE: in the code above, node pointer is changed to point to the old parent 
		 */
		if (node) {
			/* get 'new' grandparent (i.e. grandparent for old-parent (node)) */
			gp= get_grandparent(node);
			
			/* modify the coloring of the grandparent and parent so that they still satisfy the constraints */
			node->parent->tree_col= DLRBT_BLACK;
			gp->tree_col= DLRBT_RED;
			
			/* if there are several nodes that all form a left chain, do a right rotation to correct this
			 * (or a rotation in the opposite direction if they all form a right chain)
			 */
			if ((node == node->parent->left) && (node->parent == gp->left))
				rotate_right(tree, gp);
			else //if ((node == node->parent->right) && (node->parent == gp->right))
				rotate_left(tree, gp);
		}
	}
}

/* ----- */

/* Balance the tree after the given element has been added to it 
 * (using custom code, in the Binary Tree way).
 */
void BLI_dlrbTree_insert (DLRBT_Tree *tree, DLRBT_Node *node)
{
	/* sanity checks */
	if ((tree == NULL) || (node == NULL))
		return;
		
	/* firstly, the node we just added should be red by default */
	node->tree_col= DLRBT_RED;
		
	/* start from case 1, an trek through the tail-recursive insertion checks */
	insert_check_1(tree, node);
}

/* ----- */

/* Add the given data to the tree, and return the node added */
// NOTE: for duplicates, the update_cb is called (if available), and the existing node is returned
DLRBT_Node *BLI_dlrbTree_add(DLRBT_Tree *tree, DLRBT_Comparator_FP cmp_cb, 
			DLRBT_NAlloc_FP new_cb, DLRBT_NUpdate_FP update_cb, void *data)
{
	DLRBT_Node *parNode, *node=NULL;
	short new_node = 0;
	
	/* sanity checks */
	if (tree == NULL)
		return NULL;
		
	// TODO: if no comparator is supplied, try using the one supplied with the tree...
	if (cmp_cb == NULL)
		return NULL;
	// TODO: if no allocator is supplied, try using the one supplied with the tree...
	if (new_cb == NULL)
		return NULL;
	// TODO: if no updater is supplied, try using the one supplied with the tree...
		
	/* try to find the nearest node to this one */
	parNode= BLI_dlrbTree_search(tree, cmp_cb, data);
	
	/* add new node to the BST in the 'standard way' as appropriate 
	 * NOTE: we do not support duplicates in our tree...
	 */
	if (parNode) {
		/* check how this new node compares with the existing ones 
		 * NOTE: it is assumed that the values will be unit values only
		 */
		switch (cmp_cb(parNode, data)) {
			case -1: 	/* add new node as left child */
			{
				node= new_cb(data);
				new_node= 1;
				
				parNode->left= node;
				node->parent= parNode;
			}
				break;
			
			case 1: 	/* add new node as right child */
			{
				node= new_cb(data);
				new_node= 1;
				
				parNode->right= node;
				node->parent= parNode;
			}
				break;
			
			default: 	/* update the duplicate node as appropriate */
			{
				if (update_cb)
					update_cb(parNode, data);
			}
				break;
		}
	}
	else {
		/* no nodes in the tree yet... add a new node as the root */
		node= new_cb(data);
		new_node= 1;
		
		tree->root= node;
	}
	
	/* if a new node was added, it should be tagged as red, and then balanced as appropriate */
	if (new_node) {
		/* tag this new node as being 'red' */
		node->tree_col= DLRBT_RED;
		
		/* perform BST balancing steps:
		 * 	start from case 1, an trek through the tail-recursive insertion checks
		 */
		insert_check_1(tree, node);
	}
	
	/* return the node added */
	return node;
}

/* *********************************************** */
/* Remove */

// TODO: this hasn't been coded yet, since this functionality was not needed by the author

/* *********************************************** */
