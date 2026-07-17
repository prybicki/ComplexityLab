# cl_lib — pure primitives, stdlib + glm only. No Vulkan, no windowing.

file(GLOB_RECURSE CL_LIB_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_library(cl_lib STATIC ${CL_LIB_SOURCES})

target_include_directories(cl_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(cl_lib PUBLIC
    glm::glm
    spdlog::spdlog
    pthread
)
