# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#			      W A R N I N G ! ! !                             #
#             a u t h o r i z e d    p e r s o n a l    o n l y               #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

GEOMETRY_DIR = ../geometry

SOURCES *= $${GEOMETRY_DIR}/GeomCleaner.cpp \
           $${GEOMETRY_DIR}/GeomUtils.cpp \
           $${GEOMETRY_DIR}/Grid.cpp \
           $${GEOMETRY_DIR}/FastGrid.cpp \
           $${GEOMETRY_DIR}/HashGrid.cpp \
           $${GEOMETRY_DIR}/FitCurve.cpp \
           $${GEOMETRY_DIR}/Bezier.cpp \
           $${GEOMETRY_DIR}/Noise.cpp \
           $${GEOMETRY_DIR}/matrix_util.cpp \
           $${GEOMETRY_DIR}/normal_cycle.cpp

HEADERS *= $${GEOMETRY_DIR}/BBox.h \
           $${GEOMETRY_DIR}/FastGrid.h \
           $${GEOMETRY_DIR}/Geom.h \
           $${GEOMETRY_DIR}/GeomCleaner.h \
           $${GEOMETRY_DIR}/GeomUtils.h \
           $${GEOMETRY_DIR}/Grid.h \
           $${GEOMETRY_DIR}/HashGrid.h \
           $${GEOMETRY_DIR}/Polygon.h \
           $${GEOMETRY_DIR}/SweepLine.h \
           $${GEOMETRY_DIR}/FitCurve.h \
           $${GEOMETRY_DIR}/Bezier.h \
           $${GEOMETRY_DIR}/Noise.h \
           $${GEOMETRY_DIR}/VecMat.h \
           $${GEOMETRY_DIR}/matrix_util.h \
           $${GEOMETRY_DIR}/normal_cycle.h
