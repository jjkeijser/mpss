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
import logging
import re
import sys

from textwrap import TextWrapper

MAX_WIDTH = 80

out_log = logging.getLogger('out')
out_log.addHandler(logging.StreamHandler(sys.stdout))
out_log.setLevel(logging.INFO)

err_log = logging.getLogger('err')
err_log.addHandler(logging.StreamHandler(sys.stderr))
err_log.setLevel(logging.ERROR)

def _print(logger, level, msg):
    wrapper = TextWrapper(width=MAX_WIDTH, replace_whitespace=False,
                          subsequent_indent="    ")
    parags = re.split(r"(\n+)", msg)
    lines = []
    for p in parags:
        #workaround: preserve explicit newline chars.
        # see bugs.python.org/issue1859 for more info
        if "\n" in p:
            count = p.count("\n") - 1
            if not count:
                continue
            logger.log(level, "\n" * count)
        else:
            for line in wrapper.wrap(p):
                logger.log(level, line)

def p_out(msg):
    _print(out_log, logging.INFO, msg)

def p_out_debug(msg):
    _print(out_log, logging.DEBUG, msg)

def set_debug():
    out_log.setLevel(logging.DEBUG)

def p_err(msg):
    _print(err_log, logging.ERROR, msg)

