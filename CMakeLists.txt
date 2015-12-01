
PROJECT(rescue)
CMAKE_MINIMUM_REQUIRED(VERSION 2.8 FATAL_ERROR)
GET_FILENAME_COMPONENT(PROJECT_ROOT . ABSOLUTE)
GET_FILENAME_COMPONENT(BUILD_ROOT ${CMAKE_CURRENT_BINARY_DIR} ABSOLUTE)
SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})

ADD_EXECUTABLE(bootstrap src/rescue.c src/deflate.c)
target_compile_definitions(bootstrap PUBLIC -DRESCUE_BOOTSTRAP="${PROJECT_ROOT}/src/")

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/resources.c
                   COMMAND bootstrap ARGS -o ${CMAKE_CURRENT_BINARY_DIR}/resources.c ${PROJECT_ROOT}/src/inflate.c ${PROJECT_ROOT}/src/template.c
                   DEPENDS bootstrap ${PROJECT_ROOT}/src/inflate.c ${PROJECT_ROOT}/src/template.c
                   WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                   COMMENT "Generating ${CMAKE_CURRENT_BINARY_DIR}/resources.c file")

ADD_EXECUTABLE(rescue src/rescue.c src/deflate.c ${CMAKE_CURRENT_BINARY_DIR}/resources.c)
target_include_directories(rescue PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

INSTALL(TARGETS rescue RUNTIME DESTINATION bin)
