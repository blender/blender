@echo off
echo Starting Blender with the Vulkan backend

"%~dp0\blender" --gpu-backend vulkan
