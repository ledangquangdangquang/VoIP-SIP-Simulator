# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles/SIP_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/SIP_autogen.dir/ParseCache.txt"
  "SIP_autogen"
  )
endif()
