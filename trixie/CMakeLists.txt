cmake_minimum_required(VERSION 3.20)
project(trixie VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# GLFW — window creation and OS input events
# Still fetched: it's actively maintained and has no vendoring issues.
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# glad — OpenGL 3.3 core function loader
# Vendored: pre-generated with glad v2.0.6, gl:core=3.3, reproducible.
# Regenerate with: python -m glad --out-path external/glad --api gl:core=3.3 --reproducible c
add_library(glad_gl_core_33 STATIC external/glad/src/gl.c)
target_include_directories(glad_gl_core_33 PUBLIC
    external/glad/include
    external/glad/include/KHR
)

find_package(OpenGL REQUIRED)

# NanoVG — vendored in external/nanovg/
# Unmaintained upstream; we own this copy.
add_library(nanovg STATIC external/nanovg/nanovg.c)
target_include_directories(nanovg PUBLIC external/nanovg)

# midifile — vendored in external/midifile/
# Unmaintained upstream; we own this copy.
add_library(midifile STATIC
    external/midifile/src/MidiFile.cpp
    external/midifile/src/MidiEvent.cpp
    external/midifile/src/MidiEventList.cpp
    external/midifile/src/MidiMessage.cpp
    external/midifile/src/Binasc.cpp
    external/midifile/src/Options.cpp
)
target_include_directories(midifile PUBLIC external/midifile/include)

add_executable(trixie
    src/main.cpp
    src/window.cpp
    src/render.cpp
    src/journal.cpp
    src/midi_loader.cpp
    src/piano_roll.cpp
    src/input_queue.cpp
    src/playback_engine.cpp
    src/midi_out_plugin.cpp
    src/toolbar.cpp
)

target_include_directories(trixie PRIVATE src)

# GLFW_INCLUDE_NONE: glad owns all OpenGL declarations — GLFW must not pull its own.
target_compile_definitions(trixie PRIVATE
    GLFW_INCLUDE_NONE
    TRIXIE_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
)

target_link_libraries(trixie PRIVATE
    glfw
    glad_gl_core_33
    OpenGL::GL
    nanovg
    midifile
    winmm  # Windows MIDI output (MidiOutPlugin)
)
