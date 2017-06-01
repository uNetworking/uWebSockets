# Configure the uWS library.

# Determine the sources.
file(GLOB SOURCES "*.cpp")
file(GLOB HEADERS "*.h")

# Set the include directories.
# include_directories(${GradeScores_SOURCE_DIR}/library)

# Create the shared library.
add_library(uWSShared STATIC ${SOURCES} ${HEADERS})
set_target_properties(uWSShared PROPERTIES OUTPUT_NAME   uWS)
set_target_properties(uWSShared PROPERTIES PUBLIC_HEADER "${HEADERS}")

# Set the shared library installation path.
install(TARGETS uWSShared RUNTIME       DESTINATION bin
                          LIBRARY       DESTINATION lib
                          ARCHIVE       DESTINATION lib
                          PUBLIC_HEADER DESTINATION include/uWS)

# Create the static library
add_library(uWSStatic SHARED ${SOURCES} ${HEADERS})
set_target_properties(uWSStatic PROPERTIES OUTPUT_NAME   uWS)
set_target_properties(uWSStatic PROPERTIES PUBLIC_HEADER "${HEADERS}")

# Set the static library installation path.
install(TARGETS uWSStatic RUNTIME       DESTINATION bin
                          LIBRARY       DESTINATION lib
                          ARCHIVE       DESTINATION lib
                          PUBLIC_HEADER DESTINATION include/uWS)
