#!/bin/bash

THE_USER=${SUDO_USER:-${USERNAME:-guest}}

set -e

# install prerequisities, Mono x64, and .NET Core x64
if [ ! -e /etc/apt/sources.list.d/mono-xamarin.list ]
then
    apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 3FA7E0328081BFF6A14DA29AA6A19B38D3D831EF
    echo "deb http://download.mono-project.com/repo/debian wheezy main" | tee /etc/apt/sources.list.d/mono-xamarin.list
    echo "deb http://download.mono-project.com/repo/debian wheezy-libjpeg62-compat main" | tee -a /etc/apt/sources.list.d/mono-xamarin.list
    apt-get update
fi

if [ ! -e /etc/apt/sources.list.d/dotnetdev.list ]
then
    echo "deb [arch=amd64] https://apt-mo.trafficmanager.net/repos/dotnet/ trusty main" > /etc/apt/sources.list.d/dotnetdev.list
    apt-key adv --keyserver apt-mo.trafficmanager.net --recv-keys 417A0893
    apt-get update
fi

apt-get -y install curl g++ pkg-config libgdiplus libunwind8 libssl-dev unzip make mono-complete= git gettext libssl-dev libcurl4-openssl-dev zlib1g libicu-dev uuid-dev dotnet-dev-1.0.0-preview2-003121

# download and build Node.js
command -v node || result=$?

if [ $result ]
then
    sudo -u ${THE_USER} curl https://codeload.github.com/nodejs/node/tar.gz/v4.2.3 > node.v4.2.3.tar.gz
    sudo -u ${THE_USER} tar -xvf node.v4.2.3.tar.gz
    cd node-4.2.3/
    sudo -u ${THE_USER} bash -c './configure'
    sudo -u ${THE_USER} make
    make install
    cd ..
fi

# install node-gyp and mocha

npm install node-gyp -g
npm install mocha -g

# download and build Edge.js

sudo -u ${THE_USER} curl https://codeload.github.com/tjanczuk/edge/zip/master > edge.js.zip
sudo -u ${THE_USER} unzip edge.js.zip 
cd edge-master/
EDGE_DIRECTORY=$(pwd)
chown -R ${THE_USER} ~/.npm

su ${THE_USER} -l -s /bin/bash -c "cd $EDGE_DIRECTORY && npm install"
su ${THE_USER} -l -s /bin/bash -c "cd $EDGE_DIRECTORY && npm test"
su ${THE_USER} -l -s /bin/bash -c "cd $EDGE_DIRECTORY && EDGE_USE_CORECLR=1 npm test"
