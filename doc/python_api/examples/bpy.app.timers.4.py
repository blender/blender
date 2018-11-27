"""
Assign parameters to functions
------------------------------
"""
import bpy
import functools

def print_message(message):
    print("Message:", message)

bpy.app.timers.register(functools.partial(print_message, "Hello"), first_interval=2.0)
bpy.app.timers.register(functools.partial(print_message, "World"), first_interval=3.0)
