cmake_minimum_required(VERSION 3.0)
project(SimpleClient)

set(CLIENT_FILES ftpclient.c)

add_executable(ftpclient ${CLIENT_FILES})

# Указываем, что нужно линковаться с библиотекой Ws2_32
target_link_libraries(ftpclient Ws2_32)