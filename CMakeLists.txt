cmake_minimum_required(VERSION 3.5)
project(mod-treasure)

set(scripts_STAT_SRCS
  src/mod_treasure.cpp
)

AC_ADD_SCRIPT("${scripts_STAT_SRCS}")
AC_ADD_CONFIG_FILE(conf/mod_treasure.conf.dist)

message(STATUS "-> module 'mod-treasure' loaded.")
