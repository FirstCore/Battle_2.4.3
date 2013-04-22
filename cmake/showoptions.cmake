# ShowOptions

# Output generic information about the core and buildtype chosen

message("")
message("* BlizzLikeCore rev. hash   : ${rev_hash}")
message("* BlizzLikeCore rev. date   : ${rev_date}")
if( UNIX )
  message("* BlizzLikeCore buildtype  : ${CMAKE_BUILD_TYPE}")
endif()
message("")

# Output information about installation-directories and locations

message("* Install core to : ${CMAKE_INSTALL_PREFIX}")
if( UNIX )
  message("* Install libraries to   : ${LIBSDIR}")
  message("* Install configs to     : ${CONF_DIR}")
endif()
message("")

# Show infomation about the options selected during configuration

if( SERVERS )
  message("* Build world/auth          : Yes (default)")
else()
  message("* Build world/auth          : No")
endif()

if( SCRIPTS )
  message("* Build with scripts        : Yes (default)")
  add_definitions(-DSCRIPTS)
else()
  message("* Build with scripts        : No")
  set(USE_SCRIPTPCH 0)
endif()

if( TOOLS )
  message("* Build map/vmap/mmap tools : Yes")
  add_definitions(-DMMAP_GENERATOR)
else()
  message("* Build map/vmap/mmap tools : No  (default)")
endif()

if( USE_COREPCH )
  message("* Build core w/PCH          : Yes (default)")
else()
  message("* Build core w/PCH          : No")
endif()

if( USE_SCRIPTPCH )
  message("* Build scripts w/PCH       : Yes (default)")
else()
  message("* Build scripts w/PCH       : No")
endif()

if( USE_SFMT )
  message("* Use SFMT for RNG          : Yes")
  add_definitions(-DUSE_SFMT_FOR_RNG)
else()
  message("* Use SFMT for RNG          : No  (default)")
endif()

if( WITH_WARNINGS )
  message("* Show all warnings         : Yes")
else()
  message("* Show compile-warnings     : No  (default)")
endif()

if( WITH_COREDEBUG )
  message("* Use coreside debug        : Yes")
  add_definitions(-DBLIZZLIKE_DEBUG)
else()
  message("* Use coreside debug        : No  (default)")
endif()

if( WIN32 )
  if( USE_MYSQL_SOURCES )
    message("* Use MySQL sourcetree      : Yes (default)")
  else()
    message("* Use MySQL sourcetree      : No")
  endif()
endif( WIN32 )

message("")
