cmake_minimum_required(VERSION 3.10)
project(TrainTicketsAvailProvider)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add executable
add_executable(TrainTicketsAvailProvider main.cpp)

add_definitions(-DMAILIO_EXPORT=)

# Include local nlohmann/json
target_include_directories(TrainTicketsAvailProvider PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Link htmlcxx manually
target_include_directories(TrainTicketsAvailProvider PRIVATE /usr/include/htmlcxx)

target_link_libraries(TrainTicketsAvailProvider PRIVATE ${CMAKE_SOURCE_DIR}/libs/libhtmlcxx.so)
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