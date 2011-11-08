
set(licenses
	Apache_2.0.txt
	Blender.txt
	GPL.txt
	ILM.txt
	NVidia.txt
	OSL.txt
	Sobol.txt
	readme.txt)

delayed_install(${CMAKE_CURRENT_SOURCE_DIR}  "${licenses}" ${CYCLES_INSTALL_PATH}/license)

