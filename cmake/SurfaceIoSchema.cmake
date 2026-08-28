get_filename_component(SURFACE_IO_SCHEMA_FILE "${CMAKE_CURRENT_LIST_DIR}/../Scripts/surface_io_schema.conf" ABSOLUTE)
if(NOT EXISTS "${SURFACE_IO_SCHEMA_FILE}")
  message(FATAL_ERROR "Surface I/O schema does not exist: ${SURFACE_IO_SCHEMA_FILE}")
endif()

file(READ "${SURFACE_IO_SCHEMA_FILE}" SURFACE_IO_SCHEMA_SOURCE)
