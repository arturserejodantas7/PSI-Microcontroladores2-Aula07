cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(unit_logica)

set(LIB ${CMAKE_CURRENT_SOURCE_DIR}/../../lib)
target_include_directories(app PRIVATE ${LIB}/include ${CMAKE_CURRENT_SOURCE_DIR}/../comum)
target_sources(app PRIVATE
  src/main.c
  ${LIB}/src/semaforo_logica.c
)
