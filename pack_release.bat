@echo off
premake5 ninja
pushd bin
ninja
ninja Release
popd
bin\Debug\lucyban.exe just_pack 
