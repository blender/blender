from bgl import *

def createDisplayList(function, *args, **kwargs):
    displayList = glGenLists(1)
    glNewList(displayList, GL_COMPILE)
    function(*args, **kwargs)
    glEndList()
    return displayList

def freeDisplayList(displayList):
    glDeleteLists(displayList, 1)

def drawDisplayList(displayList):
    glCallList(displayList)
    glFlush()
