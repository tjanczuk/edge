#
# Edge.js Dockerfile
# Debian with Node.js, Edge.js, Mono, CoreCLR
#

FROM nodesource/jessie:4.2.3
WORKDIR /data
ADD ./samples /data/samples
RUN bash -c ' \
  set -e && \
  sed -i "s/..\/lib\/edge/edge/g" /data/samples/*.js && \
  apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 3FA7E0328081BFF6A14DA29AA6A19B38D3D831EF && \
  echo "deb http://download.mono-project.com/repo/debian wheezy main" | tee /etc/apt/sources.list.d/mono-xamarin.list && \
  echo "deb http://download.mono-project.com/repo/debian wheezy-libjpeg62-compat main" | tee -a /etc/apt/sources.list.d/mono-xamarin.list && \
  apt-get update && \
  apt-get -y install curl g++ pkg-config libgdiplus libunwind8 libssl-dev make mono-complete gettext libssl-dev libcurl4-openssl-dev zlib1g libicu-dev uuid-dev unzip && \
  curl -sSL https://raw.githubusercontent.com/aspnet/Home/dev/dnvminstall.sh | DNX_BRANCH=dev sh && \
  source /root/.dnx/dnvm/dnvm.sh && \
  dnvm install latest -r coreclr -alias edge-coreclr && \
  dnvm use edge-coreclr && \
  cd / && npm i tjanczuk/edge && \
  npm cache clean'
CMD [ "bash", "-c", "source /root/.dnx/dnvm/dnvm.sh && dnvm use edge-coreclr && echo Welcome to Edge.js && echo Samples are in /data/samples && echo Documentation is at https://github.com/tjanczuk/edge && echo Use EDGE_USE_CORECLR environment variable to choose between Mono and CoreCLR && bash" ]
