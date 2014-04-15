set -e
mkdir ~/tmp

# install prerequisities

sudo apt-get install curl g++ pkg-config gettext

# download and build Node.js

curl https://codeload.github.com/joyent/node/tar.gz/v0.10.26 > node.v0.10.26.tar.gz
tar -xvf node.v0.10.26.tar.gz
cd node-0.10.26/
./configure
make
sudo make install
cd ..

# install node-gyp and mocha

sudo npm install node-gyp -g
sudo npm install mocha -g

# download and build Mono x64

curl http://download.mono-project.com/sources/mono/mono-3.4.0.tar.bz2 > mono-3.4.0.tar.bz2
tar -xvf mono-3.4.0.tar.bz2
curl https://raw.githubusercontent.com/tjanczuk/edge/mono/tools/Microsoft.Portable.Common.targets > ./mono-3.4.0/mcs/tools/xbuild/targets/Microsoft.Portable.Common.targets
cd mono-3.4.0
./configure --prefix=/usr/local --with-glib=embedded
make
sudo make install
sudo ln -s /lib/x86_64-linux-gnu/libc.so.6 /lib/x86_64-linux-gnu/libc.so
cd ..

# download and build Edge.js

curl https://codeload.github.com/tjanczuk/edge/zip/mono > edge.js.zip
unzip edge.js.zip 
cd edge-mono/
npm install
npm test
