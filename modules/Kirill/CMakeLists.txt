include_directories(include)

get_filename_component(DIR_NAME ${CMAKE_CURRENT_LIST_DIR} NAME)
set(MODULE "${DIR_NAME}")

set(MODULE      "${DIR_NAME}")
set(APP         "app_${MODULE}")
set(LIBRARY     "lib_${MODULE}")

set(TYPE        DOUBLE)
set(THREAD_RUN  0)
set(MKL_RUN     0)
set(AVX2_RUN    0)
set(FPGA_RUN 	0)
set(CPUGPU_RUN  1)

#   Setting paths
set(INPUT_EULER_SETTING_PATH    "${CMAKE_SOURCE_DIR}/initial/setting3.ini")
set(INPUT_EULER_FUNCTION_PATH   "${CMAKE_SOURCE_DIR}/initial/function3.txt")
set(RESULT_EULER_PATH           "${CMAKE_SOURCE_DIR}/result/Kirill/euler3D_double_fpga.txt")

if(${FPGA_RUN})
	set(KERNEL_CL_PATH    		"${CMAKE_BINARY_DIR}/modules/Kirill/src/kernel.aocx")
elseif(${CPUGPU_RUN})
	set(KERNEL_CL_PATH    		"${CMAKE_BINARY_DIR}/modules/Kirill/src/kernel.cl")
endif()
#

find_package(AVX)
find_package(MKL)
if(${CPUGPU_RUN})
	find_package(OpenCL)
elseif(${FPGA_RUN})
	find_package(AlteraCL)
endif()

if(${AVX2_RUN} AND AVX_FOUND)
    set(AVX2_RUN, 1)
    message(STATUS "AVX2 RUN")
elseif(${MKL_RUN} AND MKL_FOUND)
    set(AVX2_RUN, 0)
    set(MKL_RUN, 1)
    message(STATUS "MKL RUN")
elseif(${CPUGPU_RUN} AND OPENCL_FOUND)
    set(MKL_RUN, 0)
	set(CPUGPU_RUN, 1)
	message(STATUS "CPUGPU_RUN")
else()
	set(CPUGPU_RUN, 0)
endif()

configure_file("include/utils/ts.h.in" "${CMAKE_CURRENT_LIST_DIR}/include/utils/ts.h")

add_subdirectory(src)
add_subdirectory(app)