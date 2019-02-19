cmake_minimum_required(VERSION 3.0)

project(mdet)


set(MDET_VERSION_STRING "1.0.0")
add_definitions(-DMDET_VERSION_STRING="${MDET_VERSION_STRING}")

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(TARGET_MODIFIER "32")
  set(TARGET_MODIFIER_PATH "x86")
  add_definitions(-DMDET_HOST_POINTER_SIZE=32)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(TARGET_MODIFIER "64")
  set(TARGET_MODIFIER_PATH "x64")
  add_definitions(-DMDET_HOST_POINTER_SIZE=64)
else()
  message(FATAL_ERROR "unexpected platform")
endif()
if (MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

###############################################################################
# mdet##.exe
###############################################################################
file(GLOB MDET_ROOT
  "src/*.hpp" "src/*.cpp"
)

source_group("Source" FILES ${MDET_ROOT})
set(MDET_SOURCES ${MDET_ROOT} CMakeLists.cmake)

###################
# Include OpenCV
# CMake OpenCV and then run the INSTALL build target
# E.g. D:\tools\opencv-4.0.0\builds\vs2017-64\install
set(OpenCV_DIR
  "D:\\tools\\opencv-4.0.0\\builds\\vs2017-64\\install")
find_package(OpenCV REQUIRED)
# message(${OpenCV_LIBS})
# message(${OpenCV_INCLUDE_DIRS})
# message(${OpenCV_VERSION})


add_executable("mdet${TARGET_MODIFIER}"
  ${MDET_SOURCES}
  )
include_directories(${OpenCV_INCLUDE_DIRS})
target_link_libraries("mdet${TARGET_MODIFIER}" ${OpenCV_LIBS})


if (MSVC)
  # enable parallel build
  target_compile_options("mdet${TARGET_MODIFIER}" PRIVATE "/MP")
  add_compile_options("/w4")
endif()

set_target_properties("mdet${TARGET_MODIFIER}" PROPERTIES
  CXX_STANDARD 17
  OUTPUT_NAME  "mdet${TARGET_MODIFIER}"
)


######################################
# Post build step to copy DLL binaries to same directory
# https://stackoverflow.com/questions/10671916/how-to-copy-dll-files-into-the-same-folder-as-the-executable-using-cmake
#
# find_package OpenCV defines OpenCV_CONFIG_PATH as well as path to all the libs
# ..\bins from that will be the binaries (DLLs)
#
# BUG: this isn't working yet; not sure what I'm doing wrong,
# I see the comment in the output
# the command on the comman dline seemed to work
message("We will copy to $<TARGET_FILE_DIR:mdet${TARGET_MODIFIER}>")
# this would copy the entire directory
# would be resiliant to versioning
# add_custom_command(TARGET "mdet${TARGET_MODIFIER}" POST_BUILD
#   COMMAND ${CMAKE_COMMAND} -E copy_directory
#       "${OpenCV_CONFIG_PATH}/../bin"
#       "$<TARGET_FILE_DIR:mdet${TARGET_MODIFIER}>"
#
# But instead we'll figure out the version suffix on DLLs and do it that way.
# E.g. opencv_core400.dll for 4.0
set(OpenCV_VERSION_SUFFIX "${OpenCV_VERSION_MAJOR}${OpenCV_VERSION_MINOR}${OpenCV_VERSION_PATCH}")
add_custom_command(TARGET "mdet${TARGET_MODIFIER}" POST_BUILD
   COMMAND ${CMAKE_COMMAND} -E copy
       "${OpenCV_CONFIG_PATH}/../bin/opencv_highgui${OpenCV_VERSION_SUFFIX}d.dll"
       "$<TARGET_FILE_DIR:mdet${TARGET_MODIFIER}>"
   COMMAND ${CMAKE_COMMAND} -E copy
       "${OpenCV_CONFIG_PATH}/../bin/opencv_highgui${OpenCV_VERSION_SUFFIX}.dll"
       "$<TARGET_FILE_DIR:mdet${TARGET_MODIFIER}>"
   COMMAND ${CMAKE_COMMAND} -E copy
       "${OpenCV_CONFIG_PATH}/../bin/opencv_core${OpenCV_VERSION_SUFFIX}d.dll"
       "$<TARGET_FILE_DIR:mdet${TARGET_MODIFIER}>"
   COMMAND ${CMAKE_COMMAND} -E copy
       "${OpenCV_CONFIG_PATH}/../bin/opencv_core${OpenCV_VERSION_SUFFIX}.dll"
       "$<TARGET_FILE_DIR:mdet${TARGET_MODIFIER}>"
   COMMAND ${CMAKE_COMMAND} -E copy
       "${OpenCV_CONFIG_PATH}/../bin/opencv_videoio${OpenCV_VERSION_SUFFIX}d.dll"
       "$<TARGET_FILE_DIR:mdet${TARGET_MODIFIER}>"
   COMMAND ${CMAKE_COMMAND} -E copy
       "${OpenCV_CONFIG_PATH}/../bin/opencv_videoio${OpenCV_VERSION_SUFFIX}.dll"
       "$<TARGET_FILE_DIR:mdet${TARGET_MODIFIER}>"
   COMMAND ${CMAKE_COMMAND} -E copy
       "${OpenCV_CONFIG_PATH}/../bin/opencv_imgcodecs${OpenCV_VERSION_SUFFIX}d.dll"
       "$<TARGET_FILE_DIR:mdet${TARGET_MODIFIER}>"
   COMMAND ${CMAKE_COMMAND} -E copy
       "${OpenCV_CONFIG_PATH}/../bin/opencv_imgcodecs${OpenCV_VERSION_SUFFIX}.dll"
       "$<TARGET_FILE_DIR:mdet${TARGET_MODIFIER}>"
   COMMAND ${CMAKE_COMMAND} -E copy
       "${OpenCV_CONFIG_PATH}/../bin/opencv_imgproc${OpenCV_VERSION_SUFFIX}d.dll"
       "$<TARGET_FILE_DIR:mdet${TARGET_MODIFIER}>"
   COMMAND ${CMAKE_COMMAND} -E copy
       "${OpenCV_CONFIG_PATH}/../bin/opencv_imgproc${OpenCV_VERSION_SUFFIX}.dll"
       "$<TARGET_FILE_DIR:mdet${TARGET_MODIFIER}>"
   COMMAND ${CMAKE_COMMAND} -E copy
       "${OpenCV_CONFIG_PATH}/../bin/opencv_ffmpeg${OpenCV_VERSION_SUFFIX}_64.dll"
       "$<TARGET_FILE_DIR:mdet${TARGET_MODIFIER}>"
   COMMENT "Copying OpenCV DLLs to build directory"
       )