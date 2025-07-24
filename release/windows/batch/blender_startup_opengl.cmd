@echo off
echo Starting Blender with the OpenGL backend

"%~dp0\blender" --gpu-backend opengl
