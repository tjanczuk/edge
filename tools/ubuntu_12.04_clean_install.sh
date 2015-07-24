#!/bin/bash

THE_USER=${SUDO_USER:-${USERNAME:-guest}}

set -e
sudo -u ${THE_USER} mkdir ~/tmp

# install prerequisities

apt-get -y install curl g++ pkg-config libgdiplus libunwind8 libssl-dev unzip make

# download and build Node.js

sudo -u ${THE_USER} curl https://codeload.github.com/joyent/node/tar.gz/v0.12.0 > node.v0.12.0.tar.gz
sudo -u ${THE_USER} tar -xvf node.v0.12.0.tar.gz
cd node-0.12.0/
sudo -u ${THE_USER} bash -c './configure'
sudo -u ${THE_USER} make
make install
cd ..

# install node-gyp and mocha

npm install node-gyp -g
npm install mocha -g

# download and build Mono x64

sudo -u ${THE_USER} curl http://download.mono-project.com/sources/mono/mono-4.0.1.44.tar.bz2 > mono-4.0.1.44.tar.bz2
sudo -u ${THE_USER} tar -xvf mono-4.0.1.44.tar.bz2
cd mono-4.0.1
sudo -u ${THE_USER} bash -c './configure --prefix=/usr/local --with-glib=embedded --enable-nls=no'
sudo -u ${THE_USER} make
make install
ldconfig
cd ..

# download and install CoreCLR

curl -sSL https://raw.githubusercontent.com/aspnet/Home/dev/dnvminstall.sh | DNX_BRANCH=dev sh && source ~/.dnx/dnvm/dnvm.sh

dnvm install latest -r coreclr -u
CLR_VERSION=$(dnvm list | grep " \*" | grep -oE '[0-9][^ ]+')

# TODO: remove this once Mono is no longer necessary for dnu restore and dnu build
dnvm install latest -r mono -u

# download and build Edge.js

sudo -u ${THE_USER} curl https://codeload.github.com/medicomp/edge/zip/master > edge.js.zip
sudo -u ${THE_USER} unzip edge.js.zip 
cd edge-master/
npm install --unsafe-perm

dnvm use $CLR_VERSION -r coreclr

sudo -u ${THE_USER} npm test
sudo -u ${THE_USER} EDGE_USE_CORECLR=1 PATH=$PATH npm test
chown -R ${THE_USER} $HOME/.npm
