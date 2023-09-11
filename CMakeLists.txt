
cmake_minimum_required(VERSION 3.15)
#project (libbfcp CXX)
set(CMAKE_CXX_STANDARD 14)
#SET(CMAKE_BUILD_TYPE "RelWithDebInfo")

add_compile_options(-Wall -Wstrict-prototypes -O2 -Wunused)


find_package(OpenSSL)
if(OPENSSL_FOUND)
	message(status "** OpenSSL Include: ${OPENSSL_INCLUDE_DIR}")
	message(status "** OpenSSL Libraries: ${OPENSSL_LIBRARIES}")
	message(status "** OpenSSL SSL Libraries: ${OPENSSL_SSL_LIBRARY}")
	message(status "** OpenSSL CRYPTO Libraries: ${OPENSSL_CRYPTO_LIBRARY}")
	message(status "** OpenSSL Libraries: ${OPENSSL_VERSION}")
endif()

include_directories(${OPENSSL_INCLUDE_DIR})
include_directories("bfcpmsg")

add_library(bfcpmsg STATIC bfcpmsg/bfcp_messages.c bfcpmsg/bfcp_messages_parse.c bfcpmsg/bfcp_messages_build.c)

add_library(bfcp_cli bfcpsrvclt/bfcpclt/bfcp_participant.cpp)
target_link_libraries(bfcp_cli bfcpmsg ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY} pthread)

add_executable(bfcp_participant bfcpsrvclt/bfcpclt/main.cpp)
target_link_libraries(bfcp_participant bfcp_cli)

add_library(bfcp_srv bfcpsrvclt/bfcpsrv/bfcp_server.cpp bfcpsrvclt/bfcpsrv/bfcp_link_list.cpp bfcpsrvclt/bfcpsrv/bfcp_floor_list.cpp bfcpsrvclt/bfcpsrv/bfcp_user_list.cpp)
target_link_libraries(bfcp_srv bfcpmsg ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY} pthread)

add_executable(bfcp_server bfcpsrvclt/bfcpsrv/main.cpp)
target_link_libraries(bfcp_server bfcp_srv pthread)