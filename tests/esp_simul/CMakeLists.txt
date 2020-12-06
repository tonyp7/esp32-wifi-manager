cmake_minimum_required(VERSION 3.5)

project(esp_simul)
set(ProjectId esp_simul)

add_library(${ProjectId} STATIC 
	sdkconfig.h
	stub_func.c
	arch/cc.h
)

target_include_directories(${ProjectId} PUBLIC .)

set_target_properties(${ProjectId} PROPERTIES
    C_STANDARD 11
    CXX_STANDARD 14
)

target_compile_options(${ProjectId} PRIVATE -Wall -Werror -Wextra -Wno-error=nonnull-compare)
