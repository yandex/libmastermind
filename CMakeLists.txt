cmake_minimum_required( VERSION 2.6 ) 
 
set( LIB elliptics-proxy) 
project( ${LIB} ) 
 
FILE (READ "${CMAKE_CURRENT_SOURCE_DIR}/debian/changelog" DEBCHANGELOG)

string(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+)" DEBFULLVERSION "${DEBCHANGELOG}")
STRING (REGEX MATCH "([0-9]+\\.[0-9]+)" MAJOR_VERSION "${DEBFULLVERSION}")
STRING (REGEX MATCH "([0-9]+$)" MINOR_VERSION "${DEBFULLVERSION}")
 
option(WITH_METABASE "Build with metabase and metabalancer support" ON)
SET(HAVE_METABASE false)

SET(CMAKE_CXX_FLAGS "-O0 -Wall -pedantic -std=c++0x")

find_package(Boost COMPONENTS system thread REQUIRED)

set(REQUIRED_LIBRARIES
                      elliptics_cpp
                      curl
                      ${Boost_SYSTEM_LIBRARY}
                      ${Boost_THREAD_LIBRARY})
IF(WITH_METABASE)
    SET(HAVE_METABASE true)
    FIND_LIBRARY(DEALER NAMES cocaine-dealer)

    IF(${DEALER} STREQUAL "DEALER-NOTFOUND")
        SET(HAVE_METABASE false)
    ENDIF()

    IF(HAVE_METABASE)
        SET(REQUIRED_LIBRARIES 
			${REQUIRED_LIBRARIES}
			${DEALER})
    ENDIF()
message("Metabase: ${HAVE_METABASE} ${REQUIRED_LIBRARIES}")
ENDIF()

CONFIGURE_FILE(
	"${PROJECT_SOURCE_DIR}/include/elliptics/proxy.hpp"
	"${PROJECT_SOURCE_DIR}/include/elliptics/proxy.hpp")
	
INCLUDE_DIRECTORIES(BEFORE ${PROJECT_SOURCE_DIR}/include)
 
AUX_SOURCE_DIRECTORY(src SOURCES)
set(SOURCES
	${SOURCES}
	${PROJECT_SOURCE_DIR}/src/utils.hpp)

ADD_LIBRARY(${LIB} SHARED ${SOURCES})
 
SET_TARGET_PROPERTIES(${LIB} PROPERTIES 
                             VERSION "${MAJOR_VERSION}.${MINOR_VERSION}"
                             SOVERSION ${MAJOR_VERSION})
 
TARGET_LINK_LIBRARIES(${LIB}
                      ${REQUIRED_LIBRARIES})

INSTALL(TARGETS ${LIB}
            RUNTIME DESTINATION bin COMPONENT runtime
            LIBRARY DESTINATION lib COMPONENT runtime
            ARCHIVE DESTINATION lib COMPONENT developement)

INSTALL(
    DIRECTORY
        include/
    DESTINATION include
    COMPONENT development)

set (TESTS_SOURCES
	tests/test.cpp
	tests/teamcity_cppunit.cpp
	tests/teamcity_messages.cpp
	tests/teamcity_cppunit.h
	tests/teamcity_messages.h
)
set (TEST test_${LIB})
add_executable (${TEST} ${TESTS_SOURCES})
target_link_libraries (${TEST} ${LIB} cppunit)
#set_target_properties(${TEST} PROPERTIES
#        BUILD_WITH_INSTALL_RPATH 1
#        INSTALL_RPATH .)
set_target_properties(${TEST} PROPERTIES
        INSTALL_RPATH "."
        BUILD_WITH_INSTALL_RPATH ON)
enable_testing ()
add_test (${TEST} ${TEST} ${PROJECT_SOURCE_DIR}/tests/)
