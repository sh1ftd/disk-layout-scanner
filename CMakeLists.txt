cmake_minimum_required(VERSION 3.10)
project(disk-layout-scanner C)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(COMMON_SOURCES
    src/main.c
    src/core/common.c
    src/output/text_output.c
    src/output/json_output.c
    src/output/html_output.c
)

if(WIN32)
    list(APPEND COMMON_SOURCES src/queries/queries_win.c)
else()
    list(APPEND COMMON_SOURCES src/queries/queries_linux.c)
endif()

file(READ "${CMAKE_SOURCE_DIR}/VERSION" DISK_LAYOUT_SCANNER_VERSION_RAW)
string(STRIP "${DISK_LAYOUT_SCANNER_VERSION_RAW}" DISK_LAYOUT_SCANNER_VERSION_STR)

add_executable(disk-layout-scanner ${COMMON_SOURCES})
target_include_directories(disk-layout-scanner PRIVATE src/core)
target_compile_definitions(disk-layout-scanner PRIVATE "DISK_LAYOUT_SCANNER_VERSION=\"${DISK_LAYOUT_SCANNER_VERSION_STR}\"")

install(TARGETS disk-layout-scanner RUNTIME DESTINATION bin)

if(WIN32)
    target_link_libraries(disk-layout-scanner kernel32 setupapi shell32)
    target_compile_definitions(disk-layout-scanner PRIVATE
        _CRT_SECURE_NO_WARNINGS
        UNICODE
    )
endif()

if(MSVC)
    target_compile_options(disk-layout-scanner PRIVATE
        /W4 /O2 /GL /GS /sdl /guard:cf /Gy
    )
    target_link_options(disk-layout-scanner PRIVATE
        /LTCG /OPT:REF /OPT:ICF /GUARD:CF /DYNAMICBASE /NXCOMPAT
    )
else()
    target_compile_options(disk-layout-scanner PRIVATE
        -Wall -Wextra -Wno-unused-parameter
        -O2 -flto -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE
        -Wformat -Wformat-security
    )
    target_link_options(disk-layout-scanner PRIVATE
        -flto -s -pie -Wl,-z,relro,-z,now -Wl,-z,noexecstack
    )
endif()
