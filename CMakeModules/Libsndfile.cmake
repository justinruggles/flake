MACRO(CHECK_LIBSNDFILE_DEFINE VAR)
SET(CMAKE_REQUIRED_LIBRARIES "-lsndfile")
CHECK_C_SOURCE_COMPILES(
"
#include <sndfile.h>
int main(){
    char buffer[128];
    sf_command(NULL, SFC_GET_LIB_VERSION, buffer, sizeof(buffer));
#ifndef SNDFILE_1
#error \"Version 1.0.X required.\"
#endif
}
" ${VAR})
IF(${VAR})
  ADD_DEFINE("${VAR} 1")
  SET(ADD_LIBS "${ADD_LIBS}-lsndfile")
ENDIF(${VAR})
SET(CMAKE_REQUIRED_LIBRARIES "")
ENDMACRO(CHECK_LIBSNDFILE_DEFINE)
