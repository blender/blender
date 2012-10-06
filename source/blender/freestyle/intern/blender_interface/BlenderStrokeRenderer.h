#ifndef  BLENDERSTROKERENDERER_H
# define BLENDERSTROKERENDERER_H

# include "../stroke/StrokeRenderer.h"
# include "../system/FreestyleConfig.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "DNA_material_types.h"
#include "DNA_scene_types.h"

#include "render_types.h"

#ifdef __cplusplus
}
#endif



class LIB_STROKE_EXPORT BlenderStrokeRenderer : public StrokeRenderer
{
public:
  BlenderStrokeRenderer(Render *re, int render_count);
  virtual ~BlenderStrokeRenderer();

  /*! Renders a stroke rep */
  virtual void RenderStrokeRep(StrokeRep *iStrokeRep) const;
  virtual void RenderStrokeRepBasic(StrokeRep *iStrokeRep) const;

  Object* NewMesh() const;

	Render* RenderScene(Render *re);

protected:
	Scene* old_scene;
	Scene* freestyle_scene;
	Material* material;
	float _width, _height;
	float _z, _z_delta;

	float get_stroke_vertex_z(void) const;
};

#endif // BLENDERSTROKERENDERER_H

