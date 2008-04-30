# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
#			      W A R N I N G ! ! !                             #
#             a u t h o r i z e d    p e r s o n a l    o n l y               #
# # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #

APP_DIR = ../app

SOURCES *= $${APP_DIR}/AppAboutWindow.cpp \
           $${APP_DIR}/AppCanvas.cpp \
           $${APP_DIR}/AppConfig.cpp \
           $${APP_DIR}/AppGLWidget.cpp \
           $${APP_DIR}/AppInteractiveShaderWindow.cpp \
           $${APP_DIR}/AppMainWindow.cpp \
           $${APP_DIR}/AppOptionsWindow.cpp \
           $${APP_DIR}/AppProgressBar.cpp \
           $${APP_DIR}/AppStyleWindow.cpp \
           $${APP_DIR}/Controller.cpp \
           $${APP_DIR}/QGLBasicWidget.cpp \
           $${APP_DIR}/QStyleModuleSyntaxHighlighter.cpp \
           $${APP_DIR}/AppGL2DCurvesViewer.cpp \
           $${APP_DIR}/AppDensityCurvesWindow.cpp \
           $${APP_DIR}/ConfigIO.cpp \
           $${APP_DIR}/Main.cpp

HEADERS *= $${APP_DIR}/AppAboutWindow.h \
           $${APP_DIR}/AppCanvas.h \
           $${APP_DIR}/AppConfig.h \
           $${APP_DIR}/AppGLWidget.h \
           $${APP_DIR}/AppInteractiveShaderWindow.h \
           $${APP_DIR}/AppMainWindow.h \
           $${APP_DIR}/AppOptionsWindow.h \
           $${APP_DIR}/AppProgressBar.h \
           $${APP_DIR}/AppStyleWindow.h \
           $${APP_DIR}/QGLBasicWidget.h \
           $${APP_DIR}/QStyleModuleSyntaxHighlighter.h \
           $${APP_DIR}/AppGL2DCurvesViewer.h \
           $${APP_DIR}/AppDensityCurvesWindow.h \
	   $${APP_DIR}/ConfigIO.h \
           $${APP_DIR}/Controller.h

FORMS *= $${APP_DIR}/appmainwindowbase4.ui \
         $${APP_DIR}/interactiveshaderwindow4.ui \
         $${APP_DIR}/optionswindow4.ui \
         $${APP_DIR}/progressdialog4.ui \
         $${APP_DIR}/stylewindow4.ui \
         $${APP_DIR}/densitycurveswindow4.ui

RESOURCES = $${APP_DIR}/freestyle.qrc


