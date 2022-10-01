cmake_minimum_required(VERSION 3.20)
project(protohackers)

function(find_python_virtualenv python_executable virtualenv_root)
    set(Python_ROOT_DIR "${virtualenv_root}")
    find_package(Python COMPONENTS Interpreter REQUIRED)
    set("${python_executable}" "${Python_EXECUTABLE}" PARENT_SCOPE)
endfunction()

set(PROTOHACKERS_VIRTUALENV_ROOT "${CMAKE_CURRENT_LIST_DIR}/.virtualenv")
if(NOT EXISTS "${PROTOHACKERS_VIRTUALENV_ROOT}")
    list(APPEND PROTOHACKERS_VIRTUALENV_PACKAGES conan pre-commit)

    find_program(PROTOHACKERS_PYTHON python3 REQUIRED)
    execute_process(
        COMMAND "${PROTOHACKERS_PYTHON}" -m virtualenv "${PROTOHACKERS_VIRTUALENV_ROOT}"
        COMMAND_ERROR_IS_FATAL ANY)

    find_python_virtualenv(PROTOHACKERS_VIRTUALENV_PYTHON "${PROTOHACKERS_VIRTUALENV_ROOT}")
    execute_process(
        COMMAND "${PROTOHACKERS_VIRTUALENV_PYTHON}" -m pip install ${PROTOHACKERS_VIRTUALENV_PACKAGES}
        COMMAND_ERROR_IS_FATAL ANY)
else()
    find_python_virtualenv(PROTOHACKERS_VIRTUALENV_PYTHON "${PROTOHACKERS_VIRTUALENV_ROOT}")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/.git/hooks/pre-commit")
    execute_process(
        COMMAND "${PROTOHACKERS_VIRTUALENV_PYTHON}" -m pre_commit install
        COMMAND_ERROR_IS_FATAL ANY)
endif()

if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/conanbuildinfo.cmake")
    execute_process(
        COMMAND "${PROTOHACKERS_VIRTUALENV_PYTHON}" -m conans.conan install --build=missing "${CMAKE_CURRENT_LIST_DIR}/conanfile.txt"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        COMMAND_ERROR_IS_FATAL ANY)
endif()

include("${CMAKE_CURRENT_BINARY_DIR}/conanbuildinfo.cmake")
conan_basic_setup(TARGETS)

add_executable(0_smoke_test
    src/common.cpp
    src/0_smoke_test.cpp)
set_property(TARGET 0_smoke_test PROPERTY CXX_STANDARD 20)
target_link_libraries(0_smoke_test CONAN_PKG::liburing CONAN_PKG::fmt)
