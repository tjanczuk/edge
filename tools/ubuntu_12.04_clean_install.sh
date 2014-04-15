#!/bin/bash

sudo -u ${USERNAME} set -e
sudo -u ${USERNAME} mkdir ~/tmp

# install prerequisities

apt-get install curl g++ pkg-config

# download and build Node.js

sudo -u ${USERNAME} curl https://codeload.github.com/joyent/node/tar.gz/v0.10.26 > node.v0.10.26.tar.gz
sudo -u ${USERNAME} tar -xvf node.v0.10.26.tar.gz
sudo -u ${USERNAME} cd node-0.10.26/
sudo -u ${USERNAME} ./configure
sudo -u ${USERNAME} make
make install
sudo -u ${USERNAME} cd ..

# install node-gyp and mocha

npm install node-gyp -g
npm install mocha -g

# download and build Mono x64

sudo -u ${USERNAME} curl http://download.mono-project.com/sources/mono/mono-3.4.0.tar.bz2 > mono-3.4.0.tar.bz2
sudo -u ${USERNAME} tar -xvf mono-3.4.0.tar.bz2
sudo -u ${USERNAME} curl https://raw.githubusercontent.com/tjanczuk/edge/mono/tools/Microsoft.Portable.Common.targets > ./mono-3.4.0/mcs/tools/xbuild/targets/Microsoft.Portable.Common.targets
sudo -u ${USERNAME} cd mono-3.4.0
sudo -u ${USERNAME} ./configure --prefix=/usr/local --with-glib=embedded --enable-nls=no
sudo -u ${USERNAME} make
make install
ln -s /lib/x86_64-linux-gnu/libc.so.6 /lib/x86_64-linux-gnu/libc.so
ldconfig
sudo -u ${USERNAME} cd ..

# download and build Edge.js

sudo -u ${USERNAME} curl https://codeload.github.com/tjanczuk/edge/zip/mono > edge.js.zip
sudo -u ${USERNAME} unzip edge.js.zip 
sudo -u ${USERNAME} cd edge-mono/
sudo -u ${USERNAME} npm install
sudo -u ${USERNAME} npm test
