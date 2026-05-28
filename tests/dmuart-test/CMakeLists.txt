cmake_minimum_required(VERSION 3.18)

# ======================================================================
#               dmuart_test Module Configuration
# ======================================================================
set(DMOD_MODULE_NAME        dmuart_test)
set(DMOD_AUTHOR_NAME        "Patryk Kubiak")
set(DMOD_STACK_SIZE         2048)

# ======================================================================
#               Create dmuart_test application module
# ======================================================================
dmod_add_executable(${DMOD_MODULE_NAME} ${DMOD_MODULE_VERSION}
    dmuart_test.c
)

dmod_link_modules(${DMOD_MODULE_NAME}
    dmdrvi
    dmini
)

target_include_directories(${DMOD_MODULE_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
