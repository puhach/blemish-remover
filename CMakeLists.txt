cmake_minimum_required(VERSION 2.8.12)

#PROJECT(Week5)
project(BlemishRemover)
SET(CMAKE_CXX_STANDARD 11)


######################## EDIT IF REQUIRED ####################
# Edit the line below to specify the path to OpenCV directory i.e. the path to the OpenCVConfig.cmake file. Check the examples given below.
#SET(OpenCV_DIR Enter-the-path-of-OpenCV-installation-on-your-system)
set(OpenCV_DIR /opt/opencv/4.4.0/installation/lib/cmake/opencv4)

################### OpenCV_DIR Examples  #####################

### MACOS : /usr/local/Cellar/opencv/3.3.1_1/share/OpenCV/

### UBUNTU : /usr/local/share/OpenCV/

### WINDOWS : C:\Users\yourname\Documents\opencv-3.3.1\build\install

##############################################################



find_package( OpenCV REQUIRED )

include_directories( ${OpenCV_INCLUDE_DIRS})

ADD_EXECUTABLE(reblem submission.cpp)
TARGET_LINK_LIBRARIES(reblem ${OpenCV_LIBS} )
  
if(MSVC)
  SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
  #set_property(TARGET reblem PROPERTY CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
  set_property(TARGET reblem PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
endif()

