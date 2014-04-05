require 'formula'

class Mono < Formula
  url 'http://download.mono-project.com/sources/mono/mono-3.0.7.tar.bz2'
  homepage 'http://www.mono-project.com'
  sha1 '0699c119f6aded3912797b45049932609020bc29'

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