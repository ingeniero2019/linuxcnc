g++ -L/home/git/linuxcnc-dev/lib \
 -I ../include \
    -Wl,-rpath,/home/git/linuxcnc-dev/lib   \
    -Wl,-soname,libtool_mmap.so.0 -shared \
    -o ../lib/libtool_mmap.so.0 \
    objects/emc/tool_mmap/tool_mmap.o emc/tool_mmap/tst.c -ldl
