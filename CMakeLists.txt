# -- build and run ctest windows
#mkdir build; cd build; conan install .. --build=missing -s build_type=Debug ; cmake -DCMAKE_BUILD_TYPE=Debug ..; cmake --build .; ctest;
# -- run in windows
#.\Debug\game_server.exe -c ..\data\config.json -w ..\static\ -t 100 -p 1000 -s ..\data\state
# -- visual studio debugger args
# -c $(ProjectDir)\..\data\config.json -w $(ProjectDir)\..\static\ -t 100 -p 1000 -s $(ProjectDir)\..\data\state
cmake_minimum_required(VERSION 3.11)

#PROJECT BEGIN
project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
add_compile_definitions(BOOST_USE_WINAPI_VERSION=0x0601)
add_compile_definitions(BOOST_BEAST_USE_STD_STRING_VIEW)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)
#PROJECT END

#LIB BEGIN
add_library(game_server_lib STATIC
	src/model.h
        src/model.cpp
        src/loot_generator.h
        src/loot_generator.cpp
        src/tagged.h
	src/geom.h
	src/collision_detector.h
	src/collision_detector.cpp
	src/application.h
	src/application.cpp
	src/model_serialization.h
	src/postgres.h
	src/postgres.cpp
)
target_link_libraries(game_server_lib PUBLIC CONAN_PKG::boost Threads::Threads CONAN_PKG::libpq CONAN_PKG::libpqxx)
#LIB END

#SERVER BEGIN
add_executable(game_server
	src/logger.h
	src/logger.cpp
	src/main.cpp
	src/http_server.cpp
	src/http_server.h
	src/sdk.h
	src/boost_json.cpp
	src/json_loader.h
	src/json_loader.cpp
	src/request_handler.h
	src/response_utils.h
	src/response_utils.cpp
	src/request_file.h
	src/request_file.cpp
	src/tagged_uuid.h
	src/tagged_uuid.cpp
	src/request_api.h
	src/extra_data.h
	src/extra_data.cpp
	src/ticker.h
)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(game_server PRIVATE Threads::Threads game_server_lib)
#SERVER END

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
	#TESTS BEGIN
        add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=0)
	add_executable(game_server_tests
	    tests/loot_generator_tests.cpp
		tests/collision-detector-tests.cpp
        tests/state-serialization-tests.cpp
	)
	target_link_libraries(game_server_tests PRIVATE CONAN_PKG::catch2 Threads::Threads game_server_lib)
	include(CTest)
	include(${CONAN_BUILD_DIRS_CATCH2_DEBUG}/Catch.cmake)
	catch_discover_tests(game_server_tests)
	#TESTS END
endif()#Debug
