build_lib(
  LIBNAME futurenet-ntn
  SOURCE_FILES
    model/ntn-attribute-checkers.cc
  HEADER_FILES
    model/ntn-attribute-checkers.h
    model/ntn-common-types.h
    model/ntn-constants.h
  LIBRARIES_TO_LINK
    ${libcore}
    ${libmobility}
    ${libnetwork}
    ${libinternet}
    ${libpoint-to-point}
)
