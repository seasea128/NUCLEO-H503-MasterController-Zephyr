cmake_minimum_required(VERSION 3.20.0)

set(BOARD nucleo_h503rb)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

find_package(Zephyr)
project(MasterController)

target_include_directories(app PRIVATE ./include/)

target_sources(app PRIVATE 
		       src/main.c
			   src/file_op.c
			   src/can.c
			   src/save_data_thread.c
)
