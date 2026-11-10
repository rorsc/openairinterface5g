# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_mempool test_mempool.c)
target_link_libraries(test_mempool PRIVATE utils pthread)
add_test(NAME test_mempool COMMAND ./test_mempool)
