#ifndef GL_FREE_MEMORY_VISITOR_H_
#define GL_FREE_MEMORY_VISITOR_H_

# include "../system/FreestyleConfig.h"
# include "../scene_graph/SceneVisitor.h"

/*! Mainly used to delete display lists */
class LIB_RENDERING_EXPORT GLFreeMemoryVisitor : public SceneVisitor
{
public:

    GLFreeMemoryVisitor() ;
    virtual ~GLFreeMemoryVisitor() ;

    //
    // visitClass methods
    //
    //////////////////////////////////////////////

        VISIT_DECL(IndexedFaceSet)
};

#endif // GL_FREE_MEMORY_H_