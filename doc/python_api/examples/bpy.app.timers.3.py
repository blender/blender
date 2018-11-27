"""
Run a Function n times every x seconds
--------------------------------------
"""
import bpy

counter = 0

def run_10_times():
    global counter
    counter += 1
    print(counter)
    if counter == 10:
        return None
    return 0.1

bpy.app.timers.register(run_10_times)
