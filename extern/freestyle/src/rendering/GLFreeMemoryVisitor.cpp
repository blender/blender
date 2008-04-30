#include "GLFreeMemoryVisitor.h"
#include "../scene_graph/IndexedFaceSet.h"

# ifdef WIN32
#  include <windows.h>
# endif
# ifdef __MACH__
#  include <OpenGL/gl.h>
# else
#  include <GL/gl.h>
# endif

GLFreeMemoryVisitor::GLFreeMemoryVisitor()
:SceneVisitor(){
}

GLFreeMemoryVisitor::~GLFreeMemoryVisitor(){
}

void GLFreeMemoryVisitor::visitIndexedFaceSet(IndexedFaceSet& ifs){
    GLuint dl = ifs.displayList();
    if(dl != 0){
        if(glIsList(dl)){
            glDeleteLists(dl, 1);
        }
    }
}
