"""
Run a Function in x Seconds
---------------------------
"""
import bpy

def in_5_seconds():
    print("Hello World")

bpy.app.timers.register(in_5_seconds, first_interval=5)
