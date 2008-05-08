# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#			      W A R N I N G ! ! !                             #
#             a u t h o r i z e d    p e r s o n a l    o n l y               #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

STROKE_DIR = ../stroke

SOURCES *= $${STROKE_DIR}/AdvancedFunctions0D.cpp \
           $${STROKE_DIR}/AdvancedFunctions1D.cpp \
           $${STROKE_DIR}/AdvancedStrokeShaders.cpp \
           $${STROKE_DIR}/BasicStrokeShaders.cpp \
           $${STROKE_DIR}/Canvas.cpp \
	   $${STROKE_DIR}/Chain.cpp \
           $${STROKE_DIR}/ChainingIterators.cpp \
           $${STROKE_DIR}/ContextFunctions.cpp \
           $${STROKE_DIR}/Operators.cpp \
           $${STROKE_DIR}/PSStrokeRenderer.cpp \
	   $${STROKE_DIR}/Stroke.cpp \
	   $${STROKE_DIR}/StrokeIO.cpp \			       
	   $${STROKE_DIR}/StrokeLayer.cpp \		       
	   $${STROKE_DIR}/StrokeRenderer.cpp \		       
	   $${STROKE_DIR}/StrokeRep.cpp \
	   $${STROKE_DIR}/StrokeTesselator.cpp \
	   $${STROKE_DIR}/TextStrokeRenderer.cpp \
           $${STROKE_DIR}/Curve.cpp 

HEADERS *= $${STROKE_DIR}/AdvancedFunctions0D.h \
           $${STROKE_DIR}/AdvancedFunctions1D.h \
           $${STROKE_DIR}/AdvancedPredicates1D.h \
           $${STROKE_DIR}/AdvancedStrokeShaders.h \
           $${STROKE_DIR}/BasicStrokeShaders.h \
	   $${STROKE_DIR}/Canvas.h \			       
	   $${STROKE_DIR}/Chain.h \			       
	   $${STROKE_DIR}/ChainingIterators.h \
           $${STROKE_DIR}/ContextFunctions.h \
	   $${STROKE_DIR}/Curve.h \
           $${STROKE_DIR}/CurveIterators.h \
           $${STROKE_DIR}/CurveAdvancedIterators.h \
           $${STROKE_DIR}/Module.h \
           $${STROKE_DIR}/Operators.h \
	   $${STROKE_DIR}/Predicates1D.h \
           $${STROKE_DIR}/Predicates0D.h \
           $${STROKE_DIR}/PSStrokeRenderer.h \
	   $${STROKE_DIR}/Stroke.h \
	   $${STROKE_DIR}/StrokeIO.h \			       
           $${STROKE_DIR}/StrokeIterators.h \
           $${STROKE_DIR}/StrokeAdvancedIterators.h \
	   $${STROKE_DIR}/StrokeShader.h \			       
	   $${STROKE_DIR}/StrokeLayer.h \		       
	   $${STROKE_DIR}/StrokeRenderer.h \		       
	   $${STROKE_DIR}/StrokeRep.h \ 
	   $${STROKE_DIR}/StrokeTesselator.h \
	   $${STROKE_DIR}/StyleModule.h \
	   $${STROKE_DIR}/TextStrokeRenderer.h
