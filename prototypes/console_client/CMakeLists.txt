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


# stun-library
set(STUN_DIR ${LIBS_DIR}/stun-msg)

add_library(stunmsg
  ${STUN_DIR}/src/crc32.c
  ${STUN_DIR}/src/crc32.h
  ${STUN_DIR}/src/hmac_sha1.c
  ${STUN_DIR}/src/hmac_sha1.h
  ${STUN_DIR}/src/sha1.c
  ${STUN_DIR}/src/sha1.h
  ${STUN_DIR}/src/md5.c
  ${STUN_DIR}/src/md5.h
  ${STUN_DIR}/src/stun_msg.c
  ${STUN_DIR}/include/stun/msg.h
)

target_include_directories(stunmsg
  PUBLIC
  ${STUN_DIR}/include
  )

target_compile_definitions(${PROJECT_NAME}
  PRIVATE
    ASIO_STANDALONE
)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    stunmsg
)