file(GLOB_RECURSE sources CONFIGURE_DEPENDS ./*.c* ./*.h*)

if(WIN32)
    list(APPEND sources "${CMAKE_CURRENT_SOURCE_DIR}/res.rc") # otherwise any window content will be empty on windows
endif()

target_sources(${PROJECT_NAME}
    PRIVATE
    ${sources}
)