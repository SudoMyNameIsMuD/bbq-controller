cmake_minimum_required(VERSION 3.10)

project(bbq_tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_BUILD_TYPE Debug)

# Prepare "Catch" library for other executables
set(CATCH_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
add_library(Catch INTERFACE)
target_include_directories(Catch INTERFACE ${CATCH_INCLUDE_DIR})

ADD_DEFINITIONS(-DUNIT_TEST)

# Host tests for the platform-independent controller core (no Arduino or
# library dependencies).
add_executable(coretests
    coretests.cpp
    ../lib/bbqcore/probechannel.cpp
    ../lib/bbqcore/controllercore.cpp
    ../lib/bbqcore/pidalgorithm.cpp
    ../lib/bbqcore/persistedstate.cpp
)
target_include_directories(coretests PRIVATE ../lib/bbqcore)
target_link_libraries(coretests Catch)

# Legacy tests for the retained lib/bbq code (fuzzy controller etc.). These
# need the eFLL and opt-parser sources; point LEGACY_LIBDEPS at a directory
# containing them (e.g. a PlatformIO libdeps folder) to enable.
set(LEGACY_LIBDEPS "" CACHE PATH "Directory containing eFLL and rvt-OptionParser sources")

if(LEGACY_LIBDEPS)
    ADD_DEFINITIONS(-DESP8266)

    set(LIB_SOURCES
        ${LEGACY_LIBDEPS}/eFLL/FuzzyComposition.cpp
        ${LEGACY_LIBDEPS}/eFLL/FuzzyIO.cpp
        ${LEGACY_LIBDEPS}/eFLL/Fuzzy.cpp
        ${LEGACY_LIBDEPS}/eFLL/FuzzyInput.cpp
        ${LEGACY_LIBDEPS}/eFLL/FuzzyOutput.cpp
        ${LEGACY_LIBDEPS}/eFLL/FuzzyRule.cpp
        ${LEGACY_LIBDEPS}/eFLL/FuzzyRuleAntecedent.cpp
        ${LEGACY_LIBDEPS}/eFLL/FuzzyRuleConsequent.cpp
        ${LEGACY_LIBDEPS}/eFLL/FuzzySet.cpp
        ${LEGACY_LIBDEPS}/rvt-OptionParser/src/optparser.cpp
        ../lib/bbq/bbqfanonly.cpp
        ../lib/bbq/pwmventilator.cpp
        ../lib/bbq/NTCSensor.cpp
        ../lib/bbq/onoffventilator.cpp
        ../lib/bbq/analogin.cpp
        ../lib/bbq/numericknob.cpp
        ../lib/bbq/digitalknob.cpp
        ../lib/utils/propertyutils.cpp
        ../lib/utils/utils.cpp
        ../lib/utils/makestring.cpp
    )

    include_directories(
        stubs
        ${LEGACY_LIBDEPS}/eFLL
        ${LEGACY_LIBDEPS}/rvt-OptionParser/src
        ../lib/bbq
        ../lib/utils
    )

    add_executable(tests main.cpp src/arduinostubs.hpp ${LIB_SOURCES})
    target_link_libraries(tests Catch)
endif()
