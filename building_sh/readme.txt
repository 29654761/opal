在linux安装时

apt install autoconf
apt install libtool

如果在安装libtool就生成了conf，会报错:'aclocal-1.15' is missing on your system.
解决：安装上面2个库后cd到opal或者出错的plugins项目的./configure 目录执行下 autoreconf -i -f



tar -jxvf ptlib-2.18.8.tar.bz2
tar -jxvf opal-3.18.8.tar.bz2

解压后从windows版本的源码中覆盖文件
ptlib下的include，除ptlib_config.h.in之外的所有
ptlib下的src目录

opal下的 include,除了opal_config.h.in的所有
opal下的src目录
