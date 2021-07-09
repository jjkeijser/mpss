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
"""Constants Module"""

class Enum(object):
    """Creates a dict of the application actions"""
    def __init__(self, **kwargs):
        self.d = kwargs
        for k, v in self.d.iteritems():
            setattr(self, k, v)


    def elements(self):
        return self.d.keys()

    def __iter__(self):
        for element, value in self.d.iteritems():
            yield element, value

Subcommandsmb = Enum(setpass="set-pass",
                     getinfo="getinfo",
                     cluster="set-cluster-mode",
                     ecc="set-ecc",
                     apeisupp="set-apei-supp",
                     apeiffm="set-apei-ffm",
                     apeieinj="set-apei-einj",
                     apeitable="set-apei-einjtable",
                     fwlock="set-fwlock")
