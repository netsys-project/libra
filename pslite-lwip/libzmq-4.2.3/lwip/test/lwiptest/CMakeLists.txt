cmake_minimum_required(VERSION 3.5)

project(lwiptest C)

set(LWIP_DIR         ${CMAKE_CURRENT_SOURCE_DIR}/../..)

set(RTE_SDK $ENV{RTE_SDK})
set(RTE_TARGET $ENV{RTE_TARGET})
set(DPDK_INCLUDE_DIRS "${RTE_SDK}/${RTE_TARGET}/include")
set(DPDK_LIB_DIRS "${RTE_SDK}/${RTE_TARGET}/lib")

link_directories(${DPDK_LIB_DIRS})

set (LWIP_DEFINITIONS -DLWIP_DEBUG)
set (LWIP_INCLUDE_DIRS
    "${LWIP_DIR}/src/include"
    "${LWIP_DIR}/src/unix/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/"
	"${DPDK_INCLUDE_DIRS}"
)

set (lwiptest_SRCS
		${CMAKE_CURRENT_SOURCE_DIR}/socket_api.c
		${CMAKE_CURRENT_SOURCE_DIR}/main.c)

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O2 -msse4.1")

include(${LWIP_DIR}/src/Filelists.cmake)

#add_library(lwip ${lwipnoapps_SRCS} ${lwipcontribportunix_SRCS} ${lwipcontribportunixnetifs_SRCS})
#target_compile_options(lwip PRIVATE ${LWIP_COMPILER_FLAGS})
#target_compile_definitions(lwip PRIVATE ${LWIP_DEFINITIONS} ${LWIP_MBEDTLS_DEFINITIONS})
#target_include_directories(lwip PRIVATE ${LWIP_INCLUDE_DIRS} ${LWIP_MBEDTLS_INCLUDE_DIRS})
#target_link_libraries(lwip ${LWIP_SANITIZER_LIBS})

add_executable(lwiptest ${lwipnoapps_SRCS} ${lwiptest_SRCS})
target_compile_options(lwiptest PRIVATE ${LWIP_COMPILER_FLAGS})
target_compile_definitions(lwiptest PRIVATE ${LWIP_DEFINITIONS} ${LWIP_MBEDTLS_DEFINITIONS})
target_include_directories(lwiptest PRIVATE ${LWIP_INCLUDE_DIRS} ${LWIP_MBEDTLS_INCLUDE_DIRS})

target_link_libraries(lwiptest PUBLIC "-L${DPDK_LIB_DIRS}" "-Wl,--whole-archive" rte_mempool_octeontx rte_pci rte_kvargs rte_ethdev rte_bus_pci rte_bus_vdev rte_eal rte_mempool rte_mempool_ring rte_ring rte_mbuf rte_pmd_ixgbe rte_hash rte_net rte_pmd_virtio "-Wl,--no-whole-archive" pthread dpdk numa dl)
