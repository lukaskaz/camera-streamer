cmake_minimum_required(VERSION 3.10)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

find_package(Boost COMPONENTS program_options REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

find_package(PkgConfig REQUIRED)
pkg_check_modules(CAMERA REQUIRED libcamera)
find_library(LIBCAMERA_LIBRARY libcamera.so REQUIRED)
find_library(LIBCAMERA_BASE_LIBRARY libcamera-base.so REQUIRED)

include_directories(${CAMERA_INCLUDE_DIRS})
include_directories(${CAMERA_BASE_INCLUDE_DIRS})

include(ExternalProject)

set(source_dir "${CMAKE_BINARY_DIR}/liblccv-src")
set(build_dir "${CMAKE_BINARY_DIR}/liblccv-build")

# file(GLOB patches "${CMAKE_SOURCE_DIR}/patches/*.patch")
# foreach(patch ${patches})
#   list(APPEND patching_cmd git am -s --keep-cr < ${patch} &&)
# endforeach()
# list(APPEND patching_cmd echo "End of patches")

EXTERNALPROJECT_ADD(
  liblccv
  GIT_REPOSITORY    https://github.com/kbarni/LCCV.git
  GIT_TAG           main
  PATCH_COMMAND     ${patching_cmd}
  PREFIX            liblccv-workspace
  SOURCE_DIR        ${source_dir}
  BINARY_DIR        ${build_dir}
  CONFIGURE_COMMAND mkdir /${build_dir}/build &> /dev/null
  BUILD_COMMAND     cd ${build_dir}/build && cmake -D BUILD_SHARED_LIBS=ON
                    ${source_dir} && make -j 4
  UPDATE_COMMAND    ""
  INSTALL_COMMAND   ""
  TEST_COMMAND      ""
)

include_directories(${source_dir}/include)
link_directories(${build_dir}/build)
