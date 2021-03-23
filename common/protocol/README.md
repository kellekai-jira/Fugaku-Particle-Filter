Even if we will not use protobuf later i describe all message types here in protobuf manner as i like the syntax

Packages that might be interesting later to are especially https://capnproto.org/


install protobuf:

  sudo apt install protobuf-compiler

on ubuntu


Just in case you want to compile this using protobuf:

  mkdir -p cpp py
  protoc p2p.proto --python_out=py --cpp_out=cpp


