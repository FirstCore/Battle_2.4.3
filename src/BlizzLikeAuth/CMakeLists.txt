# Copyright (C) 2011-2013 BlizzLikeCore <http://blizzlike.servegame.com/>
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

file(GLOB sources_localdir *.cpp *.h)

set(AUTHSERVER_SRCS
  ${AUTHSERVER_SRCS}
  ${sources_localdir}
)

if( WIN32 )
  set(AUTHSERVER_SRCS
    ${AUTHSERVER_SRCS}
    ${sources_Debugging}
    authserver.rc
  )
endif()

include_directories(
  ${CMAKE_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}/src/shared
  ${CMAKE_SOURCE_DIR}/src/shared/Database
  ${CMAKE_SOURCE_DIR}/src/framework
  ${ACE_INCLUDE_DIR}
  ${MYSQL_INCLUDE_DIR}
  ${OPENSSL_INCLUDE_DIR}
)

set(AUTHSERVER_LINK_FLAGS "")

add_executable(authserver
  ${AUTHSERVER_SRCS}
)

add_dependencies(authserver revision.h)

if( NOT WIN32 )
  add_definitions(-D_BLIZZLIKE_AUTH_CONFIG="${CONF_DIR}/authserver.conf")
endif()

if( UNIX )
  set(AUTHSERVER_LINK_FLAGS "-pthread ${AUTHSERVER_LINK_FLAGS}")
endif()

if( CMAKE_SYSTEM_NAME MATCHES "Darwin" )
  set(AUTHSERVER_LINK_FLAGS "-framework Carbon ${AUTHSERVER_LINK_FLAGS}")
endif()

set_target_properties(authserver PROPERTIES LINK_FLAGS "${AUTHSERVER_LINK_FLAGS}")

target_link_libraries(authserver
  shared
  blizzlikeframework
  ${MYSQL_LIBRARY}
  ${OPENSSL_LIBRARIES}
  ${OPENSSL_EXTRA_LIBRARIES}
  ${OSX_LIBS}
)

if( WIN32 )
  add_custom_command(TARGET authserver
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/authserver.conf.dist ${CMAKE_BINARY_DIR}/bin/$(ConfigurationName)/
  )
endif()

if( UNIX )
  install(TARGETS authserver DESTINATION bin)
  install(FILES authserver.conf.dist DESTINATION etc)
elseif( WIN32 )
  install(TARGETS authserver DESTINATION "${CMAKE_INSTALL_PREFIX}")
  install(FILES authserver.conf.dist DESTINATION "${CMAKE_INSTALL_PREFIX}")
endif()
