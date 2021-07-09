#!/bin/sh
# Copyright (c) 2017, Intel Corporation.
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU Lesser General Public License,
# version 2.1, as published by the Free Software Foundation.
# 
# This program is distributed in the hope it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
# more details.
#expecting variables:
#   PYINSTALLER
if test -z "$(ldd $(which python) | grep libpython)";
then
    py=/usr/bin/python ;
else
    py=$(which python) ;
fi

cd ../apps/miccheck
$py $PYINSTALLER -F miccheck.py

