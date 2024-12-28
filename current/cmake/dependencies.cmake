cmake_minimum_required(VERSION 3.10)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

find_package(OpenCV REQUIRED COMPONENTS core highgui videoio imgproc imgcodecs gapi)
find_package(InferenceEngine REQUIRED)

find_package(Boost COMPONENTS program_options REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

include(ExternalProject)

set(source_dir "${CMAKE_BINARY_DIR}/libcamera-src")
set(build_dir "${CMAKE_BINARY_DIR}/libcamera-build")

EXTERNALPROJECT_ADD(
  libcamera
  GIT_REPOSITORY    https://github.com/lukaskaz/lib-camera.git
  GIT_TAG           main
  PATCH_COMMAND     ""
  PREFIX            libcamera-workspace
  SOURCE_DIR        ${source_dir}
  BINARY_DIR        ${build_dir}
  CONFIGURE_COMMAND mkdir /${build_dir}/build &> /dev/null
  BUILD_COMMAND     cd ${build_dir}/build && cmake -D BUILD_SHARED_LIBS=ON
                    ${source_dir} && make -j 4
  UPDATE_COMMAND    ""
  INSTALL_COMMAND   ""
  TEST_COMMAND      ""
)

include_directories(${source_dir}/inc)
link_directories(${build_dir}/build)

set(source_dir "${CMAKE_BINARY_DIR}/liblogger-src")
set(build_dir "${CMAKE_BINARY_DIR}/liblogger-build")

EXTERNALPROJECT_ADD(
  liblogger
  GIT_REPOSITORY    https://github.com/lukaskaz/lib-logger.git
  GIT_TAG           main
  PATCH_COMMAND     ""
  PREFIX            liblogger-workspace
  SOURCE_DIR        ${source_dir}
  BINARY_DIR        ${build_dir}
  CONFIGURE_COMMAND mkdir /${build_dir}/build &> /dev/null
  BUILD_COMMAND     cd ${build_dir}/build && cmake -D BUILD_SHARED_LIBS=ON
                    ${source_dir} && make
  UPDATE_COMMAND    ""
  INSTALL_COMMAND   "" 
  TEST_COMMAND      ""
)

include_directories(${source_dir}/inc)
link_directories(${build_dir}/build)

set(source_dir "${CMAKE_BINARY_DIR}/libtts-src")
set(build_dir "${CMAKE_BINARY_DIR}/libtts-build")
set(vcpkg_dir "${CMAKE_BINARY_DIR}/libtts-build/build/vcpkg_installed")

EXTERNALPROJECT_ADD(
  libtts
  GIT_REPOSITORY    https://github.com/lukaskaz/lib-tts.git
  GIT_TAG           main
  PATCH_COMMAND     ""
  PREFIX            libtts-workspace
  SOURCE_DIR        ${source_dir}
  BINARY_DIR        ${build_dir}
  CONFIGURE_COMMAND mkdir /${build_dir}/build &> /dev/null
  BUILD_COMMAND     cd ${build_dir}/build &&
                    cmake -DBUILD_SHARED_LIBS=ON ${source_dir} && make
  UPDATE_COMMAND    ""
  INSTALL_COMMAND   "" 
  TEST_COMMAND      ""
)

include_directories(${source_dir}/inc)
link_directories(${build_dir}/build)
link_directories(${vcpkg_dir}/lib)

set(source_dir "${CMAKE_BINARY_DIR}/libshellcmd-src")
set(build_dir "${CMAKE_BINARY_DIR}/libshellcmd-build")

EXTERNALPROJECT_ADD(
  libshellcmd
  GIT_REPOSITORY    https://github.com/lukaskaz/lib-shellcmd.git
  GIT_TAG           main
  PATCH_COMMAND     ""
  PREFIX            libshellcmd-workspace
  SOURCE_DIR        ${source_dir}
  BINARY_DIR        ${build_dir}
  CONFIGURE_COMMAND mkdir /${build_dir}/build &> /dev/null
  BUILD_COMMAND     cd ${build_dir}/build && cmake -D BUILD_SHARED_LIBS=ON
                    ${source_dir} && make
  UPDATE_COMMAND    ""
  INSTALL_COMMAND   "" 
  TEST_COMMAND      ""
)

include_directories(${source_dir}/inc)
link_directories(${build_dir}/build)
