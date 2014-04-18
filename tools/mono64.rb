require 'formula'

class Mono64 < Formula
  url 'http://download.mono-project.com/sources/mono/mono-3.4.0.tar.bz2'
  homepage 'http://www.mono-project.com'
  sha1 'bae86f50f9a29d68d4e1917358996e7186e7f89e'

  def install
    args = ["--prefix=#{prefix}",
            "--with-glib=embedded",
            "--enable-nls=no"]

    args << "--host=x86_64-apple-darwin10" if MacOS.prefer_64_bit?

    system "curl https://raw.githubusercontent.com/tjanczuk/edge/master/tools/Microsoft.Portable.Common.targets > ./mcs/tools/xbuild/targets/Microsoft.Portable.Common.targets"
    system "./configure", *args
    system "make"
    system "make install"
  end
end