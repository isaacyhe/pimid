# Trace Library

set(TRACE_SOURCES
    trace_writer.cpp
    trace_reader.cpp
)

set(TRACE_HEADERS
    ${CMAKE_SOURCE_DIR}/include/trace/trace_format.h
    ${CMAKE_SOURCE_DIR}/include/trace/trace_writer.h
    ${CMAKE_SOURCE_DIR}/include/trace/trace_reader.h
)

add_library(trace STATIC
    ${TRACE_SOURCES}
    ${TRACE_HEADERS}
)

target_include_directories(trace PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(trace PUBLIC
    common
)

# Install targets
install(TARGETS trace
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${TRACE_HEADERS}
    DESTINATION include/trace
)
