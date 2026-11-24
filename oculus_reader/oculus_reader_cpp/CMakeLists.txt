cmake_minimum_required(VERSION 3.16)
project(oculus_reader_driver)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Eigen3 REQUIRED)

add_library(${PROJECT_NAME}
  src/oculus_reader.cpp
  src/oculus_button_parser.cpp
)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(${PROJECT_NAME} PUBLIC Eigen3::Eigen)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}Targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT ${PROJECT_NAME}Targets
  FILE ${PROJECT_NAME}Targets.cmake
  NAMESPACE ${PROJECT_NAME}::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

option(BUILD_TEST_DRIVER "Build the standalone test executable" OFF)

if(BUILD_TEST_DRIVER)
  add_executable(test_oculus_driver
    src/test_driver_oculus.cpp
  )
  target_link_libraries(test_oculus_driver PRIVATE ${PROJECT_NAME})
endif()

configure_package_config_file(
  cmake/${PROJECT_NAME}Config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)