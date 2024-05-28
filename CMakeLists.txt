# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hello_world)

target_sources(app PRIVATE src/main.c)
target_include_directories(app PRIVATE src)
target_sources_ifdef(CONFIG_NRF_CLOUD_COAP app PRIVATE src/cloud_coap.c)
