cmake_minimum_required(VERSION 3.20.0)

set(BOARD nucleo_h503rb)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_C_STANDARD 11)

option(MC_LTO "Enable LTO for the main application" ON)
set(MC_ControllerName "MC_1" CACHE STRING "Controller Name")
set(MC_ServerAddress "tcp://94.130.24.30:1883" CACHE STRING "Remote server address")

find_package(Zephyr)
project(MasterController)
set_property(TARGET app PROPERTY C_STANDARD 23)

# NanoPB for protobuf code generation
list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
include(nanopb)

if(MC_LTO)
	set_property(TARGET app PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# Compile protobuf into C headers
zephyr_nanopb_sources(app ./Protobuf-FYP/proto/data.proto)
zephyr_nanopb_sources(app ./Protobuf-FYP/proto/timestamp.proto)

target_include_directories(app PRIVATE
	./include/
	./include/upload_thread/
)

# Add defines for variable defined by CMake
target_compile_definitions(app PRIVATE CONTROLLER_NAME="${MC_ControllerName}")
target_compile_definitions(app PRIVATE SERVER_ADDR="${MC_ServerAddress}")

target_sources(app PRIVATE 
		      src/main.c
			  src/main_state.c
			  src/file_op.c
			  src/can.c
			  src/save_data_thread.c
			  src/message.c
			  src/sim7600_driver.c
			  src/upload_thread/upload_thread.c
			  src/upload_thread/upload_state.c
)
