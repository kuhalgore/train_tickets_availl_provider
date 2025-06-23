cmake_minimum_required(VERSION 3.10)
project(TrainTicketsAvailProvider)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)


# Add executable
add_executable(TrainTicketsAvailProvider main.cpp)
add_definitions(-DMAILIO_EXPORT=)

add_library(gumbo STATIC
    third_party/gumbo/attribute.c
    third_party/gumbo/parser.c
    third_party/gumbo/string_buffer.c
    third_party/gumbo/string_piece.c
    third_party/gumbo/tag.c
    third_party/gumbo/tokenizer.c
    third_party/gumbo/utf8.c
    third_party/gumbo/util.c
    third_party/gumbo/error.c           # ← Add this
    third_party/gumbo/char_ref.c        # ← And this
    third_party/gumbo/vector.c 

)
target_include_directories(gumbo PUBLIC third_party/gumbo)

# Include local nlohmann/json
target_include_directories(TrainTicketsAvailProvider PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(TrainTicketsAvailProvider PRIVATE gumbo)


target_link_libraries(TrainTicketsAvailProvider PRIVATE ${CMAKE_SOURCE_DIR}/libs/libcurl.so)
target_link_libraries(TrainTicketsAvailProvider PRIVATE ${CMAKE_SOURCE_DIR}/libs/libmailio.so)
target_link_libraries(TrainTicketsAvailProvider PRIVATE ${CMAKE_SOURCE_DIR}/libs/libcrypto.so)

# Tell the linker where to find .so files
link_directories(${CMAKE_SOURCE_DIR}/libs)


# Embed RPATH relative to the executable
set_target_properties(TrainTicketsAvailProvider PROPERTIES
    BUILD_RPATH "$ORIGIN/libs"
    INSTALL_RPATH "$ORIGIN/libs"
)