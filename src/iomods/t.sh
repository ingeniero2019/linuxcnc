#!/bin/bash
cd /home/git/linuxcnc-dev/tests
runtests interp
runtests tool-info
runtests io-startup
runtests toolchanger

