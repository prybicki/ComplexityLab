# Apps — one executable per subdirectory, each linking cl_engine.

add_executable(testApp testApp/main.cpp)
target_link_libraries(testApp PRIVATE cl_engine)
