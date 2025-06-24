add_library(mailio STATIC
 src/base64.cpp
 src/binary.cpp
 src/bit7.cpp
 src/bit8.cpp
 src/codec.cpp
 src/dialog.cpp
 src/imap.cpp
 src/mailboxes.cpp
 src/mime.cpp
 src/message.cpp
 src/pop3.cpp
 src/percent.cpp
 src/quoted_printable.cpp
 src/q_codec.cpp
 src/smtp.cpp
)
find_package(Boost REQUIRED COMPONENTS system)

target_include_directories(mailio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

target_link_libraries(mailio PUBLIC OpenSSL::SSL OpenSSL::Crypto CURL::libcurl Boost::system)
