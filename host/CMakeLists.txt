add_executable(dsp_host_render src/main.cpp)
target_include_directories(dsp_host_render PRIVATE include)
target_link_libraries(dsp_host_render PRIVATE dsp)
