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
from _miccheck.common import exceptions as ex
from _miccheck.common import printing as prnt
from _miccheck.common import testtypes as ttypes

class TestRunner:
    #d_ : default, o_ : optional
    #htests : host tests, dtests: device tests
    def __init__(self, d_htests, o_htests, d_dtests, o_dtests):
        self._num_tests_run = 0
        self._d_htests = d_htests
        self._o_htests = o_htests
        self._d_dtests = d_dtests
        self._o_dtests = o_dtests

    def _run(self, test, test_output, device=-1):
        try:
            test.run(device=device)
            test_output += " ... pass"
        except Exception as excp:
            test_output += " ... fail\n    {0}".format(str(excp))
            raise
        finally:
            self._num_tests_run += 1
            out = prnt.p_out if test.get_status() == ttypes.PASS else prnt.p_err
            out(test_output)

    def _run_device_test(self, test, device):
        test_output = ('  Test %d (mic%d): %s' % (self._num_tests_run,
                       device.dev, test.msg_executing()))
        self._run(test, test_output, device)


    def _run_host_test(self, test):
        test_output = ('  Test %d: %s' % (self._num_tests_run,
                       test.msg_executing()))
        self._run(test, test_output)

    def run(self, devices_list):
        if self._d_htests:
            prnt.p_out('Executing default tests for host')

        for test in self._d_htests:
            self._run_host_test(test)

        #run optional host tests
        if self._o_htests:
            prnt.p_out('Executing optional tests for host')

        optional_host_fail = False
        for test in self._o_htests:
            try:
                self._run_host_test(test)
            except:
                optional_host_fail = True
                continue

        device_failed = False
        # main loop for every device
        for i, device in enumerate(devices_list):
            inner_device_failed = False

            if self._d_dtests:
                prnt.p_out("Executing default tests for device: {0}".format(device.name))
                # inner loop for default device tests. Break if failure.
                try:
                    for test in self._d_dtests[i]:
                        self._run_device_test(test, device)
                except ex.FailedTestException:
                    device_failed = inner_device_failed = True

            # if inner_device_failed, or there aren't any optional tests,
            # continue testing next device
            if inner_device_failed or not self._o_dtests:
                continue

            # inner loop for optional device tests. Continue if failue.
            prnt.p_out('Executing optional tests for device: {0}'.format(device.name))
            for test in self._o_dtests[i]:
                try:
                    self._run_device_test(test, device)
                except ex.FailedTestException:
                    device_failed = True
                    continue

        if optional_host_fail:
            raise ex.FailedTestException('An optional host test failed')

        if device_failed:
            raise ex.FailedTestException('A device test failed')

    def get_tests(self):
        return (self._d_htests, self._o_htests,
                self._d_dtests, self._o_dtests)

