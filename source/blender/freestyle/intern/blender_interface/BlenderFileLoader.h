#ifndef  BLENDER_FILE_LOADER_H
# define BLENDER_FILE_LOADER_H

# include <string.h>              
# include <float.h>

# include "../system/FreestyleConfig.h"
# include "../scene_graph/NodeGroup.h"
# include "../scene_graph/NodeTransform.h"
# include "../scene_graph/NodeShape.h"
# include "../scene_graph/IndexedFaceSet.h"
# include "../geometry/BBox.h"
# include "../geometry/Geom.h"
# include "../geometry/GeomCleaner.h"

#ifdef __cplusplus
extern "C" {
#endif

	#include "DNA_material_types.h"
	#include "DNA_scene_types.h"
	#include "render_types.h"
	#include "renderdatabase.h"
	
	#include "BKE_mesh.h"
	#include "BKE_scene.h"
	#include "BLI_math.h"
	
#ifdef __cplusplus
}
#endif


class NodeGroup;

struct LoaderState {
	float *pv;
	float *pn;
	unsigned *pvi;
	unsigned *pni;
	unsigned *pmi;
	unsigned currentIndex;
	unsigned currentMIndex;
	float minBBox[3];
	float maxBBox[3];
};

class LIB_SCENE_GRAPH_EXPORT BlenderFileLoader
{
public:
  /*! Builds a MaxFileLoader */
	BlenderFileLoader(Render *re, SceneRenderLayer* srl);
  virtual ~BlenderFileLoader();

  /*! Loads the 3D scene and returns a pointer to the scene root node */
  NodeGroup * Load();

  /*! Gets the number of read faces */
  inline unsigned int numFacesRead() {return _numFacesRead;}

  /*! Gets the smallest edge size read */
  inline real minEdgeSize() {return _minEdgeSize;}

protected:
	void insertShapeNode(ObjectInstanceRen *obi, int id);
	int countClippedFaces(float v1[3], float v2[3], float v3[3], int clip[3]);
	void clipLine(float v1[3], float v2[3], float c[3], float z);
	void clipTriangle(int numTris, float triCoords[][3], float v1[3], float v2[3], float v3[3], int clip[3]);
	void addTriangle(struct LoaderState *ls, float v1[3], float v2[3], float v3[3]);

protected:
	Render* _re;
	SceneRenderLayer* _srl;
	NodeGroup* _Scene;
	unsigned _numFacesRead;
	real _minEdgeSize;
	float _viewplane_left;
	float _viewplane_right;
	float _viewplane_bottom;
	float _viewplane_top;
	float _z_near, _z_far;
};

#endif // BLENDER_FILE_LOADER_H
