cmake_minimum_required(VERSION 2.8)

project (BurnerFirmware)

SET(CMAKE_SYSTEM_NAME Linux)  

include_directories( /usr/local/include )
link_directories( /usr/local/lib/ )

include_directories( app/include/ )
file(GLOB headers "app/include/*.h" )
file(GLOB sources "app/src/*.c" )

link_directories( ${CMAKE_CURRENT_SOURCE_DIR}/lib/ )

add_executable(burner-firmware ${sources} ${headers})

target_link_libraries( burner-firmware pthread utils orcania iniparser )
