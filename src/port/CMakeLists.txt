
# ======================================================================
#               Parameters
# ======================================================================
set(DMUART_MCU_SERIES "stm32f7" CACHE STRING "Target MCU series")

# ======================================================================
#               dmuart_port Module Configuration
# ======================================================================
# Name of the module 
set(DMOD_MODULE_NAME        dmuart_port)

# Version is already set above and used in project()
# No need to set it again here

# Author (should be string)
set(DMOD_AUTHOR_NAME        "Patryk Kubiak")

# Stack size for the module (should be integer)
set(DMOD_STACK_SIZE         1024)

# Path to the DMR file for this module
set(DMOD_DMR_PATH           ${CMAKE_SOURCE_DIR}/dmuart_port.dmr)

# Determine common source files based on MCU family
set(COMMON_SOURCES "")
if(DMUART_MCU_SERIES STREQUAL "stm32f7")
    # STM32F7 uses stm32_common helper
    set(COMMON_SOURCES stm32_common/stm32_common.c)
endif()

#
#   dmod_add_library - create a library module
#   it has the same signature as add_library
#   and can be used in the same way after the creation
#   (for example, to link libraries)
#
dmod_add_library(${DMOD_MODULE_NAME} ${DMOD_MODULE_VERSION}
    # List of source files - can include C and C++ files
    ${DMUART_MCU_SERIES}/port.c
    ${COMMON_SOURCES}
)

# One dmod_link_modules() call: each call rewrites the target's version
# requirements file.
# stm32f7: RX by DMA - dmdma 0.4 (its lease API), dmheap (the buffer from
# dmod-boot's "dma" heap, never cached).
set(DMUART_PORT_MODULES
    dm_sw_ring
    dmclk_port
    dmosi
)
if(DMUART_MCU_SERIES STREQUAL "stm32f7")
    list(APPEND DMUART_PORT_MODULES
        dmdma@>=0.4
        dmheap
    )
endif()
dmod_link_modules(${DMOD_MODULE_NAME} ${DMUART_PORT_MODULES})

target_include_directories(${DMOD_MODULE_NAME} BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(${DMOD_MODULE_NAME}_if INTERFACE
    ${CMAKE_SOURCE_DIR}/include
)
