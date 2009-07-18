/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 *
 * Contributor(s): Joshua Leung (original author)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BLI_DLRB_TREE_H
#define BLI_DLRB_TREE_H

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
enum eDLRBT_Colors {
	DLRBT_BLACK= 0,
	DLRBT_RED,
};

/* -------- */

/* The Tree Data */
typedef struct DLRBT_Tree {
	/* ListBase capabilities */
	void *first, *last;			/* these should be based on DLRBT_Node-s */

	/* Root Node */
	void *root;					/* this should be based on DLRBT_Node-s */
} DLRBT_Tree;

/* ********************************************** */
/* Public API */

/* Create a new tree, and initialise as necessary */
DLRBT_Tree *BLI_dlrbTree_new(void);

/* Initialises some given trees */
void BLI_dlrbTree_init(DLRBT_Tree *tree);

/* Free some tree */
void BLI_dlrbTree_free(DLRBT_Tree *tree);

/* Make sure the tree's Double-Linked list representation is valid */
void BLI_dlrbTree_linkedlist_sync(DLRBT_Tree *tree);



/* Balance the tree after the given element has been added to it 
 * (using custom code, in the Binary Tree way).
 */
void BLI_dlrbTree_insert(DLRBT_Tree *tree, DLRBT_Node *node);

/* Remove the given element from the tree and balance again */
// FIXME: this is not implemented yet... 
void BLI_dlrbTree_remove(DLRBT_Tree *tree, DLRBT_Node *node);

/* ********************************************** */

#endif // BLI_DLRB_TREE_H
