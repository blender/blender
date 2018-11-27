"""
Use a Timer to react to events in another thread
------------------------------------------------

You should never modify Blender data at arbitrary points in time in separate threads.
However you can use a queue to collect all the actions that should be executed when Blender is in the right state again.
Pythons `queue.Queue` can be used here, because it implements the required locking semantics.
"""
import bpy
import queue

execution_queue = queue.Queue()

# This function can savely be called in another thread.
# The function will be executed when the timer runs the next time.
def run_in_main_thread(function):
    execution_queue.put(function)

def execute_queued_functions():
    while not execution_queue.empty():
        function = execution_queue.get()
        function()
    return 1

bpy.app.timers.register(execute_queued_functions)
