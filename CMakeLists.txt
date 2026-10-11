cmake_minimum_required(VERSION 3.22)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal macOS build")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(GRID_ARVx VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GRID_BUILD_APP "Build standalone application" ON)
option(GRID_BUILD_PLUGINS "Build DAW plugins" ON)
option(GRID_ENABLE_CLAP "Build CLAP using the pinned JUCE adapter" ON)
option(GRID_ENABLE_VST2 "Reserved for an authorized legacy implementation" OFF)
option(GRID_ENABLE_AAX "Reserved for an authorized AAX implementation" OFF)
option(GRID_CORE_TESTS_ONLY "Test the audio core without downloading JUCE" OFF)
if(GRID_ENABLE_VST2 OR GRID_ENABLE_AAX)
    message(FATAL_ERROR "VST2/AAX are reserved options, not implemented in this foundation. Leave them OFF. SDK authorization and wrapper work are required.")
endif()
include(CTest)
if(BUILD_TESTING)
    add_executable(grid_audio_tests tests/audio_tests.cpp)
    target_include_directories(grid_audio_tests PRIVATE Source)
    find_package(Threads REQUIRED)
    target_link_libraries(grid_audio_tests PRIVATE Threads::Threads)
    add_test(NAME audio_core COMMAND grid_audio_tests)
endif()
if(GRID_CORE_TESTS_ONLY)
    return()
endif()

include(FetchContent)
# Complete commits keep dependencies stable. Nothing needs to be preinstalled in the repo.
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 51d11a2be6d5c97ccf12b4e5e827006e19f0555a)
FetchContent_MakeAvailable(JUCE)
set(GRID_SOURCES Source/Plugin/Processor.cpp Source/Plugin/Editor.cpp Source/Visual/Canvas.cpp)
function(grid_configure target)
    target_sources(${target} PRIVATE ${GRID_SOURCES})
    target_include_directories(${target} PRIVATE Source)
    target_compile_definitions(${target} PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(${target} PRIVATE juce::juce_audio_utils juce::juce_opengl
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endfunction()

if(GRID_BUILD_PLUGINS)
    set(GRID_FORMATS VST3)
    if(APPLE)
        list(APPEND GRID_FORMATS AU)
    endif()
    juce_add_plugin(GridARVxPlugin
        COMPANY_NAME "Kone Labie" BUNDLE_ID "com.konelabie.gridarvx.plugin"
        PLUGIN_MANUFACTURER_CODE KnLb PLUGIN_CODE GAvx
        PRODUCT_NAME "GRID-ARVx" FORMATS ${GRID_FORMATS}
        IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx Analyzer AU_MAIN_TYPE kAudioUnitType_Effect)
    grid_configure(GridARVxPlugin)
    if(GRID_ENABLE_CLAP)
        set(CLAP_JUCE_EXTENSIONS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(clap_juce_extensions
            GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
            GIT_TAG 7adee3a1bd4684d4caa5601100e364abccff4b4d
            GIT_SUBMODULES_RECURSE TRUE)
        FetchContent_MakeAvailable(clap_juce_extensions)
        clap_juce_extensions_plugin(TARGET GridARVxPlugin
            CLAP_ID "com.konelabie.gridarvx" CLAP_FEATURES audio-effect analyzer
            CLAP_USE_JUCE_PARAMETER_RANGES ALL)
    endif()
endif()
if(GRID_BUILD_APP)
    juce_add_gui_app(GridARVxApp COMPANY_NAME "Kone Labie" PRODUCT_NAME "GRID-ARVx"
        BUNDLE_ID "com.konelabie.gridarvx.app"
        MICROPHONE_PERMISSION_ENABLED TRUE
        MICROPHONE_PERMISSION_TEXT "GRID-ARVx uses your chosen audio input to animate visuals.")
    grid_configure(GridARVxApp)
    target_sources(GridARVxApp PRIVATE Source/App/Main.cpp)
    target_compile_definitions(GridARVxApp PRIVATE GRID_STANDALONE=1)
endif()
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/grid-dependencies.json"
    CONTENT "{\"juce\":\"${juce_SOURCE_DIR}\",\"clap\":\"${clap_juce_extensions_SOURCE_DIR}\"}\n")
