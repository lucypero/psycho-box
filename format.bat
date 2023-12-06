@echo off
pushd src
clang-format -i *.cpp *.hpp
popd