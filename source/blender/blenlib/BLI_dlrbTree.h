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

#ifndef __BLI_DLRBTREE_H__
#define __BLI_DLRBTREE_H__

/** \file BLI_dlrbTree.h
 *  \ingroup bli
 *  \author Joshua Leung
 */

/* Double-Linked Red-Black Tree Implementation:
 * 
 * This is simply a Red-Black Tree implementation whose nodes can later
 * be arranged + retrieved as elements in a Double-Linked list (i.e. ListBase).
 * The Red-Black Tree implementation is based on the methods defined by Wikipedia.
 */

/* ********************************************** */
/* Data Types and Type Defines  */

/* Base Structs --------------------------------- */

/* Basic Layout for a Node */
typedef struct DLRBT_Node {
	/* ListBase capabilities */
	struct DLRBT_Node *next, *prev;		
	
	/* Tree Associativity settings */
	struct DLRBT_Node *left, *right;
	struct DLRBT_Node *parent;
	
	char tree_col;
	/* ... for nice alignment, next item should usually be a char too... */
} DLRBT_Node;

/* Red/Black defines for tree_col */
typedef enum eDLRBT_Colors {
	DLRBT_BLACK= 0,
	DLRBT_RED,
} eDLRBT_Colors;

/* -------- */

/* The Tree Data */
typedef struct DLRBT_Tree {
	/* ListBase capabilities */
	void *first, *last;			/* these should be based on DLRBT_Node-s */

	/* Root Node */
	void *root;					/* this should be based on DLRBT_Node-s */
} DLRBT_Tree;

/* Callback Types --------------------------------- */

/* return -1, 0, 1 for whether the given data is less than, equal to, or greater than the given node 
 *	- node: <DLRBT_Node> the node to compare to
 *	- data: pointer to the relevant data or values stored in the bitpattern dependent on the function
 */
typedef short (*DLRBT_Comparator_FP)(void *node, void *data);

/* return a new node instance wrapping the given data 
 *	- data: pointer to the relevant data to create a subclass of node from
 */
typedef DLRBT_Node *(*DLRBT_NAlloc_FP)(void *data);

/* update an existing node instance accordingly to be in sync with the given data *	
 * 	- node: <DLRBT_Node> the node to update
 *	- data: pointer to the relevant data or values stored in the bitpattern dependent on the function
 */
typedef void (*DLRBT_NUpdate_FP)(void *node, void *data);

/* ********************************************** */
/* Public API */

/* ADT Management ------------------------------- */

/* Create a new tree, and initialise as necessary */
DLRBT_Tree *BLI_dlrbTree_new(void);

/* Initialises some given trees */
void BLI_dlrbTree_init(DLRBT_Tree *tree);

/* Free some tree */
void BLI_dlrbTree_free(DLRBT_Tree *tree);

/* Make sure the tree's Double-Linked list representation is valid */
void BLI_dlrbTree_linkedlist_sync(DLRBT_Tree *tree);


/* Searching ------------------------------------ */

/* Find the node which matches or is the closest to the requested node */
DLRBT_Node *BLI_dlrbTree_search(DLRBT_Tree *tree, DLRBT_Comparator_FP cmp_cb, void *search_data);

/* Find the node which exactly matches the required data */
DLRBT_Node *BLI_dlrbTree_search_exact(DLRBT_Tree *tree, DLRBT_Comparator_FP cmp_cb, void *search_data);

/* Find the node which occurs immediately before the best matching node */
DLRBT_Node *BLI_dlrbTree_search_prev(DLRBT_Tree *tree, DLRBT_Comparator_FP cmp_cb, void *search_data);

/* Find the node which occurs immediately after the best matching node */
DLRBT_Node *BLI_dlrbTree_search_next(DLRBT_Tree *tree, DLRBT_Comparator_FP cmp_cb, void *search_data);


/* Check whether there is a node matching the requested node */
short BLI_dlrbTree_contains(DLRBT_Tree *tree, DLRBT_Comparator_FP cmp_cb, void *search_data);


/* Node Operations (Managed) --------------------- */
/* These methods automate the process of adding/removing nodes from the BST, 
 * using the supplied data and callbacks
 */

/* Add the given data to the tree, and return the node added */
// NOTE: for duplicates, the update_cb is called (if available), and the existing node is returned
DLRBT_Node *BLI_dlrbTree_add(DLRBT_Tree *tree, DLRBT_Comparator_FP cmp_cb, 
			DLRBT_NAlloc_FP new_cb, DLRBT_NUpdate_FP update_cb, void *data);


/* Remove the given element from the tree and balance again */
// FIXME: this is not implemented yet... 
// void BLI_dlrbTree_remove(DLRBT_Tree *tree, DLRBT_Node *node);

/* Node Operations (Manual) --------------------- */
/* These methods require custom code for creating BST nodes and adding them to the 
 * tree in special ways, such that the node can then be balanced.
 *
 * It is recommended that these methods are only used where the other method is too cumbersome...
 */

/* Balance the tree after the given node has been added to it 
 * (using custom code, in the Binary Tree way).
 */
void BLI_dlrbTree_insert(DLRBT_Tree *tree, DLRBT_Node *node);

/* ********************************************** */

#endif // __BLI_DLRBTREE_H__
