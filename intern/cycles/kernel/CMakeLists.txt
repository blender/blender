remove_extra_strict_flags()

set(INC
	.
	../util
	osl
	svm
)

set(INC_SYS

)

set(SRC
	kernels/cpu/kernel.cpp
	kernels/cpu/kernel_split.cpp
	kernels/opencl/kernel.cl
	kernels/opencl/kernel_state_buffer_size.cl
	kernels/opencl/kernel_data_init.cl
	kernels/opencl/kernel_path_init.cl
	kernels/opencl/kernel_queue_enqueue.cl
	kernels/opencl/kernel_scene_intersect.cl
	kernels/opencl/kernel_lamp_emission.cl
	kernels/opencl/kernel_do_volume.cl
	kernels/opencl/kernel_indirect_background.cl
	kernels/opencl/kernel_shader_eval.cl
	kernels/opencl/kernel_holdout_emission_blurring_pathtermination_ao.cl
	kernels/opencl/kernel_subsurface_scatter.cl
	kernels/opencl/kernel_direct_lighting.cl
	kernels/opencl/kernel_shadow_blocked.cl
	kernels/opencl/kernel_next_iteration_setup.cl
	kernels/opencl/kernel_indirect_subsurface.cl
	kernels/opencl/kernel_buffer_update.cl
	kernels/cuda/kernel.cu
	kernels/cuda/kernel_split.cu
)

set(SRC_BVH_HEADERS
	bvh/bvh.h
	bvh/bvh_nodes.h
	bvh/bvh_shadow_all.h
	bvh/bvh_subsurface.h
	bvh/bvh_traversal.h
	bvh/bvh_types.h
	bvh/bvh_volume.h
	bvh/bvh_volume_all.h
	bvh/qbvh_nodes.h
	bvh/qbvh_shadow_all.h
	bvh/qbvh_subsurface.h
	bvh/qbvh_traversal.h
	bvh/qbvh_volume.h
	bvh/qbvh_volume_all.h
)

set(SRC_HEADERS
	kernel_accumulate.h
	kernel_bake.h
	kernel_camera.h
	kernel_compat_cpu.h
	kernel_compat_cuda.h
	kernel_compat_opencl.h
	kernel_debug.h
	kernel_differential.h
	kernel_emission.h
	kernel_film.h
	kernel_globals.h
	kernel_image_opencl.h
	kernel_jitter.h
	kernel_light.h
	kernel_math.h
	kernel_montecarlo.h
	kernel_passes.h
	kernel_path.h
	kernel_path_branched.h
	kernel_path_common.h
	kernel_path_state.h
	kernel_path_surface.h
	kernel_path_subsurface.h
	kernel_path_volume.h
	kernel_projection.h
	kernel_queues.h
	kernel_random.h
	kernel_shader.h
	kernel_shadow.h
	kernel_subsurface.h
	kernel_textures.h
	kernel_types.h
	kernel_volume.h
	kernel_work_stealing.h
)

set(SRC_KERNELS_CPU_HEADERS
	kernel.h
	kernels/cpu/kernel_cpu.h
	kernels/cpu/kernel_cpu_impl.h
	kernels/cpu/kernel_cpu_image.h
)

set(SRC_KERNELS_CUDA_HEADERS
	kernels/cuda/kernel_config.h
)

set(SRC_CLOSURE_HEADERS
	closure/alloc.h
	closure/bsdf.h
	closure/bsdf_ashikhmin_velvet.h
	closure/bsdf_diffuse.h
	closure/bsdf_diffuse_ramp.h
	closure/bsdf_microfacet.h
	closure/bsdf_microfacet_multi.h
	closure/bsdf_microfacet_multi_impl.h
	closure/bsdf_oren_nayar.h
	closure/bsdf_phong_ramp.h
	closure/bsdf_reflection.h
	closure/bsdf_refraction.h
	closure/bsdf_toon.h
	closure/bsdf_transparent.h
	closure/bsdf_util.h
	closure/bsdf_ashikhmin_shirley.h
	closure/bsdf_hair.h
	closure/bssrdf.h
	closure/emissive.h
	closure/volume.h
)

set(SRC_SVM_HEADERS
	svm/svm.h
	svm/svm_attribute.h
	svm/svm_blackbody.h
	svm/svm_bump.h
	svm/svm_camera.h
	svm/svm_closure.h
	svm/svm_convert.h
	svm/svm_checker.h
	svm/svm_color_util.h
	svm/svm_brick.h
	svm/svm_displace.h
	svm/svm_fresnel.h
	svm/svm_wireframe.h
	svm/svm_wavelength.h
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
	svm/svm_math_util.h
	svm/svm_mix.h
	svm/svm_musgrave.h
	svm/svm_noise.h
	svm/svm_noisetex.h
	svm/svm_normal.h
	svm/svm_ramp.h
	svm/svm_ramp_util.h
	svm/svm_sepcomb_hsv.h
	svm/svm_sepcomb_vector.h
	svm/svm_sky.h
	svm/svm_tex_coord.h
	svm/svm_texture.h
	svm/svm_types.h
	svm/svm_value.h
	svm/svm_vector_transform.h
	svm/svm_voronoi.h
	svm/svm_voxel.h
	svm/svm_wave.h
)

set(SRC_GEOM_HEADERS
	geom/geom.h
	geom/geom_attribute.h
	geom/geom_curve.h
	geom/geom_motion_curve.h
	geom/geom_motion_triangle.h
	geom/geom_motion_triangle_intersect.h
	geom/geom_motion_triangle_shader.h
	geom/geom_object.h
	geom/geom_patch.h
	geom/geom_primitive.h
	geom/geom_subd_triangle.h
	geom/geom_triangle.h
	geom/geom_triangle_intersect.h
	geom/geom_volume.h
)

set(SRC_UTIL_HEADERS
	../util/util_atomic.h
	../util/util_color.h
	../util/util_half.h
	../util/util_hash.h
	../util/util_math.h
	../util/util_math_fast.h
	../util/util_static_assert.h
	../util/util_transform.h
	../util/util_texture.h
	../util/util_types.h
)

set(SRC_SPLIT_HEADERS
	split/kernel_buffer_update.h
	split/kernel_data_init.h
	split/kernel_direct_lighting.h
	split/kernel_do_volume.h
	split/kernel_holdout_emission_blurring_pathtermination_ao.h
	split/kernel_indirect_background.h
	split/kernel_indirect_subsurface.h
	split/kernel_lamp_emission.h
	split/kernel_next_iteration_setup.h
	split/kernel_path_init.h
	split/kernel_queue_enqueue.h
	split/kernel_scene_intersect.h
	split/kernel_shader_eval.h
	split/kernel_shadow_blocked.h
	split/kernel_split_common.h
	split/kernel_split_data.h
	split/kernel_split_data_types.h
	split/kernel_subsurface_scatter.h
)

# CUDA module

if(WITH_CYCLES_CUDA_BINARIES)
	# 32 bit or 64 bit
	if(CUDA_64_BIT_DEVICE_CODE)
		set(CUDA_BITS 64)
	else()
		set(CUDA_BITS 32)
	endif()

	# CUDA version
	execute_process(COMMAND ${CUDA_NVCC_EXECUTABLE} "--version" OUTPUT_VARIABLE NVCC_OUT)
	string(REGEX REPLACE ".*release ([0-9]+)\\.([0-9]+).*" "\\1" CUDA_VERSION_MAJOR "${NVCC_OUT}")
	string(REGEX REPLACE ".*release ([0-9]+)\\.([0-9]+).*" "\\2" CUDA_VERSION_MINOR "${NVCC_OUT}")
	set(CUDA_VERSION "${CUDA_VERSION_MAJOR}${CUDA_VERSION_MINOR}")

	# warn for other versions
	if(CUDA_VERSION MATCHES "80")
	else()
		message(WARNING
			"CUDA version ${CUDA_VERSION_MAJOR}.${CUDA_VERSION_MINOR} detected, "
			"build may succeed but only CUDA 8.0 is officially supported")
	endif()

	# build for each arch
	set(cuda_sources kernels/cuda/kernel.cu kernels/cuda/kernel_split.cu
		${SRC_HEADERS}
		${SRC_KERNELS_CUDA_HEADERS}
		${SRC_BVH_HEADERS}
		${SRC_SVM_HEADERS}
		${SRC_GEOM_HEADERS}
		${SRC_CLOSURE_HEADERS}
		${SRC_UTIL_HEADERS}
	)
	set(cuda_cubins)

	macro(CYCLES_CUDA_KERNEL_ADD arch split experimental)
		if(${split})
			set(cuda_extra_flags "-D__SPLIT__")
			set(cuda_cubin kernel_split)
		else()
			set(cuda_extra_flags "")
			set(cuda_cubin kernel)
		endif()

		if(${experimental})
			set(cuda_extra_flags ${cuda_extra_flags} -D__KERNEL_EXPERIMENTAL__)
			set(cuda_cubin ${cuda_cubin}_experimental)
		endif()

		set(cuda_cubin ${cuda_cubin}_${arch}.cubin)

		if(WITH_CYCLES_DEBUG)
			set(cuda_debug_flags "-D__KERNEL_DEBUG__")
		else()
			set(cuda_debug_flags "")
		endif()

		set(cuda_nvcc_command ${CUDA_NVCC_EXECUTABLE})
		set(cuda_nvcc_version ${CUDA_VERSION})

		set(cuda_version_flags "-D__KERNEL_CUDA_VERSION__=${cuda_nvcc_version}")
		set(cuda_math_flags "--use_fast_math")

		if(split)
			set(cuda_kernel_src "/kernels/cuda/kernel_split.cu")
		else()
			set(cuda_kernel_src "/kernels/cuda/kernel.cu")
		endif()

		add_custom_command(
			OUTPUT ${cuda_cubin}
			COMMAND ${cuda_nvcc_command}
					-arch=${arch}
					${CUDA_NVCC_FLAGS}
					-m${CUDA_BITS}
					--cubin ${CMAKE_CURRENT_SOURCE_DIR}${cuda_kernel_src}
					-o ${CMAKE_CURRENT_BINARY_DIR}/${cuda_cubin}
					--ptxas-options="-v"
					${cuda_arch_flags}
					${cuda_version_flags}
					${cuda_math_flags}
					${cuda_extra_flags}
					${cuda_debug_flags}
					-I${CMAKE_CURRENT_SOURCE_DIR}/../util
					-I${CMAKE_CURRENT_SOURCE_DIR}/svm
					-DCCL_NAMESPACE_BEGIN=
					-DCCL_NAMESPACE_END=
					-DNVCC
			DEPENDS ${cuda_sources})

		delayed_install("${CMAKE_CURRENT_BINARY_DIR}" "${cuda_cubin}" ${CYCLES_INSTALL_PATH}/lib)
		list(APPEND cuda_cubins ${cuda_cubin})

		unset(cuda_extra_flags)
		unset(cuda_debug_flags)

		unset(cuda_nvcc_command)
		unset(cuda_nvcc_version)
	endmacro()

	foreach(arch ${CYCLES_CUDA_BINARIES_ARCH})
		# Compile regular kernel
		CYCLES_CUDA_KERNEL_ADD(${arch} FALSE FALSE)

		if(WITH_CYCLES_CUDA_SPLIT_KERNEL_BINARIES)
			# Compile split kernel
			CYCLES_CUDA_KERNEL_ADD(${arch} TRUE FALSE)
		endif()
	endforeach()

	add_custom_target(cycles_kernel_cuda ALL DEPENDS ${cuda_cubins})
endif()

# OSL module

if(WITH_CYCLES_OSL)
	add_subdirectory(osl)
	add_subdirectory(shaders)
endif()

# CPU module

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

if(CXX_HAS_SSE)
	list(APPEND SRC
		kernels/cpu/kernel_sse2.cpp
		kernels/cpu/kernel_sse3.cpp
		kernels/cpu/kernel_sse41.cpp
		kernels/cpu/kernel_split_sse2.cpp
		kernels/cpu/kernel_split_sse3.cpp
		kernels/cpu/kernel_split_sse41.cpp
	)

	set_source_files_properties(kernels/cpu/kernel_sse2.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_SSE2_KERNEL_FLAGS}")
	set_source_files_properties(kernels/cpu/kernel_sse3.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_SSE3_KERNEL_FLAGS}")
	set_source_files_properties(kernels/cpu/kernel_sse41.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_SSE41_KERNEL_FLAGS}")
	set_source_files_properties(kernels/cpu/kernel_split_sse2.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_SSE2_KERNEL_FLAGS}")
	set_source_files_properties(kernels/cpu/kernel_split_sse3.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_SSE3_KERNEL_FLAGS}")
	set_source_files_properties(kernels/cpu/kernel_split_sse41.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_SSE41_KERNEL_FLAGS}")
endif()

if(CXX_HAS_AVX)
	list(APPEND SRC
		kernels/cpu/kernel_avx.cpp
		kernels/cpu/kernel_split_avx.cpp
	)
	set_source_files_properties(kernels/cpu/kernel_avx.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_AVX_KERNEL_FLAGS}")
	set_source_files_properties(kernels/cpu/kernel_split_avx.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_AVX_KERNEL_FLAGS}")
endif()

if(CXX_HAS_AVX2)
	list(APPEND SRC
		kernels/cpu/kernel_avx2.cpp
		kernels/cpu/kernel_split_avx2.cpp
	)
	set_source_files_properties(kernels/cpu/kernel_avx2.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_AVX2_KERNEL_FLAGS}")
	set_source_files_properties(kernels/cpu/kernel_split_avx2.cpp PROPERTIES COMPILE_FLAGS "${CYCLES_AVX2_KERNEL_FLAGS}")
endif()

add_library(cycles_kernel
	${SRC}
	${SRC_HEADERS}
	${SRC_KERNELS_CPU_HEADERS}
	${SRC_KERNELS_CUDA_HEADERS}
	${SRC_BVH_HEADERS}
	${SRC_CLOSURE_HEADERS}
	${SRC_SVM_HEADERS}
	${SRC_GEOM_HEADERS}
	${SRC_SPLIT_HEADERS}
)

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

delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "kernels/opencl/kernel.cl" ${CYCLES_INSTALL_PATH}/kernel/kernels/opencl)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "kernels/opencl/kernel_state_buffer_size.cl" ${CYCLES_INSTALL_PATH}/kernel/kernels/opencl)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "kernels/opencl/kernel_data_init.cl" ${CYCLES_INSTALL_PATH}/kernel/kernels/opencl)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "kernels/opencl/kernel_path_init.cl" ${CYCLES_INSTALL_PATH}/kernel/kernels/opencl)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "kernels/opencl/kernel_queue_enqueue.cl" ${CYCLES_INSTALL_PATH}/kernel/kernels/opencl)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "kernels/opencl/kernel_scene_intersect.cl" ${CYCLES_INSTALL_PATH}/kernel/kernels/opencl)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "kernels/opencl/kernel_lamp_emission.cl" ${CYCLES_INSTALL_PATH}/kernel/kernels/opencl)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "kernels/opencl/kernel_do_volume.cl" ${CYCLES_INSTALL_PATH}/kernel/kernels/opencl)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "kernels/opencl/kernel_indirect_background.cl" ${CYCLES_INSTALL_PATH}/kernel/kernels/opencl)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "kernels/opencl/kernel_shader_eval.cl" ${CYCLES_INSTALL_PATH}/kernel/kernels/opencl)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "kernels/opencl/kernel_holdout_emission_blurring_pathtermination_ao.cl" ${CYCLES_INSTALL_PATH}/kernel/kernels/opencl)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "kernels/opencl/kernel_subsurface_scatter.cl" ${CYCLES_INSTALL_PATH}/kernel/kernels/opencl)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "kernels/opencl/kernel_direct_lighting.cl" ${CYCLES_INSTALL_PATH}/kernel/kernels/opencl)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "kernels/opencl/kernel_shadow_blocked.cl" ${CYCLES_INSTALL_PATH}/kernel/kernels/opencl)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "kernels/opencl/kernel_next_iteration_setup.cl" ${CYCLES_INSTALL_PATH}/kernel/kernels/opencl)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "kernels/opencl/kernel_indirect_subsurface.cl" ${CYCLES_INSTALL_PATH}/kernel/kernels/opencl)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "kernels/opencl/kernel_buffer_update.cl" ${CYCLES_INSTALL_PATH}/kernel/kernels/opencl)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "kernels/cuda/kernel.cu" ${CYCLES_INSTALL_PATH}/kernel/kernels/cuda)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "kernels/cuda/kernel_split.cu" ${CYCLES_INSTALL_PATH}/kernel/kernels/cuda)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_HEADERS}" ${CYCLES_INSTALL_PATH}/kernel)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_KERNELS_CUDA_HEADERS}" ${CYCLES_INSTALL_PATH}/kernel/kernels/cuda)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_BVH_HEADERS}" ${CYCLES_INSTALL_PATH}/kernel/bvh)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_CLOSURE_HEADERS}" ${CYCLES_INSTALL_PATH}/kernel/closure)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_SVM_HEADERS}" ${CYCLES_INSTALL_PATH}/kernel/svm)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_GEOM_HEADERS}" ${CYCLES_INSTALL_PATH}/kernel/geom)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_UTIL_HEADERS}" ${CYCLES_INSTALL_PATH}/kernel)
delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_SPLIT_HEADERS}" ${CYCLES_INSTALL_PATH}/kernel/split)

