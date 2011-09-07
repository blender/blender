# Testing the BGL module

import Blender
from Blender.BGL import *
from Blender import Draw

R = G = B = 0
A = 1

instructions = "Hold mouse buttons to change the background color."
quitting = " Press ESC or q to quit."

def show_win():
  glClearColor(R,G,B,A)                # define color used to clear buffers 
  glClear(GL_COLOR_BUFFER_BIT)         # use it to clear the color buffer
  glColor3f(1,1,1)                     # change default color
  glRasterPos2i(50,100)                # move cursor to x = 50, y = 100
  Draw.Text("Testing BGL  + Draw")     # draw this text there
  glRasterPos2i(350,20)                # move cursor again
  Draw.Text(instructions + quitting)   # draw another msg
  glBegin(GL_LINE_LOOP)                # begin a vertex-data list
  glVertex2i(46,92)
  glVertex2i(120,92)
  glVertex2i(120,115)
  glVertex2i(46,115)
  glEnd()                              # close this list
  glColor3f(0.35,0.18,0.92)            # change default color again
  glBegin(GL_POLYGON)                  # another list, for a polygon
  glVertex2i(315, 292)
  glVertex2i(412, 200)
  glVertex2i(264, 256)
  glEnd()
  Draw.Redraw(1)                       # make changes visible.

def ev(evt, val):                      # this is a callback for Draw.Register()
  global R,G,B,A                       # it handles input events
  if evt == Draw.ESCKEY or evt == Draw.QKEY:
    Draw.Exit()                        # this quits the script
  elif evt == Draw.LEFTMOUSE: R = 1 - R
  elif evt == Draw.MIDDLEMOUSE: G = 1 - G
  elif evt == Draw.RIGHTMOUSE: B = 1 - B
  else:
    Draw.Register(show_win, ev, None)

Draw.Register(show_win, ev, None)      # start the main loop
