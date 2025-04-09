build_lib(
  LIBNAME leo
  SOURCE_FILES
  helper/file-reader.cc
  helper/ip-assignment.cc
  helper/routing-manager.cc
  HEADER_FILES
    helper/file-reader.h
    helper/ip-assignment.h
    helper/routing-manager.h
    model/custom-node-data.h
  )
