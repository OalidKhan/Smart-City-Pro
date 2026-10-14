cmake_minimum_required(VERSION 3.21)

project(SmartCityPro VERSION 0.6.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Concurrent)

add_library(smartcity_core
    src/core/models/CityLocation.cpp
    src/core/models/CityLocation.h
    src/core/models/ConstructionCandidate.cpp
    src/core/models/ConstructionCandidate.h
    src/core/models/EmergencyModels.cpp
    src/core/models/EmergencyModels.h
    src/core/models/Road.cpp
    src/core/models/Road.h
    src/algorithms/WeightedGraph.cpp
    src/algorithms/WeightedGraph.h
    src/algorithms/AlgorithmEvent.h
    src/algorithms/DisjointSetUnion.cpp
    src/algorithms/DisjointSetUnion.h
    src/algorithms/InfrastructureAlgorithms.cpp
    src/algorithms/InfrastructureAlgorithms.h
    src/algorithms/RouteAlgorithms.cpp
    src/algorithms/RouteAlgorithms.h
    src/algorithms/FloydWarshall.cpp
    src/algorithms/FloydWarshall.h
    src/services/CityRepository.cpp
    src/services/CityRepository.h
    src/services/EmergencyDispatchSimulation.cpp
    src/services/EmergencyDispatchSimulation.h
)

target_include_directories(smartcity_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(SmartCityPro
    src/main.cpp
    src/gui/MainWindow.cpp
    src/gui/MainWindow.h
    src/gui/animation/AlgorithmAnimationController.cpp
    src/gui/animation/AlgorithmAnimationController.h
    src/gui/pages/DashboardPage.cpp
    src/gui/pages/DashboardPage.h
    src/gui/pages/CityMapPage.cpp
    src/gui/pages/CityMapPage.h
    src/gui/pages/PlaceholderPage.cpp
    src/gui/pages/PlaceholderPage.h
    src/gui/pages/RouteFinderPage.cpp
    src/gui/pages/RouteFinderPage.h
    src/gui/pages/AllPairsRoutesPage.cpp
    src/gui/pages/AllPairsRoutesPage.h
    src/gui/pages/InfrastructurePage.cpp
    src/gui/pages/InfrastructurePage.h
    src/gui/pages/EmergencyDispatchPage.cpp
    src/gui/pages/EmergencyDispatchPage.h
    src/services/CityJsonSerializer.cpp
    src/services/CityJsonSerializer.h
    src/services/DispatchJsonSerializer.cpp
    src/services/DispatchJsonSerializer.h
)

set_target_properties(SmartCityPro PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)

target_link_libraries(SmartCityPro PRIVATE smartcity_core Qt6::Widgets Qt6::Concurrent)

if(WIN32)
    set_target_properties(SmartCityPro PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(smartcity_core_tests tests/core_tests.cpp)
    target_link_libraries(smartcity_core_tests PRIVATE smartcity_core)
    add_test(NAME smartcity_core_tests COMMAND smartcity_core_tests)
endif()

