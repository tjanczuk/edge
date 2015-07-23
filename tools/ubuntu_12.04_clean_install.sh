#!/bin/bash

THE_USER=${SUDO_USER:-${USERNAME:-guest}}

set -e

# install prerequisities and Mono x64
if [ ! -e /etc/apt/sources.list.d/mono-xamarin.list ]
then
    apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 3FA7E0328081BFF6A14DA29AA6A19B38D3D831EF
    echo "deb http://download.mono-project.com/repo/debian wheezy main" | tee /etc/apt/sources.list.d/mono-xamarin.list
    apt-get update
fi

apt-get -y install curl g++ pkg-config libgdiplus libunwind8 libssl-dev unzip make mono-complete

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

# download and install CoreCLR

curl -sSL https://raw.githubusercontent.com/aspnet/Home/dev/dnvminstall.sh | DNX_BRANCH=dev sudo -u ${THE_USER} sh

su ${THE_USER} -l -s /bin/bash -c "source .dnx/dnvm/dnvm.sh && dnvm install 1.0.0-beta7-12274 -r coreclr -u -a edge-coreclr"

curl -sSL https://raw.githubusercontent.com/aspnet/Home/dev/dnvminstall.sh | DNX_BRANCH=dev sh && source ~/.dnx/dnvm/dnvm.sh
dnvm install latest -r coreclr -u -p

CLR_VERSION=$(dnvm list | grep " \*" | grep -oE '[0-9][^ ]+')
chmod 775 $DNX_USER_HOME/runtimes/dnx-coreclr-linux-x64.$CLR_VERSION/bin/dnx
chmod 775 $DNX_USER_HOME/runtimes/dnx-coreclr-linux-x64.$CLR_VERSION/bin/dnu

# download and build Edge.js

sudo -u ${THE_USER} curl https://codeload.github.com/medicomp/edge/zip/master > edge.js.zip
sudo -u ${THE_USER} unzip edge.js.zip 
cd edge-master/
EDGE_DIRECTORY=$(pwd)

su ${THE_USER} -l -s /bin/bash -c "source ~/.dnx/dnvm/dnvm.sh && dnvm use edge-mono && cd $EDGE_DIRECTORY && npm install"

su ${THE_USER} -l -s /bin/bash -c "cd $EDGE_DIRECTORY && npm test"
su ${THE_USER} -l -s /bin/bash -c "source ~/.dnx/dnvm/dnvm.sh && dnvm use edge-coreclr && cd $EDGE_DIRECTORY && EDGE_USE_CORECLR=1 npm test"
