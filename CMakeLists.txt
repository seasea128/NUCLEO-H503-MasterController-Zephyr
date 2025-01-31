cmake_minimum_required(VERSION 3.20.0)

set(BOARD nucleo_h503rb)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_C_STANDARD 11)

option(MasterController_LTO "Enable LTO for the main application" ON)

find_package(Zephyr)
project(MasterController)
set_property(TARGET app PROPERTY C_STANDARD 23)

# NanoPB for protobuf code generation
list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
include(nanopb)

if(MasterController_LTO)
	set_property(TARGET app PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

zephyr_nanopb_sources(app ./Protobuf-FYP/proto/data.proto)
zephyr_nanopb_sources(app ./Protobuf-FYP/proto/timestamp.proto)

target_include_directories(app PRIVATE ./include/)

target_sources(app PRIVATE 
		       src/main.c
			   src/file_op.c
			   src/can.c
			   src/save_data_thread.c
			   src/message.c
			   src/sim7600_driver.c
)

