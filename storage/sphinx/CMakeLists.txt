INCLUDE(CheckCCompilerFlag)

ADD_DEFINITIONS(-DMYSQL_SERVER)

CHECK_C_COMPILER_FLAG("-Wno-write-strings" WNO_WRITE_STRING)
IF(WNO_WRITE_STRING)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-write-strings")
ENDIF()

IF(MSVC)
  LINK_LIBRARIES(ws2_32)
ENDIF(MSVC)

SET(SPHINX_SOURCES ha_sphinx.cc snippets_udf.cc)
MYSQL_ADD_PLUGIN(sphinx ${SPHINX_SOURCES} STORAGE_ENGINE)
