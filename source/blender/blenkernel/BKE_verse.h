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
 * Contributor(s): Jiri Hnidek.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* #define WITH_VERSE */

#ifndef BKE_VERSE_H
#define BKE_VERSE_H

#include "DNA_listBase.h"
#include "BLI_dynamiclist.h"

#include "verse.h"
#include "verse_ms.h"

struct VNode;
struct VerseEdge;

/*
 * Verse Edge Hash (similar to edit edge hash)
 */
#define VEDHASHSIZE    (512*512)
#define VEDHASH(a, b)  ((a<b ? a : b) % VEDHASHSIZE)

/*
 * verse data: 4 float value
 */
typedef struct quat_real32_item {
	struct quat_real32_item *next, *prev;
	struct VLayer *vlayer;		/* pointer at VerseLayer */
	uint32 id;			/* id of item */
	real32 value[4];
} quat_real32_item;

/*
 * verse data: 4 uint32 values
 */
typedef struct quat_uint32_item {
	struct quat_uint32_item *next, *prev;
	struct VLayer *vlayer;		/* pointer at VerseLayer */
	uint32 id;			/* id of item */
	uint32 value[4];
} quat_uint32_item;

/*
 * verse data: 3 float values
 */
typedef struct vec_real32_item {
	struct vec_real32_item *next, *prev;
	struct VLayer *vlayer;		/* pointer at VerseLayer */
	uint32 id;			/* id of item */
	real32 value[3];
} vec_real32_item;

/*
 * verse data: float value (weight)
 */
typedef struct real32_item {
	struct real32_item *next, *prev;
	struct VLayer *vlayer;		/* pointer at VerseLayer */
	uint32 id;			/* id of item */
	real32 value;
} real32_item;

/*
 * verse data: uint32 value
 */
typedef struct uint32_item {
	struct uint32_item *next, *prev;
	struct VLayer *vlayer;		/* pointer at VerseLayer */
	uint32 id;			/* id of item */
	uint32 value;
} uint32_item;

/*
 * verse data: uint8 value
 */
typedef struct uint8_item {
	struct uint8_item *next, *prev;
	struct VLayer *vlayer;		/* pointer at VerseLayer */
	uint32 id;			/* id of item */
	uint8 value;
} uint8_item;

/*
 * verse data: vertex
 */
typedef struct VerseVert {
	struct VerseVert *next, *prev;
	/* verse data */
	struct VLayer *vlayer;		/* pointer at VerseLayer */
	uint32 id;			/* id of vertex */
	real32 co[3];			/* x,y,z-coordinates of vertex */
	real32 no[3];			/* normal of vertex */
	/* blender internals */
	short flag;			/* flags: VERT_DELETED, VERT_RECEIVED, etc. */
	void *vertex;			/* pointer at EditVert or MVert */
	int counter;			/* counter of VerseFaces using this VerseVert */
	union {
		unsigned int index;	/* counter need during transformation to mesh */
		struct VerseVert *vvert;
	} tmp;				/* pointer at new created verse vert, it is
					 * used during duplicating geometry node */	
	float *cos;			/* modified coordinates of vertex */
	float *nos;			/* modified normal vector */
} VerseVert;

/*
 * structture used for verse edge hash
 */
typedef struct HashVerseEdge {
	struct VerseEdge *vedge;
	struct HashVerseEdge *next;
} HashVerseEdge;

/*
 * fake verse data: edge
 */
typedef struct VerseEdge {
	struct VerseEdge *next, *prev;
	uint32 v0, v1;			/* indexes of verse vertexes */
	int counter;			/* counter of verse faces using this edge */
	struct HashVerseEdge hash;	/* hash table */
	union {
		unsigned int index;	/* temporary index of edge */
	} tmp;
} VerseEdge;

/*
 * verse data: polygon
 */
typedef struct VerseFace {
	struct VerseFace *next, *prev;
	/* verse data */
	struct VLayer *vlayer;		/* pointer at VerseLayer */
	uint32 id;			/* id of face */
	struct VerseVert *vvert0;	/* pointer at 1st VerseVert */
	struct VerseVert *vvert1;	/* pointer at 2nd VerseVert */
	struct VerseVert *vvert2;	/* pointer at 3th VerseVert */
	struct VerseVert *vvert3;	/* pointer at 4th VerseVert */
	unsigned int v0, v1, v2, v3;	/* indexes of VerseVerts ... needed during receiving */
	/* blender internals */
	char flag;			/* flags: FACE_SEND_READY, FACE_SENT, FACE_RECEIVED, FACE_CHANGED*/
	short counter;			/* counter of missed VerseVertexes */
	void *face;			/* pointer at EditFace */
	float no[3];			/* normal vector */
	float *nos;			/* modified normal vector */
} VerseFace;

/*
 * verse data: layer
 */
typedef struct VLayer {
	struct VLayer *next, *prev;
	/* verse data*/
	struct VNode *vnode;		/* pointer at VerseNode */
	uint16 id;			/* id of layer */
	char *name;			/* name of layer */
	VNGLayerType type;		/* type of layer (VN_G_LAYER_VERTEX_XYZ, VN_G_LAYER_POLYGON_CORNER_UINT32) */
	uint32 def_int;			/* default integer value */
	real64 def_real;		/* default float value */
	/* blender internals */
	char flag;			/* flags: LAYER_SENT, LAYER_RECEIVED, LAYER_DELETED, LAYER_OBSOLETE */
	short content;			/* type of content (VERTEX_LAYER, POLYGON_LAYER) */
	struct DynamicList dl;		/* vertexes, polygons, etc. */
	struct ListBase queue;		/* queue of vertexes, polygons, etc. waiting for sending to verse server */
	struct ListBase orphans;	/* list of versedata (polygons, etc.), that can be added to the DynamicList
					 * due to not received VerseVerts */
	unsigned int counter;		/* counter of sent items */
	/* client dependent methods */
	void (*post_layer_create)(struct VLayer *vlayer);
	void (*post_layer_destroy)(struct VLayer *vlayer);
} VLayer;

/*
 * verse data: link
 */
typedef struct VLink{
	struct VLink *next, *prev;
	/* verse data */
	struct VerseSession *session;	/* session pointer */
	struct VNode *source;		/* object VerseNode "pointing" at some other VerseNode */
	struct VNode *target;		/* VerseNode linked with some object node */
	unsigned int id;		/* id of VerseLink */
	unsigned int target_id;		/* some unknow id */
	char *label;			/* name/label of VerseLink */
	/* blender internals */
	char flag;			/* flags: LINK_SEND_READY */
	/* client dependent methods */
	void (*post_link_set)(struct VLink *vlink);
	void (*post_link_destroy)(struct VLink *vlink);
} VLink;

/*
 * bitmap layer 
 */
typedef struct VBitmapLayer {
	struct VBitmapLayer *next, *prev;
	/* verse data */
	struct VNode *vnode;		/* pointer at Verse Node */
	VLayerID id;			/* id of layer */
	char *name;			/* name of layer */
	VNBLayerType type;		/* type of layer (bits per channel) 1, 8, 16, 32, 64 */
	void *data;			/* dynamic allocated data */
	/* blender internals */
	char flag;
} VBitmapLayer;

/*
 * data of bitmap node
 */
typedef struct VBitmapData {
	struct DynamicList layers;	/* dynamic list with access array of bitmap layers */
	struct ListBase queue;		/* queue of layers waiting for receiving from verse server */
	uint16 width;			/* width of all verse layers */
	uint16 height;			/* height of all verse layers */
	uint16 depth;			/* depth of bitmap 1 is 2D bitmap, >1 is 3D bitmap */
	/* blender internals */
	uint16 t_width;			/* = (width/VN_B_TILE_SIZE + 1)*VN_B_TILE_SIZE */
	uint16 t_height;		/* = (height/VN_B_TILE_SIZE + 1)*VN_B_TILE_SIZE */
	void *image;			/* pointer at image */
	/* client dependent methods */
	void (*post_bitmap_dimension_set)(struct VNode *vnode);
	void (*post_bitmap_layer_create)(struct VBitmapLayer *vblayer);
	void (*post_bitmap_layer_destroy)(struct VBitmapLayer *vblayer);
	void (*post_bitmap_tile_set)(struct VBitmapLayer *vblayer, unsigned int xs, unsigned int ys);
}VBitmapData;

/* 
 * data of geometry node
 */
typedef struct VGeomData {
	struct DynamicList layers;	/* dynamic list with access array of Layers */
	struct VLink *vlink;		/* pointer at VerseLink connecting object node and geom node */
	struct ListBase queue;		/* queue of our layers waiting for receiving from verse server */
	void *mesh;			/* pointer at Mesh (object node) */
	void *editmesh;			/* pointer at EditMesh (edit mode) */
	struct HashVerseEdge *hash;	/* verse edge hash */
	struct ListBase edges;		/* list of fake verse edges */
	/* client dependent methods */
	void (*post_vertex_create)(struct VerseVert *vvert);
	void (*post_vertex_set_xyz)(struct VerseVert *vvert);
	void (*post_vertex_delete)(struct VerseVert *vvert);
	void (*post_vertex_free_constraint)(struct VerseVert *vvert);
	void (*post_polygon_create)(struct VerseFace *vface);
	void (*post_polygon_set_corner)(struct VerseFace *vface);
	void (*post_polygon_delete)(struct VerseFace *vface);
	void (*post_polygon_free_constraint)(struct VerseFace *vface);
	void (*post_geometry_free_constraint)(struct VNode *vnode);
	void (*post_polygon_set_uint8)(struct VerseFace *vface);
} VGeomData;

/*
 * data of object node
 */
typedef struct VObjectData {
	struct DynamicList links;	/* dynamic list with access array of links between other nodes */
	struct ListBase queue;		/* queue of links waiting for sending and receiving from verse server */
	float pos[3];			/* position of object VerseNode */
	float quat[4];			/* rotation of object VerseNode stored in quat */
	float scale[3];			/* scale of object VerseNode */
	void *object;			/* pointer at object */
	short flag;			/* flag: POS_RECEIVE_READY, ROT_RECEIVE_READY. SCALE_RECEIVE_READY */
	/* client dependent methods */
/*	void (*post_transform)(struct VNode *vnode);*/
	void (*post_transform_pos)(struct VNode *vnode);
	void (*post_transform_rot)(struct VNode *vnode);
	void (*post_transform_scale)(struct VNode *vnode);
	void (*post_object_free_constraint)(struct VNode *vnode);
} VObjectData;

/*
 * Verse Tag
 */
typedef struct VTag {
	struct VTag *next, *prev;
	/* verse data*/
	struct VTagGroup *vtaggroup;	/* pointer at Verse Tag Group */
	uint16 id;			/* id of this tag */
	char *name;			/* name of this tag*/
	VNTagType type;			/* type: VN_TAG_BOOLEAN, VN_TAG_UINT32, VN_TAG_REAL64, VN_TAG_REAL64_VEC3,
					   VN_TAG_LINK, VN_TAG_ANIMATION, VN_TAG_BLOB */
	VNTag *tag;			/* pointer at value (enum: vboolean, vuint32, vreal64, vstring,
					   vreal64_vec3, vlink, vanimation, vblob)*/
	/* blender internals */
	void *value;			/* pointer at blender value */
} VTag;

/*
 * Verse Tag Group (verse tags are grouped in tag groups)
 */
typedef struct VTagGroup {
	struct VTagGroup *next, *prev;
	/* verse data*/
	struct VNode *vnode;		/* pointer at Verse Node */
	uint16 id;			/* id of this tag group */
	char *name;			/* name of this tag group */
	/* blender internals */
	struct DynamicList tags;	/* dynamic list with access array containing tags */
	struct ListBase queue;		/* list of tags waiting for receiving from verse server */
	/* client dependent methods */
	void (*post_tag_change)(struct VTag *vatg);
	void (*post_taggroup_create)(struct VTagGroup *vtaggroup);
} VTagGroup;

 /*
 * Verse Method Group
 */
typedef struct VMethodGroup
{
	struct VMethodGroup *next, *prev;
	uint16 group_id;
	char name[16];
	struct ListBase methods;
} VMethodGroup;

/*
 * Verse Method
 */
typedef struct VMethod
{
	struct VMethod *next, *prev;
	uint16 id;
	char name[500];
	uint8 param_count;
	VNOParamType *param_type;
	char **param_name;
} VMethod;

/*
 * Verse Node
 */
typedef struct VNode {
	struct VNode *next, *prev;
	/* verse data*/
	struct VerseSession *session;	/* session pointer */
	VNodeID id;			/* node id */
	VNodeID owner_id;		/* owner's id of this node */
	char *name;			/* name of this node */
	uint32 type;			/* type of node (V_NT_OBJECT, V_NT_GEOMETRY, V_NT_BITMAP) */
	/* blender internals */
	char flag;			/* flags: NODE_SENT, NODE_RECEIVED, NODE_DELTED, NODE_OBSOLETE */
	struct DynamicList taggroups;	/* dynamic list with access array of taggroups */
	struct ListBase methodgroups;	/* method groups */
	struct ListBase queue;		/* list of taggroups waiting for receiving from verse server */
	void *data;			/* generic pointer at some data (VObjectData, VGeomData, ...) */
	int counter;			/* counter of verse link pointing at this vnode (vlink->target) */
	/* client dependent methods */
	void (*post_node_create)(struct VNode *vnode);	
	void (*post_node_destroy)(struct VNode *vnode);
	void (*post_node_name_set)(struct VNode *vnode);
#ifdef VERSECHAT
	/* verse chat */
	int chat_flag;			/* CHAT_LOGGED, CHAT_NOTLOGGED */
#endif
} VNode;


/*
 * Verse Session: verse client can be connected to several verse servers
 * it is neccessary to store some information about each session
 */
typedef struct VerseSession {
	struct VerseSession *next, *prev;
	/* verse data */
	VSession *vsession;		/* pointer at VSeesion (verse.h) */
	uint32 avatar;			/* id of avatar */
	char *address;			/* string containg IP/domain name of verse server and number of port */
	void *connection;		/* no clue */
	uint8 *host_id;			/* no clue */
	/* blender internals */
	short flag;			/* flag: VERSE_CONNECTING, VERSE_CONNECTED */
	DynamicList nodes;		/* list of verse nodes */
	ListBase queue;			/* list of nodes waiting for sending to verse server */
	unsigned int counter;		/* count of events, when connection wasn't accepted */
	/* client dependent methods */
	void (*post_connect_accept)(struct VerseSession *session);
	void (*post_connect_terminated)(struct VerseSession *session);
	void (*post_connect_update)(struct VerseSession *session);
} VerseSession;

typedef struct VerseServer {
	struct VerseServer *next, *prev;
	char *name;			/* human-readable server name */
	char *ip;			/* string containing IP/domain name of verse server and number of port */
	short flag;			/* flag: VERSE_CONNECTING, VERSE_CONNECTED */
	struct VerseSession *session;	/* pointer to related session */
} VerseServer;
/*
 * list of post callback functions
 */
typedef struct PostCallbackFunction {
	void (*function)(void *arg);
	void *param;
} PostCallbackFunction;

/* VerseSession->flag */
#define VERSE_CONNECTING	1
#define VERSE_CONNECTED		2
#define VERSE_AUTOSUBSCRIBE	4

/* max VerseSession->counter value */
#define MAX_UNCONNECTED_EVENTS	100

/* VNode flags */
#define NODE_SENT		1
#define NODE_RECEIVED		2
#define NODE_DELTED		4
#define NODE_OBSOLETE		8

#ifdef VERSECHAT
#define CHAT_NOTLOGGED		0
#define CHAT_LOGGED		1
#endif

/* VLayer flags */
#define LAYER_SENT		1
#define LAYER_RECEIVED		2
#define LAYER_DELETED		4
#define LAYER_OBSOLETE		8

/* VLink->flag */
#define LINK_SEND_READY		1

/* VObjectData->flag */
#define POS_RECEIVE_READY	1
#define ROT_RECEIVE_READY	2
#define SCALE_RECEIVE_READY	4
#define POS_SEND_READY		8
#define ROT_SEND_READY		16
#define SCALE_SEND_READY	32

/* VLayer->content */
#define VERTEX_LAYER		0
#define POLYGON_LAYER		1

/* VerseVert->flag */
#define VERT_DELETED		1	/* vertex delete command was received from verse server */
#define VERT_RECEIVED		2	/* VerseVert was received from verse server (is not in sending queue) */
#define VERT_LOCKED		4	/* VerseVert is ready to send local position to verse server */
#define VERT_POS_OBSOLETE	8	/* position of vertex was changed during sending to verse server */
#define VERT_OBSOLETE		16	/* vertex delete command was sent to verse server; it means, that
					 * no information related to this vertex shoudln't be sent to verse
					 * until verse vertex is completely deleted ... then this vertex id
					 * can be reused again for new vertex */

/* VerseFace->flag */
#define FACE_SEND_READY		1	/* VerseFace is ready for sending to verse server */
#define FACE_RECEIVED		2	/* VerseFace was received from verse server */
#define FACE_SENT		4	/* VerseFace was sent to verse server and we expect receiving from verse server */
#define FACE_DELETED		8	/* delete command was sent to verse server */
#define FACE_CHANGED		16	/* VerseFace was only changed not created */
#define FACE_OBSOLETE		32	/* VerseFace was changed during sending to verse server */

/* Queue type */
#define VERSE_NODE		1
#define VERSE_LINK		2
#define VERSE_LAYER		3
#define VERSE_VERT		4
#define VERSE_FACE		5

#define VERSE_TAG		6
#define VERSE_TAG_GROUP		7

#define VERSE_VERT_UINT32	8
#define VERSE_VERT_REAL32	9
#define VERSE_VERT_VEC_REAL32	10

#define VERSE_FACE_UINT8	11
#define VERSE_FACE_UINT32	12
#define VERSE_FACE_REAL32	13
#define VERSE_FACE_QUAT_UINT32	14
#define VERSE_FACE_QUAT_REAL32	15

/* Verse Bitmap Layer flags */
#define VBLAYER_SUBSCRIBED	1

/* function prototypes */

/* functions from verse_session.c */
void set_verse_session_callbacks(void);
struct VerseSession *versesession_from_vsession(VSession *vsession);
struct VerseSession *current_verse_session(void);
struct VerseSession *create_verse_session(const char *name, const char *pass, const char *address, uint8 *expected_key);
void free_verse_session(struct VerseSession *session);
void b_verse_update(void);
void b_verse_ms_get(void);
void b_verse_connect(char *address);
void end_verse_session(struct VerseSession *session);
void end_all_verse_sessions(void);

/* functions from verse_node.c */
void send_verse_tag(struct VTag *vtag);
void send_verse_taggroup(struct VTagGroup *vtaggroup);
void send_verse_node(struct VNode *vnode);
void free_verse_node_data(struct VNode *vnode);
void free_verse_node(struct VNode *vnode);
struct VNode* lookup_vnode(VerseSession *session, VNodeID node_id);
struct VNode* create_verse_node(VerseSession *session, VNodeID node_id, uint8 type, VNodeID owner_id);
void set_node_callbacks(void);

/* functions from verse_object_node.c */
struct VLink *find_unsent_parent_vlink(struct VerseSession *session, struct VNode *vnode);
struct VLink *find_unsent_child_vlink(struct VerseSession *session, struct VNode *vnode);
struct VLink *create_verse_link(VerseSession *session, struct VNode *source, struct VNode *target, uint16 link_id, uint32 target_id, const char *label);
void send_verse_object_position(struct VNode *vnode);
void send_verse_object_rotation(struct VNode *vnode);
void send_verse_object_scale(struct VNode *vnode);
void send_verse_link(struct VLink *vlink);

void free_object_data(struct VNode *vnode);
void set_object_callbacks(void);
struct VObjectData *create_object_data(void);
	

/* functions from verse_method.c */
void free_verse_methodgroup(VMethodGroup *vmg);
#ifdef VERSECHAT
void send_say(const char *chan, const char *utter);
void send_login(struct VNode *vnode);
void send_logout(struct VNode *vnode);
void send_join(struct VNode *vnode, const char *chan);
void send_leave(struct VNode *vnode, const char *chan);
#endif
void set_method_callbacks(void);

/* functions from verse_geometry_node.c */
struct VerseFace* create_verse_face(struct VLayer *vlayer, uint32 polygon_id, uint32 v0, uint32 v1, uint32 v2, uint32 v3);
struct VerseVert* create_verse_vertex(struct VLayer *vlayer, uint32 vertex_id, real32 x, real32 y, real32 z);
struct VLayer *create_verse_layer(struct VNode *vnode, VLayerID layer_id, const char *name, VNGLayerType type, uint32 def_integer, real64 def_real);
struct VGeomData *create_geometry_data(void);

void send_verse_layer(struct VLayer *vlayer);

void send_verse_face_corner_quat_real32(struct quat_real32_item *item, short type);
void send_verse_face_corner_quat_uint32(struct quat_uint32_item *item, short type);
void send_verse_face_real32(struct real32_item *item, short type);
void send_verse_face_uint32(struct uint32_item *item, short type);
void send_verse_face_uint8(struct uint8_item *item, short type);

void send_verse_vert_vec_real32(struct vec_real32_item *item, short type);
void send_verse_vert_real32(struct real32_item *item, short type);
void send_verse_vert_uint32(struct uint32_item *item, short type);

void send_verse_vertex_delete(struct VerseVert *vvert);
void send_verse_vertex(struct VerseVert *vvert);
void send_verse_face_delete(struct VerseFace *vface);

void destroy_geometry(struct VNode *vnode);

struct VLayer* find_verse_layer_type(struct VGeomData *geom, short content);
void add_item_to_send_queue(struct ListBase *lb, void *item, short type);
void free_geom_data(struct VNode *vnode);
void set_geometry_callbacks(void);

/* functions prototypes from verse_bitmap.c */
void set_bitmap_callbacks(void);
void free_bitmap_layer_data(struct VBitmapLayer *vblayer);
struct VBitmapLayer *create_bitmap_layer(struct VNode *vnode, VLayerID layer_id, const char *name, VNBLayerType type);
void free_bitmap_node_data(struct VNode *vnode);
struct VBitmapData *create_bitmap_data(void);

#endif
