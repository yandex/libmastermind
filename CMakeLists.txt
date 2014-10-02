cmake_minimum_required( VERSION 2.6 ) 
 
set(LIB mastermind)
project(${LIB})
 
FILE (READ "${CMAKE_CURRENT_SOURCE_DIR}/debian/changelog" DEBCHANGELOG)

string(REGEX MATCH "([0-9]+\\.[0-9]+)" DEBFULLVERSION "${DEBCHANGELOG}")
string(REGEX MATCH "([0-9]+)" MAJOR_VERSION "${DEBFULLVERSION}")
string (REGEX MATCH "([0-9]+$)" MINOR_VERSION "${DEBFULLVERSION}")

set(CMAKE_CXX_FLAGS "-Wall -pedantic -std=c++0x -O2 -g")

set(REQUIRED_LIBRARIES
	cocaine-core
	cocaine-framework
	boost_thread
	boost_system
	jsoncpp
	)
	
include_directories(BEFORE ${PROJECT_SOURCE_DIR}/include)
 
aux_source_directory(src SOURCES)
set(SOURCES
	${SOURCES}
	${PROJECT_SOURCE_DIR}/src/utils.hpp
	)

add_library(${LIB} SHARED ${SOURCES})
 
set_target_properties(${LIB} PROPERTIES 
	VERSION "${MAJOR_VERSION}.${MINOR_VERSION}"
	SOVERSION ${MAJOR_VERSION}
	)
 
target_link_libraries(${LIB}
	${REQUIRED_LIBRARIES})

#add_subdirectory(bindings)

install(TARGETS ${LIB}
	RUNTIME DESTINATION bin COMPONENT runtime
	LIBRARY DESTINATION lib COMPONENT runtime
	ARCHIVE DESTINATION lib COMPONENT developement
	)

install(
    DIRECTORY
        include/
    DESTINATION include
    COMPONENT development)

#set (TESTS_SOURCES
#	tests/test.cpp
#	tests/teamcity_cppunit.cpp
#	tests/teamcity_messages.cpp
#	tests/teamcity_cppunit.h
#	tests/teamcity_messages.h
#)
#set (TEST test_${LIB})
#add_executable (${TEST} ${TESTS_SOURCES})
#target_link_libraries (${TEST} ${LIB} cppunit)
##set_target_properties(${TEST} PROPERTIES
#        BUILD_WITH_INSTALL_RPATH 1
#        INSTALL_RPATH .)
#set_target_properties(${TEST} PROPERTIES
#        INSTALL_RPATH "."
#        BUILD_WITH_INSTALL_RPATH ON)
#enable_testing ()
#add_test (${TEST} ${TEST} ${PROJECT_SOURCE_DIR}/tests/)
#set(PYTEST pytest_runner.sh)
#add_test (${PYTEST} ${PYTEST} ${PROJECT_SOURCE_DIR}/tests/)
#file(COPY
#	${CMAKE_CURRENT_SOURCE_DIR}/tests/${PYTEST}
#	DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)
