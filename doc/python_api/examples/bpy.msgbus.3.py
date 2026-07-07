"""
It is also possible to create subscriptions on a property of all instances of a
certain type:
"""
import bpy

subscribe_to = (bpy.types.Object, "location")
