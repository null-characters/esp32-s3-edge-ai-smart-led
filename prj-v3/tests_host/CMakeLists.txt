# 主机端单元测试 CMakeLists.txt
# 使用 Unity 测试框架，无需硬件

cmake_minimum_required(VERSION 3.22)

project(host_tests C)

# Unity 测试框架
include(FetchContent)
FetchContent_Declare(
    Unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG v2.5.2
)
FetchContent_MakeAvailable(Unity)

# 测试可执行文件
add_executable(host_tests
    test_host_main.c
)

target_include_directories(host_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${unity_SOURCE_DIR}/src
)

target_link_libraries(host_tests PRIVATE
    unity
)

# 编译选项
target_compile_options(host_tests PRIVATE
    -Wall
    -Wextra
    -std=c11
)

# 运行测试
enable_testing()
add_test(NAME host_tests COMMAND host_tests)

# 自定义目标：运行测试并输出详细结果
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS host_tests
)