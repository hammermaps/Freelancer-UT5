cmake_minimum_required(VERSION 3.16)
project(fltool CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Die Formatbibliothek liegt als UE-Modul im Projekt und wird hier mitgebaut (eine Quelle fuer fltool und FLImport).
set(FORMATS ${CMAKE_CURRENT_SOURCE_DIR}/../../Source/FreelancerFormats)
add_library(flformats STATIC
	${FORMATS}/Private/Utf.cpp ${FORMATS}/Private/Ini.cpp ${FORMATS}/Private/Vms.cpp
	${FORMATS}/Private/Model.cpp ${FORMATS}/Private/ShipData.cpp)
target_include_directories(flformats PUBLIC ${FORMATS}/Public PRIVATE ${FORMATS}/Private)

add_executable(fltool src/main.cpp)
target_link_libraries(fltool flformats)

enable_testing()
add_executable(fltool_tests tests/test_main.cpp)
target_link_libraries(fltool_tests flformats)
add_test(NAME fltool_tests COMMAND fltool_tests)
