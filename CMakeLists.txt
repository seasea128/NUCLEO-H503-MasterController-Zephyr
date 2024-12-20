cmake_minimum_required(VERSION 3.20.0)

set(BOARD nucleo_h503rb)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_C_STANDARD 23)

find_package(Zephyr)
project(MasterController)
set_property(TARGET app PROPERTY C_STANDARD 23)

target_include_directories(app PRIVATE ./include/)

target_sources(app PRIVATE 
		       src/main.c
			   src/file_op.c
			   src/can.c
			   src/save_data_thread.c
			   src/message.c
)
