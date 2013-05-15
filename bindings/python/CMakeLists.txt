find_package(Boost REQUIRED COMPONENTS python)
#find_package(PythonLibs REQUIRED)
#find_package(PythonInterp REQUIRED)
#find_package(PythonExtensionDir REQUIRED)
#message(STATUS "Python includes are situated in (${PYTHON_INCLUDE_PATH}, ${PYTHON_INCLUDE_DIRS})")

INCLUDE_DIRECTORIES(BEFORE ${PROJECT_SOURCE_DIR}/include)
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

find_package(PythonLibs REQUIRED)
include_directories(${PYTHON_INCLUDE_PATH})
include_directories(${PYTHON_INCLUDE_DIRS})
link_directories(${PYTHON_LIBRARIES})

add_library(proxy SHARED proxy_python.cpp)
target_link_libraries(proxy ${Boost_LIBRARIES} ${PYTHON_LIBRARIES} elliptics-proxy)

set_target_properties(proxy PROPERTIES
    PREFIX ""
    OUTPUT_NAME "elliptics_proxy"
    VERSION ${MAJOR_VERSION}
    SOVERSION ${MINOR_VERSION}
    )

#install(TARGETS libelliptics_python
#    LIBRARY DESTINATION ${PYTHON_EXTENSION_DIR}
#    ARCHIVE DESTINATION ${PYTHON_EXTENSION_DIR}
#    BUNDLE DESTINATION library
#    )

#install(CODE "EXECUTE_PROCESS(
#	COMMAND ${PYTHON_EXECUTABLE} setup.py install --prefix=${CMAKE_INSTALL_PREFIX}
#	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}python_binding//
#)")
