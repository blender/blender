FROM gitpod/workspace-full
SHELL ["/bin/bash", "-c"]

RUN sudo apt-get -qq update

# https://wiki.blender.org/wiki/Building_Blender/Linux/Ubuntu
RUN sudo apt-get install build-essential git subversion cmake libx11-dev libxxf86vm-dev libxcursor-dev libxi-dev libxrandr-dev libxinerama-dev libglew-dev
