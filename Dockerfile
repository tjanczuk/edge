#
# Edge.js Dockerfile
# Debian with Node.js, Edge.js, Mono, CoreCLR
#

FROM nodesource/trusty:6.3.0
WORKDIR /data
ADD ./samples /data/samples
RUN bash -c ' \
  set -eux && \
  sed -i "s/..\/lib\/edge/edge/g" /data/samples/*.js && \
  \
  echo "deb [arch=amd64] https://apt-mo.trafficmanager.net/repos/dotnet-release/ trusty main" > /etc/apt/sources.list.d/dotnetdev.list && \
  apt-key adv --keyserver apt-mo.trafficmanager.net --recv-keys 417A0893 && \
  apt-get -y update && \
  apt-get -y install dotnet-dev-1.0.0-preview2-003121 && \
  \
  apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 3FA7E0328081BFF6A14DA29AA6A19B38D3D831EF && \
  echo "deb http://download.mono-project.com/repo/debian wheezy/snapshots/4.2.4 main" | tee /etc/apt/sources.list.d/mono-xamarin.list && \
  apt-get -y update && \
  apt-get -y install curl g++ pkg-config libgdiplus libunwind8 libssl-dev make mono-complete gettext libssl-dev libcurl4-openssl-dev zlib1g libicu-dev uuid-dev unzip && \
  \
  npm i tjanczuk/edge && \
  npm cache clean'
CMD [ "bash", "-c", "echo Welcome to Edge.js && echo Samples are in /data/samples && echo Documentation is at https://github.com/tjanczuk/edge && echo Use EDGE_USE_CORECLR=1 environment variable to select CoreCLR, otherwise Mono is used && bash" ]
