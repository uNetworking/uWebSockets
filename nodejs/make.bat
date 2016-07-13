call "%VS140COMNTOOLS%..\..\vc\vcvarsall.bat" amd64

if not exist targets (
mkdir targets
curl https://nodejs.org/dist/v6.3.0/node-v6.3.0-headers.tar.gz | tar xz -C targets
curl https://nodejs.org/dist/v6.3.0/win-x64/node.lib > targets/node-v6.3.0/node.lib
)

cl /I targets/node-v6.3.0/include/node /DNODEJS_WINDOWS /EHsc /Ox /LD /Fedist/uws_win32_48.node addon.cpp ../src/*.cpp targets/node-v6.3.0/node.lib

rm *.obj
rm dist/*.exp
rm dist/*.lib
