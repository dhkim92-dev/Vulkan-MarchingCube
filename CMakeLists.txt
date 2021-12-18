cmake_minimum_required(VERSION 3.10)
PROJECT(Vulkan_Marchingcube VERSION 1.0.0)
SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED True)

SET(VULKAN_SDK $ENV{VULKAN_SDK})
add_definitions(-DGLFW_INCLUDE_VULKAN)
INCLUDE_DIRECTORIES(externals/VKEngine/include externals/VKEngine/externals/glm)
ADD_SUBDIRECTORY(externals/VKEngine)

IF(WIN32)
    SET(GLFW_PATH $ENV{GLFW_PATH})
    INCLUDE_DIRECTORIES(${VULKAN_SDK}/include ${GLFW_PATH}/include include)
    LINK_DIRECTORIES(${VULKAN_SDK}/Lib ${GLFW_PATH}/lib, externals/VKEngine/lib)
    LINK_LIBRARIES(vulkan-1 VKEngine glfw3 gdi32)
    SET(GLFW_LINK glfw3)
    SET(VULKAN_LINK vulkan-1)
    
else()
    INCLUDE_DIRECTORIES(${VULKAN_SDK}/include /usr/local/include /usr/include)
    LINK_DIRECTORIES(${VULKAN_SDK}/lib} externals/VKEngine/lib /usr/local/lib /usr/lib)
    LINK_LIBRARIES(vulkan VKEngine glfw)
    set(GLFW_LINK glfw)
    SET(VULKAN_LINK vulkan)
endif()

INCLUDE_DIRECTORIES(include src)

SET(PROJECT_SOURCE 
    src/main.cpp
    src/marchingcube.cpp
    src/scan.cpp
    include/marchingcube.h
    include/scan.h
)

add_executable(Vulkan_Marchingcube ${PROJECT_SOURCE})
