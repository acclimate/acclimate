#  Copyright (C) 2017 Sven Willner <sven.willner@gmail.com>
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU Affero General Public License as published
#  by the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU Affero General Public License for more details.
#
#  You should have received a copy of the GNU Affero General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.

set(SETTINGSNODE_MODULES_PATH ${CMAKE_CURRENT_LIST_DIR})
if (NOT TARGET settingsnode)
  add_library(settingsnode INTERFACE)
  function(include_settingsnode TARGET)
    set_property(TARGET settingsnode PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${SETTINGSNODE_MODULES_PATH}/include)
    target_link_libraries(${TARGET} settingsnode)
  endfunction()
endif()
