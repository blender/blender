"""
Run a Function every x Seconds
------------------------------
"""
import bpy

def every_2_seconds():
    print("Hello World")
    return 2

bpy.app.timers.register(every_2_seconds)
