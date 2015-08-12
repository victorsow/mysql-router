# Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

cmake_minimum_required(VERSION 2.8.11)

include(cmake/version.cmake)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

if(${CMAKE_VERSION} VERSION_GREATER "3.0")
  cmake_policy(SET CMP0042 NEW)
  cmake_policy(SET CMP0046 NEW)
  cmake_policy(SET CMP0048 NEW)
  if(${CMAKE_VERSION} VERSION_GREATER "3.1")
    cmake_policy(SET CMP0054 NEW)
  endif()
  project("MySQLRouter" VERSION ${PROJECT_VERSION_TEXT} LANGUAGES C CXX)
else()
  project("MySQLRouter")
endif()

include(cmake/settings.cmake)

# Required tools, libraries, etc..
include(cmake/testing.cmake)  # does not enable testing
include(cmake/platform.cmake)
include(cmake/compiler.cmake)
find_package(Threads REQUIRED)
include(cmake/harness.cmake)
include(cmake/docs.cmake)
include(cmake/packaging.cmake)
include_directories(${CMAKE_BINARY_DIR}/include)

# Enable testing
if(ENABLE_TESTS)
  include(cmake/python.cmake)
  include(cmake/gmocktest.cmake)
  enable_testing()
  add_subdirectory(tests)
endif()

# Compiler & Linker settings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX11_FLAG}")

# Configure
include(cmake/configure.cmake)

# Load all modules, including plugins
add_subdirectory(src)
