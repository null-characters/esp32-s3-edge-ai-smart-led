# 主机端单元测试 CMakeLists.txt
# 使用 Unity 测试框架，无需硬件

cmake_minimum_required(VERSION 3.22)

project(host_tests C)

# 覆盖率选项
option(ENABLE_COVERAGE "Enable code coverage reporting" OFF)

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

# 覆盖率配置
if(ENABLE_COVERAGE)
    message(STATUS "Code coverage enabled")
    
    # 添加覆盖率编译选项
    target_compile_options(host_tests PRIVATE
        -fprofile-arcs
        -ftest-coverage
        -O0
        -g
    )
    
    # 链接覆盖率库
    target_link_options(host_tests PRIVATE
        -fprofile-arcs
        -ftest-coverage
    )
    
    # 查找 gcov/lcov
    find_program(GCOV_PATH gcov)
    find_program(LCOV_PATH lcov)
    find_program(GENHTML_PATH genhtml)
    
    if(LCOV_PATH AND GENHTML_PATH)
        # 生成覆盖率报告目标
        add_custom_target(coverage
            # 初始化覆盖率数据
            COMMAND ${LCOV_PATH} --base-directory ${CMAKE_SOURCE_DIR} 
                    --directory ${CMAKE_BINARY_DIR} 
                    --zerocounters
            # 运行测试
            COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
            # 收集覆盖率数据
            COMMAND ${LCOV_PATH} --base-directory ${CMAKE_SOURCE_DIR}
                    --directory ${CMAKE_BINARY_DIR}
                    --capture
                    --output-file coverage.info
            # 过滤系统头文件
            COMMAND ${LCOV_PATH} --remove coverage.info '/usr/*' 
                    --output-file coverage.info.cleaned
            # 生成 HTML 报告
            COMMAND ${GENHTML_PATH} -o coverage_report coverage.info.cleaned
            # 清理临时文件
            COMMAND ${CMAKE_COMMAND} -E remove coverage.info coverage.info.cleaned
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS host_tests
            COMMENT "Generating code coverage report..."
        )
        
        message(STATUS "Coverage report target available: make coverage")
    else()
        message(WARNING "lcov/genhtml not found, coverage report disabled")
    endif()
endif()

# 运行测试
enable_testing()
add_test(NAME host_tests COMMAND host_tests)

# 自定义目标：运行测试并输出详细结果
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS host_tests
)

# 帮助信息
message(STATUS "")
message(STATUS "Host Tests Configuration:")
message(STATUS "  Build: cmake -B build -S .")
message(STATUS "  Run:   cmake --build build && ./build/host_tests")
message(STATUS "  Coverage: cmake -B build -S . -DENABLE_COVERAGE=ON && cmake --build build && cmake --build build --target coverage")
message(STATUS "")
