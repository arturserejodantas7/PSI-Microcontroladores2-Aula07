# Semáforo de PEDESTRES — placa 1
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(semaforo_pedestres)

set(LIB ${CMAKE_CURRENT_SOURCE_DIR}/../lib)
target_include_directories(app PRIVATE ${LIB}/include)
target_sources(app PRIVATE
  src/main.c
  ${LIB}/src/semaforo_logica.c
  ${LIB}/src/hal_comum.c
  ${LIB}/src/hal_gpio.c
  ${LIB}/src/pedestres.c
)
