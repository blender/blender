# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#			      W A R N I N G ! ! !                             #
#             a u t h o r i z e d    p e r s o n a l    o n l y               #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

SCENE_GRAPH_DIR = ../scene_graph

SOURCES *= $${SCENE_GRAPH_DIR}/IndexedFaceSet.cpp \
           $${SCENE_GRAPH_DIR}/LineRep.cpp \
           $${SCENE_GRAPH_DIR}/MaxFileLoader.cpp \
           $${SCENE_GRAPH_DIR}/NodeCamera.cpp \
           $${SCENE_GRAPH_DIR}/NodeDrawingStyle.cpp \
           $${SCENE_GRAPH_DIR}/NodeGroup.cpp \
           $${SCENE_GRAPH_DIR}/NodeLight.cpp \
           $${SCENE_GRAPH_DIR}/NodeShape.cpp \
           $${SCENE_GRAPH_DIR}/NodeTransform.cpp \
           $${SCENE_GRAPH_DIR}/OrientedLineRep.cpp \
           $${SCENE_GRAPH_DIR}/ScenePrettyPrinter.cpp \
           $${SCENE_GRAPH_DIR}/TriangleRep.cpp \
           $${SCENE_GRAPH_DIR}/VertexRep.cpp \
           $${SCENE_GRAPH_DIR}/Rep.cpp \
           $${SCENE_GRAPH_DIR}/SceneVisitor.cpp

HEADERS *= $${SCENE_GRAPH_DIR}/DrawingStyle.h \
           $${SCENE_GRAPH_DIR}/IndexedFaceSet.h \
           $${SCENE_GRAPH_DIR}/LineRep.h \
           $${SCENE_GRAPH_DIR}/Material.h \
           $${SCENE_GRAPH_DIR}/MaxFileLoader.h \
           $${SCENE_GRAPH_DIR}/Node.h \
           $${SCENE_GRAPH_DIR}/NodeCamera.h \
           $${SCENE_GRAPH_DIR}/NodeDrawingStyle.h \
           $${SCENE_GRAPH_DIR}/NodeGroup.h \
           $${SCENE_GRAPH_DIR}/NodeLight.h \
           $${SCENE_GRAPH_DIR}/NodeShape.h \
           $${SCENE_GRAPH_DIR}/NodeTransform.h \
           $${SCENE_GRAPH_DIR}/OrientedLineRep.h \
           $${SCENE_GRAPH_DIR}/Rep.h \
           $${SCENE_GRAPH_DIR}/ScenePrettyPrinter.h \
           $${SCENE_GRAPH_DIR}/SceneVisitor.h \
           $${SCENE_GRAPH_DIR}/TriangleRep.h \
           $${SCENE_GRAPH_DIR}/VertexRep.h
