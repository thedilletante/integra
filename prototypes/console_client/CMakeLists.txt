cmake_minimum_required(VERSION 3.9)
project(console_client)

set(CMAKE_CXX_STANDARD 17)

set(ROOT_PROJECT_DIR ${CMAKE_CURRENT_LIST_DIR}/../..)
set(LIBS_DIR ${ROOT_PROJECT_DIR}/libs)


set(PROJECT_SOURCES
  main.cpp
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

target_include_directories(${PROJECT_NAME}
  PRIVATE
    ${LIBS_DIR}/asio/asio/include
    ${LIBS_DIR}/RxCpp/Rx/v2/src
)

target_compile_definitions(${PROJECT_NAME}
  PRIVATE
    ASIO_STANDALONE
)