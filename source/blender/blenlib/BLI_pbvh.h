struct MFace;
struct MVert;
struct PBVH;

/* Returns 1 if the search should continue from this node, 0 otherwise */
typedef int (*BLI_pbvh_SearchCallback)(float bb_min[3], float bb_max[3],
				       void *data);

typedef void (*BLI_pbvh_HitCallback)(const int *face_indices,
				     const int *vert_indices,
				     int totface, int totvert, void *data);
int BLI_pbvh_search_range(float bb_min[3], float bb_max[3], void *data_v);

typedef enum {
	PBVH_SEARCH_NORMAL,

	/* When the callback returns a 1 for a leaf node, that node will be
	   marked as modified */
	PBVH_SEARCH_MARK_MODIFIED,
	
	/* Update gpu data for modified nodes. Also clears the Modified flag. */
	PBVH_SEARCH_MODIFIED,

	
	PBVH_SEARCH_UPDATE
} PBVH_SearchMode;

/* Pass the node as data to the callback */
#define PBVH_NodeData (void*)0xa
/* Pass the draw buffers as data to the callback */
#define PBVH_DrawData (void*)0xb

void BLI_pbvh_search(struct PBVH *bvh, BLI_pbvh_SearchCallback scb,
		     void *search_data, BLI_pbvh_HitCallback hcb,
		     void *hit_data, PBVH_SearchMode mode);

/* The hit callback is called for all leaf nodes intersecting the ray;
   it's up to the callback to find the primitive within the leaves that is
   hit first */
void BLI_pbvh_raycast(struct PBVH *bvh, BLI_pbvh_HitCallback cb, void *data,
		      float ray_start[3], float ray_normal[3]);


int BLI_pbvh_update_search_cb(float bb_min[3], float bb_max[3], void *data_v);

/* Get the bounding box around all nodes that have been marked as modified. */
void BLI_pbvh_modified_bounding_box(struct PBVH *bvh,
				    float bb_min[3], float bb_max[3]);
void BLI_pbvh_reset_modified_bounding_box(struct PBVH *bvh);

/* Lock is off by default, turn on to stop redraw from clearing the modified
   flag from nodes */
void BLI_pbvh_toggle_modified_lock(struct PBVH *bvh);



struct PBVH *BLI_pbvh_new(BLI_pbvh_HitCallback update_cb, void *update_cb_data);
void BLI_pbvh_build(struct PBVH *bvh, struct MFace *faces, struct MVert *verts,
		    int totface, int totvert);
void BLI_pbvh_free(struct PBVH *bvh);

void BLI_pbvh_set_source(struct PBVH *bvh, struct MVert *, struct MFace *mface);
