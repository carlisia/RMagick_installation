# This script is based on a blog post by Solomon White, found here: 
# http://onrails.org/articles/2009/09/04/rmagick-from-source-on-snow-leopard

!/bin/sh

cd /usr/local/src

# prerequisite packages

cd freetype-2.3.11
./configure --prefix=/usr/local
make
sudo make install
cd /usr/local/src

cd libpng-1.2.39
./configure --prefix=/usr/local
make
sudo make install
cd /usr/local/src

cd jpeg-7
ln -s `which glibtool` ./libtool
export MACOSX_DEPLOYMENT_TARGET=10.6
./configure --enable-shared --prefix=/usr/local
make
sudo make install
cd /usr/local/src

cd tiff-3.8.2
./configure --prefix=/usr/local
make
sudo make install
cd /usr/local/src

cd libwmf-0.2.8.4
make clean
./configure
make
sudo make install
cd /usr/local/src

cd lcms-1.17
make clean
./configure
make
sudo make install
cd /usr/local/src

cd ghostscript-8.70
./configure  --prefix=/usr/local
make
sudo make install
cd /usr/local/src

sudo mv fonts /usr/local/share/ghostscript

# Image Magick
cd `ls | grep ImageMagick-`
export CPPFLAGS=-I/usr/local/include
export LDFLAGS=-L/usr/local/lib
./configure --prefix=/usr/local --disable-static --with-modules --without-perl --without-magick-plus-plus --with-quantum-depth=8 --with-gs-font-dir=/usr/local/share/ghostscript/fonts --disable-openmp
make
sudo make install
cd /usr/local/src

# RMagick
sudo gem install rmagick