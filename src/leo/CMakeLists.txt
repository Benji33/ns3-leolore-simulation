build_lib(
  LIBNAME leo
  SOURCE_FILES
  helper/file-reader.cc
  helper/ip-assignment.cc
  helper/routing-manager.cc
  helper/switching-forwarder.cc
  model/custom-ipv4-l3-protocol.cc
  HEADER_FILES
    helper/file-reader.h
    helper/ip-assignment.h
    helper/routing-manager.h
    helper/switching-forwarder.h
    model/custom-node-data.h
    model/custom-ipv4-l3-protocol.h
  )
