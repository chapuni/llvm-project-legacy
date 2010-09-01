"""Test Python APIs for target, breakpoint, and process."""

import os, sys, time
import unittest2
import lldb
from lldbtest import *

class HelloWorldTestCase(TestBase):

    mydir = "hello_world"

    @unittest2.skipUnless(sys.platform.startswith("darwin"), "requires Darwin")
    def test_with_dsym_and_run_command(self):
        """Create target, breakpoint, launch a process, and then kill it.

        Use dsym info and lldb "run" command.
        """
        self.buildDsym()
        self.hello_world_python(useLaunchAPI = False)

    #@unittest2.expectedFailure
    def test_with_dwarf_and_process_launch_api(self):
        """Create target, breakpoint, launch a process, and then kill it.

        Use dwarf map (no dsym) and process launch API.
        """
        self.buildDwarf()
        self.hello_world_python(useLaunchAPI = True)

    def hello_world_python(self, useLaunchAPI):
        """Create target, breakpoint, launch a process, and then kill it."""

        exe = os.path.join(os.getcwd(), "a.out")

        target = self.dbg.CreateTarget(exe)

        breakpoint = target.BreakpointCreateByLocation("main.c", 4)

        # The default state after breakpoint creation should be enabled.
        self.assertTrue(breakpoint.IsEnabled(),
                        "Breakpoint should be enabled after creation")

        breakpoint.SetEnabled(False)
        self.assertTrue(not breakpoint.IsEnabled(),
                        "Breakpoint.SetEnabled(False) works")

        breakpoint.SetEnabled(True)
        self.assertTrue(breakpoint.IsEnabled(),
                        "Breakpoint.SetEnabled(True) works")

        # rdar://problem/8364687
        # SBTarget.LaunchProcess() issue (or is there some race condition)?

        if useLaunchAPI:
            process = target.LaunchProcess([''], [''], os.ctermid(), 0, False)
            # Apply some dances after LaunchProcess() in order to break at "main".
            # It only works sometimes.
            self.breakAfterLaunch(process, "main")
        else:
            # On the other hand, the following line of code are more reliable.
            self.runCmd("run", setCookie=False)

        #self.runCmd("thread backtrace")
        #self.runCmd("breakpoint list")
        #self.runCmd("thread list")

        process = target.GetProcess()
        thread = process.GetThreadAtIndex(0)

        self.assertTrue(thread.GetStopReason() == StopReasonEnum("Breakpoint"),
                        STOPPED_DUE_TO_BREAKPOINT)

        # The breakpoint should have a hit count of 1.
        self.assertTrue(breakpoint.GetHitCount() == 1, BREAKPOINT_HIT_ONCE)

        # Now kill the process, and we are done.
        rc = process.Kill()
        self.assertTrue(rc.Success())


if __name__ == '__main__':
    import atexit
    lldb.SBDebugger.Initialize()
    atexit.register(lambda: lldb.SBDebugger.Terminate())
    unittest2.main()
