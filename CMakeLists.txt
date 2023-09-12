
cmake_minimum_required(VERSION 3.15)
#project (libbfcp CXX)
set(CMAKE_CXX_STANDARD 14)
#SET(CMAKE_BUILD_TYPE "RelWithDebInfo -Wstrict-prototypes")

add_compile_options(-Wall -O2 -Wunused -Wstrict-prototypes)

include_directories("bfcpmsg")
include_directories(" bfcpsrvclt/bfcpclt")

add_library(bfcpmsg STATIC bfcpmsg/bfcp_messages.c bfcpmsg/bfcp_messages_parse.c bfcpmsg/bfcp_messages_build.c)

add_library(bfcp_cli bfcpsrvclt/bfcpclt/bfcp_participant.cpp)
target_link_libraries(bfcp_cli bfcpmsg pthread)

add_executable(bfcp_participant bfcpsrvclt/bfcpclt/main.cpp)
target_link_libraries(bfcp_participant bfcp_cli)

add_library(bfcp_srv bfcpsrvclt/bfcpsrv/bfcp_server.cpp bfcpsrvclt/bfcpsrv/bfcp_link_list.cpp bfcpsrvclt/bfcpsrv/bfcp_floor_list.cpp bfcpsrvclt/bfcpsrv/bfcp_user_list.cpp)
target_link_libraries(bfcp_srv bfcpmsg pthread)

add_executable(bfcp_server bfcpsrvclt/bfcpsrv/main.cpp)
target_link_libraries(bfcp_server bfcp_srv pthread)

add_executable(bfcp_pal main.cpp bfcpal.cpp)
target_link_libraries(bfcp_pal bfcp_cli)