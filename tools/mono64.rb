require 'formula'

class Mono64 < Formula
  url 'http://download.mono-project.com/sources/mono/mono-4.0.4.1.tar.bz2'
  homepage 'http://www.mono-project.com'
  sha1 '12f3dbdac92e937cafba1d4e5a168c4cf2620935'

  def install
    args = ["--prefix=#{prefix}",
            "--with-glib=embedded",
            "--enable-nls=no"]

    args << "--host=x86_64-apple-darwin10" if MacOS.prefer_64_bit?

    system "./configure", *args
    system "make"
    system "make install"
  end
end