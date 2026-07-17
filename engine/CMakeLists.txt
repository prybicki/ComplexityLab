# cl_engine — ComplexityLab framework: rendering, platform, external toolkits.

file(GLOB_RECURSE CL_ENGINE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(cl_engine STATIC ${CL_ENGINE_SOURCES})

target_include_directories(cl_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(cl_engine SYSTEM PUBLIC ${vulkanmemoryallocator_SOURCE_DIR}/include)

target_link_libraries(cl_engine PUBLIC
    cl_lib
    Vulkan::Vulkan
    glfw
    imgui
    dl
    yaml-cpp
    PkgConfig::LIBAV
)
