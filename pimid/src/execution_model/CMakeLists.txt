# Execution Model Library

set(EXECUTION_MODEL_SOURCES
    execution_model_factory.cpp
    zsim_execution_model.cpp
    event_driven_execution_model.cpp
)

set(EXECUTION_MODEL_HEADERS
    ${CMAKE_SOURCE_DIR}/pimid/include/execution_model/execution_model.h
    ${CMAKE_SOURCE_DIR}/pimid/include/execution_model/zsim_execution_model.h
    ${CMAKE_SOURCE_DIR}/pimid/include/execution_model/event_driven_execution_model.h
)

add_library(execution_model STATIC
    ${EXECUTION_MODEL_SOURCES}
    ${EXECUTION_MODEL_HEADERS}
)

target_include_directories(execution_model PUBLIC
    ${CMAKE_SOURCE_DIR}/pimid/include
    ${CMAKE_SOURCE_DIR}/pimid/external/zsim/src
)

target_link_libraries(execution_model PUBLIC
    common
    memory_models
    plugin
)

# Install targets
install(TARGETS execution_model
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES ${EXECUTION_MODEL_HEADERS}
    DESTINATION include/execution_model
)
