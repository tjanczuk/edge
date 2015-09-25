#!/bin/bash

THE_USER=${SUDO_USER:-${USERNAME:-guest}}

set -e
sudo -u ${THE_USER} mkdir /home/${THE_USER}/tmp

# install prerequisities

apt-get -y install curl g++ pkg-config libgdiplus

# download and build Node.js

sudo -u ${THE_USER} curl https://codeload.github.com/joyent/node/tar.gz/v4.1.1 > node.v4.1.1.tar.gz
sudo -u ${THE_USER} tar -xvf node.v4.1.1.tar.gz
cd node-4.1.1/
sudo -u ${THE_USER} bash -c './configure'
sudo -u ${THE_USER} make
make install
cd ..

# install node-gyp and mocha

npm install node-gyp -g
npm install mocha -g

# download and build Mono x64

sudo -u ${THE_USER} curl http://download.mono-project.com/sources/mono/mono-4.0.4.1.tar.bz2 > mono-4.0.4.1.tar.bz2
sudo -u ${THE_USER} tar -xvf mono-4.0.4.1.tar.bz2
cd mono-4.0.4.1
# see http://stackoverflow.com/questions/15627951/mono-dllnotfound-error
sudo -u ${THE_USER} bash -c 'sed -i "s/\@prefix\@\/lib\///g" ./data/config.in'
sudo -u ${THE_USER} bash -c './configure --prefix=/usr/local --with-glib=embedded --enable-nls=no'
sudo -u ${THE_USER} make
make install
ldconfig
cd ..

# download and build Edge.js

sudo -u ${THE_USER} curl https://codeload.github.com/tjanczuk/edge/zip/master > edge.js.zip
sudo -u ${THE_USER} unzip edge.js.zip 
cd edge-master/
sudo -u ${THE_USER} npm install
sudo -u ${THE_USER} npm test
