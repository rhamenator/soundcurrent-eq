cmake_minimum_required(VERSION 3.20)
project(soundcurrent-eq VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets)

add_executable(soundcurrent-eq src/main.cpp)
target_link_libraries(soundcurrent-eq PRIVATE Qt6::Core Qt6::Widgets)
target_compile_options(soundcurrent-eq PRIVATE -Wall -Wextra -Wpedantic)

include(GNUInstallDirs)
install(TARGETS soundcurrent-eq RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/io.github.rhamenator.SoundCurrentEQ.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/io.github.rhamenator.SoundCurrentEQ.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
