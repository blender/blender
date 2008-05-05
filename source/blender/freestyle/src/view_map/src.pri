# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#			      W A R N I N G ! ! !                             #
#             a u t h o r i z e d    p e r s o n a l    o n l y               #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

VIEW_MAP_DIR = ../view_map

SOURCES *= $${VIEW_MAP_DIR}/Functions0D.cpp \
           $${VIEW_MAP_DIR}/Functions1D.cpp \
	   $${VIEW_MAP_DIR}/Silhouette.cpp \
	   $${VIEW_MAP_DIR}/SilhouetteGeomEngine.cpp \
	   $${VIEW_MAP_DIR}/ViewMap.cpp \
	   $${VIEW_MAP_DIR}/ViewMapBuilder.cpp \
	   $${VIEW_MAP_DIR}/ViewMapIO.cpp \
	   $${VIEW_MAP_DIR}/ViewMapTesselator.cpp \
	   $${VIEW_MAP_DIR}/FEdgeXDetector.cpp \
       $${VIEW_MAP_DIR}/ViewEdgeXBuilder.cpp \
       $${VIEW_MAP_DIR}/SteerableViewMap.cpp 

HEADERS *= $${VIEW_MAP_DIR}/Functions0D.h \
           $${VIEW_MAP_DIR}/Functions1D.h \
           $${VIEW_MAP_DIR}/Interface0D.h \
           $${VIEW_MAP_DIR}/Interface1D.h \
	   $${VIEW_MAP_DIR}/Silhouette.h \
	   $${VIEW_MAP_DIR}/SilhouetteGeomEngine.h \
	   $${VIEW_MAP_DIR}/ViewMap.h \
           $${VIEW_MAP_DIR}/ViewMapAdvancedIterators.h \
	   $${VIEW_MAP_DIR}/ViewMapBuilder.h \
	   $${VIEW_MAP_DIR}/ViewMapIO.h \
           $${VIEW_MAP_DIR}/ViewMapIterators.h \
	   $${VIEW_MAP_DIR}/ViewMapTesselator.h \
	   $${VIEW_MAP_DIR}/FEdgeXDetector.h \
       $${VIEW_MAP_DIR}/ViewEdgeXBuilder.h \
       $${VIEW_MAP_DIR}/SteerableViewMap.h 
