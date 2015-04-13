cmake_minimum_required(VERSION 2.6)

################################################################
#                   CUSTOMIZE                                  #
################################################################
project (VirtualDeviceToolkit)			# name of the project
set (vdt_source_dir_name "source")		# name of the dir containing ALL implementations
set (vdt_header_dir_name "include")		# name of the dir containing ALL declarations
set (vdt_utility_dir_name "utils")		# name of the subdir for third-party source/headers

################################################################
#                   SOURCE DIR                                 #
################################################################
set (PROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${vdt_source_dir_name})
include_directories("${PROJECT_SOURCE_DIR}")	# because TEMPLATES are used!!

################################################################
#                   INCLUDE DIR                                #
################################################################
set (PROJECT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${vdt_header_dir_name})
include_directories("${PROJECT_INCLUDE_DIR}")

################################################################
#                   ADD PROJECT FILES                          #
################################################################
#file(GLOB_RECURSE source_files "cpp/*.cpp")
#file(GLOB_RECURSE header_files "include/*.hpp" "include/*.h")
file( GLOB  vdt_source_files "${vdt_source_dir_name}/*.cpp")
source_group("${vdt_source_dir_name}" FILES ${vdt_source_files})
file( GLOB  vdt_utils_source_files "${vdt_source_dir_name}/${vdt_utility_dir_name}/*.cpp")
source_group("${vdt_source_dir_name}\\${vdt_utility_dir_name}" FILES ${vdt_utils_source_files})
file( GLOB  vdt_header_files "${vdt_header_dir_name}/*.hpp" "${vdt_header_dir_name}/*.h")
source_group("${vdt_header_dir_name}" FILES ${vdt_header_files})
file( GLOB  vdt_utils_header_files "${vdt_header_dir_name}/${vdt_utility_dir_name}/*.h" "${vdt_header_dir_name}/${vdt_utility_dir_name}/*.hpp" )
source_group("${vdt_header_dir_name}\\${vdt_utility_dir_name}" FILES ${vdt_utils_header_files})

################################################################
#                   EXTERNAL DEPENDENCIES                      #
################################################################
find_package(OpenCV REQUIRED)						#OpenCV
include_directories("${OpenCV_INCLUDE_DIRS}")		#OpenCV

################################################################
#                   BUILD SETUP                                #
################################################################
include_directories("${PROJECT_BINARY_DIR}")
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})


add_library(${PROJECT_NAME} STATIC
	${vdt_source_files}
	${vdt_utils_source_files}
	${vdt_header_files}
	${vdt_utils_header_files}
	)

set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX _d)
#set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE Cxx)