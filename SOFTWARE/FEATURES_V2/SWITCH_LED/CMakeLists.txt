# CMakeLists.txt a l'arrel del projecte

cmake_minimum_required(VERSION 3.16)

# Indiquem on és l'ESP-IDF
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Nom del projecte
project(SWITCH_LED)
