
set(INC
	.
	../util
	osl
	svm
)

set(SRC
	kernel.cpp
	kernel_optimized.cpp
	kernel.cl
	kernel.cu
)

set(SRC_HEADERS
	kernel.h
	kernel_accumulate.h
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
	kernel_passes.h
	kernel_path.h
	kernel_qbvh.h
	kernel_random.h
	kernel_shader.h
	kernel_textures.h
	kernel_triangle.h
	kernel_types.h
)

set(SRC_SVM_HEADERS
	svm/bsdf.h
	svm/bsdf_ashikhmin_velvet.h
	svm/bsdf_diffuse.h
	svm/bsdf_oren_nayar.h
	svm/bsdf_microfacet.h
	svm/bsdf_reflection.h
	svm/bsdf_refraction.h
	svm/bsdf_transparent.h
	svm/bsdf_ward.h
	svm/bsdf_westin.h
	svm/emissive.h
	svm/svm.h
	svm/svm_attribute.h
	svm/svm_bsdf.h
	svm/svm_camera.h
	svm/svm_closure.h
	svm/svm_convert.h
	svm/svm_checker.h
	svm/svm_displace.h
	svm/svm_fresnel.h
	svm/svm_gamma.h
	svm/svm_brightness.h
	svm/svm_geometry.h
	svm/svm_gradient.h
	svm/svm_hsv.h
	svm/svm_image.h
	svm/svm_invert.h
	svm/svm_light_path.h
	svm/svm_magic.h
	svm/svm_mapping.h
	svm/svm_math.h
	svm/svm_mix.h
	svm/svm_musgrave.h
	svm/svm_noise.h
	svm/svm_noisetex.h
	svm/svm_normal.h
	svm/svm_sepcomb_rgb.h
	svm/svm_sky.h
	svm/svm_tex_coord.h
	svm/svm_texture.h
	svm/svm_types.h
	svm/svm_value.h
	svm/svm_voronoi.h
	svm/svm_wave.h
	svm/volume.h
)

set(SRC_UTIL_HEADERS
	../util/util_color.h
	../util/util_math.h
	../util/util_transform.h
	../util/util_types.h
)
# CUDA module

if(WITH_CYCLES_CUDA_BINARIES)
	if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
		set(CUDA_BITS 64)
	else()
		set(CUDA_BITS 32)
	endif()

	set(cuda_sources kernel.cu ${headers} ${svm_headers})
	set(cuda_cubins)

	foreach(arch ${CYCLES_CUDA_BINARIES_ARCH})
		set(cuda_cubin kernel_${arch}.cubin)

		add_custom_command(
			OUTPUT ${cuda_cubin}
			COMMAND ${CUDA_NVCC_EXECUTABLE} -arch=${arch} -m${CUDA_BITS} --cubin ${CMAKE_CURRENT_SOURCE_DIR}/kernel.cu --use_fast_math -o ${CMAKE_CURRENT_BINARY_DIR}/${cuda_cubin} --ptxas-options="-v" --maxrregcount=24 --opencc-options -OPT:Olimit=0 -I${CMAKE_CURRENT_SOURCE_DIR}/../util -I${CMAKE_CURRENT_SOURCE_DIR}/svm -DCCL_NAMESPACE_BEGIN= -DCCL_NAMESPACE_END= -DNVCC
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

include_directories(${INC})

add_library(cycles_kernel ${SRC} ${SRC_HEADERS} ${SRC_SVM_HEADERS})

if(WITH_CYCLES_OPTIMIZED_KERNEL)
	set_source_files_properties(kernel_optimized.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_OPTIMIZED_KERNEL_FLAGS}")
endif()

if(WITH_CYCLES_CUDA)
	add_dependencies(cycles_kernel cycles_kernel_cuda)
endif()

# OpenCL kernel

#set(KERNEL_PREPROCESSED ${CMAKE_CURRENT_BINARY_DIR}/kernel_preprocessed.cl)
#add_custom_command(
#	OUTPUT ${KERNEL_PREPROCESSED}
#	COMMAND gcc -x c++ -E ${CMAKE_CURRENT_SOURCE_DIR}/kernel.cl -I ${CMAKE_CURRENT_SOURCE_DIR}/../util/ -DCCL_NAMESPACE_BEGIN= -DCCL_NAMESPACE_END= -o ${KERNEL_PREPROCESSED}
#	DEPENDS ${SRC_KERNEL} ${SRC_UTIL_HEADERS})
#add_custom_target(cycles_kernel_preprocess ALL DEPENDS ${KERNEL_PREPROCESSED})
#delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${KERNEL_PREPROCESSED}" ${CYCLES_INSTALL_PATH}/kernel)

delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "kernel.cl" ${CYCLES_INSTALL_PATH}/kernel)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "kernel.cu" ${CYCLES_INSTALL_PATH}/kernel)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_HEADERS}" ${CYCLES_INSTALL_PATH}/kernel)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_SVM_HEADERS}" ${CYCLES_INSTALL_PATH}/kernel/svm)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_UTIL_HEADERS}" ${CYCLES_INSTALL_PATH}/kernel)

