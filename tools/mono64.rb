require 'formula'

class Mono64 < Formula
  url 'http://download.mono-project.com/sources/mono/mono-4.0.1.44.tar.bz2'
  homepage 'http://www.mono-project.com'
  sha256 'eaf5bd9d19818cb89483b3c9cae2ee3569643fd621560da036f6a49f6b3e3a6f'

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
