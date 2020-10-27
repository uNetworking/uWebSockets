#!/bin/bash
set -eoux pipefail

basedir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${basedir}/.."

show_library_deps()
{
  pushd 'install/lib'
  ldd 'libuwebsockets.so'
  popd
}

# $1 : build dir suffix
# $2+: additional cmake options
cmake_build()
{
  rm -rf "build.${1}" && mkdir "build.${1}"
  pushd "build.${1}"
  cmake -DCMAKE_INSTALL_PREFIX=install ${@:2} ..
  cmake --build .
  cmake --install .
  show_library_deps
  popd
}

# Without examples
cmake_build 'default'
cmake_build 'openssl' '-DENABLE_OPENSSL=ON'
cmake_build 'libuv' '-DENABLE_LIBUV=ON'
cmake_build 'openssl-libuv' '-DENABLE_OPENSSL=ON -DENABLE_LIBUV=ON'

# With examples
cmake_build 'default-examples'
cmake_build 'openssl-examples' '-DENABLE_OPENSSL=ON -DBUILD_EXAMPLES=ON'
cmake_build 'libuv-examples' '-DENABLE_LIBUV=ON -DBUILD_EXAMPLES=ON'
cmake_build 'openssl-libuv-examples' '-DENABLE_OPENSSL=ON -DENABLE_LIBUV=ON -DBUILD_EXAMPLES=ON'
