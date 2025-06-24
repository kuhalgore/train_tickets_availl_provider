cmake_minimum_required(VERSION 3.10)
project(TrainTicketsAvailProvider)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(Boost REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)

add_executable(TrainTicketsAvailProvider main.cpp)
target_compile_definitions(TrainTicketsAvailProvider PRIVATE CROW_USE_BOOST)
add_definitions(-DMAILIO_EXPORT=)

# Includes
target_include_directories(TrainTicketsAvailProvider PRIVATE
    ${CMAKE_SOURCE_DIR}/include        # nlohmann/json
    ${CMAKE_SOURCE_DIR}/third_party/mailio/include
)

# Gumbo
add_library(gumbo STATIC
    third_party/gumbo/attribute.c
    third_party/gumbo/parser.c
    third_party/gumbo/string_buffer.c
    third_party/gumbo/string_piece.c
    third_party/gumbo/tag.c
    third_party/gumbo/tokenizer.c
    third_party/gumbo/utf8.c
    third_party/gumbo/util.c
    third_party/gumbo/error.c
    third_party/gumbo/char_ref.c
    third_party/gumbo/vector.c
)
target_include_directories(gumbo PUBLIC third_party/gumbo)
target_link_libraries(TrainTicketsAvailProvider PRIVATE gumbo)

# Attempt to find installed Mailio; fallback to subdirectory
find_library(MAILIO_LIB mailio PATHS /usr/local/lib)
if (MAILIO_LIB AND EXISTS "/usr/local/include/mailio/message.hpp")
    message(STATUS "Using system-installed Mailio")
    target_include_directories(TrainTicketsAvailProvider PRIVATE /usr/local/include)
    target_link_libraries(TrainTicketsAvailProvider PRIVATE ${MAILIO_LIB})
else()
    message(STATUS "Using fallback Mailio from third_party")
    add_subdirectory(third_party/mailio)
    target_link_libraries(TrainTicketsAvailProvider PRIVATE mailio)
endif()

# Link remaining dependencies
target_link_libraries(TrainTicketsAvailProvider PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    CURL::libcurl
    Boost::system
)
