FROM gitpod/workspace-full
SHELL ["/bin/bash", "-c"]

RUN sudo apt-get -qq update

# https://wiki.blender.org/wiki/Building_Blender/Linux/Ubuntu
RUN sudo apt-get -qq install -y build-essential git subversion cmake libx11-dev libxxf86vm-dev libxcursor-dev libxi-dev libxrandr-dev libxinerama-dev libglew-dev
RUN mkdir lib && cd lib && svn checkout https://svn.blender.org/svnroot/bf-blender/trunk/lib/linux_centos7_x86_64