cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(Airgorithm)

file(COPY data DESTINATION ${CMAKE_BINARY_DIR})

# CLI
#add_executable(Airgorithm
#        main.cpp
#)

# SFML Frontend
set(SFML_DIR "C:/Users/arian/Downloads/SFML/SFML-2.5.1/lib/cmake/SFML")

find_package(SFML 2.5.1 COMPONENTS system window graphics audio REQUIRED)

add_executable(Airgorithm
        frontend.cpp
)

 target_link_libraries(Airgorithm sfml-system sfml-window sfml-graphics sfml-audio)