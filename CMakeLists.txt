cmake_minimum_required( VERSION 2.6 ) 
 
set( LIB elliptics-proxy) 
project( ${LIB} ) 
 
set( MAJOR_VERSION 0.1.0 )
set( MINOR_VERSION 0.1 )
 
set( SOURCE_ROOT src )
set( HEADERS include )
 
aux_source_directory( ${SOURCE_ROOT} SOURCES )
include_directories( "${HEADERS}"
                     "${SOURCE_ROOT}")
 
#set( LIB_INSTALL_PATH /usr/local/lib )
#set( HEADERS_INSTALL_PATH /usr/local/include/my_lib )
#link_directories( "${LIB_INSTALL_PATH}" )
 
set( REQUIRED_LIBRARIES 
		elliptics_cpp
		curl
		boost_thread 
		boost_system )
 
set( CMAKE_CXX_FLAGS "-O0 -Wall -pedantic" )

INCLUDE_DIRECTORIES(BEFORE ${PROJECT_SOURCE_DIR}/include)
 
add_library( ${LIB} SHARED ${SOURCES} )
target_link_libraries( ${LIB} ${REQUIRED_LIBRARIES} )
 
set_target_properties( ${LIB} PROPERTIES 
                              VERSION ${MAJOR_VERSION} 
                              SOVERSION ${MINOR_VERSION} )
 
#install( TARGETS ${LIB} DESTINATION ${LIB_INSTALL_PATH} )
#install( DIRECTORY ${HEADERS}/
#         DESTINATION ${HEADERS_INSTALL_PATH}
#         FILES_MATCHING PATTERN "*.hpp" )
install( TARGETS ${LIB}
    RUNTIME DESTINATION bin COMPONENT runtime
    LIBRARY DESTINATION lib COMPONENT runtime
    ARCHIVE DESTINATION lib COMPONENT developement)

INSTALL(
    DIRECTORY
        include/
    DESTINATION include
    COMPONENT development)

set (TESTS_SOURCES
	tests/test.cpp)
set (TEST test_${PROJECT})
add_executable (${TEST} ${TESTS_SOURCES})
target_link_libraries (${TEST} ${LIB})
enable_testing ()
add_test (${TEST} ${TEST})
