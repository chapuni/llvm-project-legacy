"""
Test lldb data formatter subsystem.
"""

import os, time
import unittest2
import lldb
from lldbtest import *

class DataFormatterTestCase(TestBase):

    # test for rdar://problem/9973865 (If you use "${var}" in the summary string for an aggregate type, the summary doesn't print for a pointer to that type)
    mydir = os.path.join("functionalities", "data-formatter", "rdar-9973865")

    @unittest2.skipUnless(sys.platform.startswith("darwin"), "requires Darwin")
    def test_with_dsym_and_run_command(self):
        """Test data formatter commands."""
        self.buildDsym()
        self.data_formatter_commands()

    def test_with_dwarf_and_run_command(self):
        """Test data formatter commands."""
        self.buildDwarf()
        self.data_formatter_commands()

    def setUp(self):
        # Call super's setUp().
        TestBase.setUp(self)
        # Find the line number to break at.
        self.line = line_number('main.cpp', '// Set break point at this line.')

    def data_formatter_commands(self):
        """Test that that file and class static variables display correctly."""
        self.runCmd("file a.out", CURRENT_EXECUTABLE_SET)

        self.expect("breakpoint set -f main.cpp -l %d" % self.line,
                    BREAKPOINT_CREATED,
            startstr = "Breakpoint created: 1: file ='main.cpp', line = %d, locations = 1" %
                        self.line)

        self.runCmd("run", RUN_SUCCEEDED)

        # The stop reason of the thread should be breakpoint.
        self.expect("thread list", STOPPED_DUE_TO_BREAKPOINT,
            substrs = ['stopped',
                       'stop reason = breakpoint'])

        # This is the function to remove the custom formats in order to have a
        # clean slate for the next test case.
        def cleanup():
            self.runCmd('type summary clear', check=False)

        # Execute the cleanup function during test case tear down.
        self.addTearDownHook(cleanup)

        self.runCmd("type summary add -f \"SUMMARY SUCCESS ${var}\" Summarize")
        
        self.expect('frame variable mine_ptr',
                substrs = ['SUMMARY SUCCESS <invalid usage of pointer value as object>'])

        self.expect('frame variable *mine_ptr',
                substrs = ['SUMMARY SUCCESS <invalid use of aggregate type>'])

        self.runCmd("type summary add -f \"SUMMARY SUCCESS ${var.first}\" Summarize")

        self.expect('frame variable mine_ptr',
                    substrs = ['SUMMARY SUCCESS 10'])

        self.expect('frame variable *mine_ptr',
                    substrs = ['SUMMARY SUCCESS 10'])

if __name__ == '__main__':
    import atexit
    lldb.SBDebugger.Initialize()
    atexit.register(lambda: lldb.SBDebugger.Terminate())
    unittest2.main()
