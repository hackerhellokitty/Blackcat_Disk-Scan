cmake_minimum_required(VERSION 3.20)
project(BlackCatDiskScanner VERSION 4.2.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# libharu via FetchContent
# ---------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    libharu
    GIT_REPOSITORY https://github.com/libharu/libharu.git
    GIT_TAG        master
)

# Force static build — no DLL dependency
set(BUILD_SHARED_LIBS        OFF CACHE BOOL "" FORCE)
set(LIBHPDF_STATIC           ON  CACHE BOOL "" FORCE)
set(LIBHPDF_EXAMPLES         OFF CACHE BOOL "" FORCE)
set(LIBHPDF_TESTS            OFF CACHE BOOL "" FORCE)
set(LIBHPDF_ENABLE_EXCEPTIONS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(libharu)

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
set(SOURCES
    src/main.c
    src/admin.c
    src/disk.c
    src/eventlog.c
    src/i18n.c
    src/pdf.c
    src/ui.c
    src/utils.c
    src/app.rc
)

add_executable(blackcat_disk_scanner WIN32 ${SOURCES})

# ---------------------------------------------------------------------------
# Include paths
# ---------------------------------------------------------------------------
target_include_directories(blackcat_disk_scanner PRIVATE
    src/
    ${libharu_SOURCE_DIR}/include
    ${libharu_BINARY_DIR}/include
)

# ---------------------------------------------------------------------------
# Link libraries
# ---------------------------------------------------------------------------
target_link_libraries(blackcat_disk_scanner PRIVATE
    hpdf
    kernel32
    user32
    shell32
    advapi32
    comctl32
    comdlg32
    gdi32
)

# ---------------------------------------------------------------------------
# Compiler definitions
# ---------------------------------------------------------------------------
target_compile_definitions(blackcat_disk_scanner PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
    NOMINMAX
)

# ---------------------------------------------------------------------------
# MSVC-specific flags
# ---------------------------------------------------------------------------
if(MSVC)
    target_compile_options(blackcat_disk_scanner PRIVATE
        /W3
        /wd4100   # unreferenced formal parameter
        /wd4996   # deprecated CRT functions
        /utf-8
    )

    # UAC manifest: require administrator
    set_target_properties(blackcat_disk_scanner PROPERTIES
        LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\""
    )
endif()

# ---------------------------------------------------------------------------
# Copy assets to output
# ---------------------------------------------------------------------------
if(EXISTS "${CMAKE_SOURCE_DIR}/assets/ssd.ico")
    configure_file(
        ${CMAKE_SOURCE_DIR}/assets/ssd.ico
        ${CMAKE_BINARY_DIR}/ssd.ico
        COPYONLY
    )
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install(TARGETS blackcat_disk_scanner
    RUNTIME DESTINATION bin
)
