cmake_minimum_required(VERSION 2.6)

PROJECT(tffs-tools C)
ADD_DEFINITIONS(-Os -ggdb -Wall -Werror --std=gnu99 -Wmissing-declarations)

SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

ADD_EXECUTABLE(fritz_tffs_read fritz_tffs_read.c)
INSTALL(TARGETS fritz_tffs_read RUNTIME DESTINATION sbin)
