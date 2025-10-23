cmake_minimum_required(VERSION 3.10)

project("DasherPPMReWrite")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_definitions(HAVE_ROUND)

add_subdirectory("DasherCore")

ADD_Executable(DasherPPM
	${PROJECT_SOURCE_DIR}/src/NewPPM.cpp
	${PROJECT_SOURCE_DIR}/src/main.cpp
)
add_dependencies(DasherPPM DasherCore)
target_link_libraries(DasherPPM PRIVATE DasherCore)
