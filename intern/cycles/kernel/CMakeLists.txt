
set(sources
	kernel.cpp
	kernel.cl
	kernel.cu)

set(headers
	kernel.h
	kernel_bvh.h
	kernel_camera.h
	kernel_compat_cpu.h
	kernel_compat_cuda.h
	kernel_compat_opencl.h
	kernel_differential.h
	kernel_displace.h
	kernel_emission.h
	kernel_film.h
	kernel_globals.h
	kernel_light.h
	kernel_math.h
	kernel_mbvh.h
	kernel_montecarlo.h
	kernel_object.h
	kernel_path.h
	kernel_qbvh.h
	kernel_random.h
	kernel_shader.h
	kernel_textures.h
	kernel_triangle.h
	kernel_types.h)

set(svm_headers
	svm/bsdf.h
	svm/bsdf_ashikhmin_velvet.h
	svm/bsdf_diffuse.h
	svm/bsdf_microfacet.h
	svm/bsdf_reflection.h
	svm/bsdf_refraction.h
	svm/bsdf_transparent.h
	svm/bsdf_ward.h
	svm/bsdf_westin.h
	svm/emissive.h
	svm/svm.h
	svm/svm_attribute.h
	svm/svm_blend.h
	svm/svm_bsdf.h
	svm/svm_closure.h
	svm/svm_clouds.h
	svm/svm_convert.h
	svm/svm_displace.h
	svm/svm_distorted_noise.h
	svm/svm_fresnel.h
	svm/svm_geometry.h
	svm/svm_image.h
	svm/svm_light_path.h
	svm/svm_magic.h
	svm/svm_mapping.h
	svm/svm_marble.h
	svm/svm_math.h
	svm/svm_mix.h
	svm/svm_musgrave.h
	svm/svm_noise.h
	svm/svm_noisetex.h
	svm/svm_sky.h
	svm/svm_stucci.h
	svm/svm_tex_coord.h
	svm/svm_texture.h
	svm/svm_types.h
	svm/svm_value.h
	svm/svm_voronoi.h
	svm/svm_wood.h
	svm/volume.h
	)

set(util_headers
	../util/util_color.h
	../util/util_math.h
	../util/util_transform.h
	../util/util_types.h)

# CUDA module

if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
	set(CUDA_BITS 64)
else()
	set(CUDA_BITS 32)
endif()

if(WITH_CYCLES_CUDA)
	set(cuda_sources kernel.cu ${headers} ${svm_headers})
	set(cuda_cubins)

	foreach(arch ${CYCLES_CUDA_ARCH})
		set(cuda_cubin kernel_${arch}.cubin)

		add_custom_command(
			OUTPUT ${cuda_cubin}
			COMMAND ${CUDA_NVCC_EXECUTABLE} -arch=${arch} -m${CUDA_BITS} --cubin ${CMAKE_CURRENT_SOURCE_DIR}/kernel.cu --use_fast_math -o ${CMAKE_CURRENT_BINARY_DIR}/${cuda_cubin} --ptxas-options="-v" --maxrregcount=${CYCLES_CUDA_MAXREG} --opencc-options -OPT:Olimit=0 -I${CMAKE_CURRENT_SOURCE_DIR}/../util -I${CMAKE_CURRENT_SOURCE_DIR}/svm -DCCL_NAMESPACE_BEGIN= -DCCL_NAMESPACE_END= -DNVCC
			DEPENDS ${cuda_sources})

		delayed_install("${CMAKE_CURRENT_BINARY_DIR}" "${cuda_cubin}" ${CYCLES_INSTALL_PATH}/lib)
		list(APPEND cuda_cubins ${cuda_cubin})
	endforeach()

	add_custom_target(cycles_kernel_cuda ALL DEPENDS ${cuda_cubins})
endif()

# OSL module

if(WITH_CYCLES_OSL)
	add_subdirectory(osl)
endif()

# CPU module

include_directories(. ../util osl svm)

add_library(cycles_kernel ${sources} ${headers} ${svm_headers})

if(WITH_CYCLES_CUDA)
	add_dependencies(cycles_kernel cycles_kernel_cuda)
endif()

# OPENCL kernel

#set(kernel_preprocessed ${CMAKE_CURRENT_BINARY_DIR}/kernel_preprocessed.cl)
#add_custom_command(
#	OUTPUT ${kernel_preprocessed}
#	COMMAND gcc -x c++ -E ${CMAKE_CURRENT_SOURCE_DIR}/kernel.cl -I ${CMAKE_CURRENT_SOURCE_DIR}/../util/ -DCCL_NAMESPACE_BEGIN= -DCCL_NAMESPACE_END= -DWITH_OPENCL -o ${kernel_preprocessed}
#	DEPENDS ${kernel_sources} ${util_headers})
#add_custom_target(cycles_kernel_preprocess ALL DEPENDS ${kernel_preprocessed})
#delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${kernel_preprocessed}" ${CYCLES_INSTALL_PATH}/kernel)

delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "kernel.cl" ${CYCLES_INSTALL_PATH}/kernel)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "kernel.cu" ${CYCLES_INSTALL_PATH}/kernel)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${headers}" ${CYCLES_INSTALL_PATH}/kernel)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${svm_headers}" ${CYCLES_INSTALL_PATH}/kernel/svm)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${util_headers}" ${CYCLES_INSTALL_PATH}/kernel)

