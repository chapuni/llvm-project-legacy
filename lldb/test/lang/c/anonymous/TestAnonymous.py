"""Test that anonymous structs/unions are transparent to member access"""

import os, time
import unittest2
import lldb
from lldbtest import *
import lldbutil

class AnonymousTestCase(TestBase):

    mydir = os.path.join("lang", "c", "anonymous")

    @dsym_test
    def test_expr_nest_with_dsym(self):
        self.buildDsym()
        self.expr_nest()

    @dsym_test
    def test_expr_child_with_dsym(self):
        self.buildDsym()
        self.expr_child()

    @dsym_test
    def test_expr_grandchild_with_dsym(self):
        self.buildDsym()
        self.expr_grandchild()

    @dsym_test
    def test_expr_parent(self):
        self.buildDsym()
        self.expr_parent()

    @unittest2.expectedFailure # llvm.org/pr15591 
    @dsym_test
    def test_expr_null(self):
        self.buildDsym()
        self.expr_null()

    @skipIfGcc # llvm.org/pr15036: LLDB generates an incorrect AST layout for an anonymous struct when DWARF is generated by GCC
    @skipIfIcc # llvm.org/pr15036: LLDB generates an incorrect AST layout for an anonymous struct when DWARF is generated by ICC
    @dwarf_test
    def test_expr_nest_with_dwarf(self):
        self.buildDwarf()
        self.expr_nest()

    @dwarf_test
    def test_expr_child_with_dwarf(self):
        self.skipTest("Skipped because LLDB asserts due to an incorrect AST layout for an anonymous struct: see llvm.org/pr15036")
        self.buildDwarf()
        self.expr_child()

    @skipIfGcc # llvm.org/pr15036: This particular regression was introduced by r181498
    @skipIfIcc # llvm.org/pr15036: This particular regression was introduced by r181498
    @dwarf_test
    def test_expr_grandchild_with_dwarf(self):
        self.buildDwarf()
        self.expr_grandchild()

    @dwarf_test
    def test_expr_parent(self):
        self.buildDwarf()
        self.expr_parent()

    @unittest2.expectedFailure # llvm.org/pr15591 
    @dwarf_test
    def test_expr_null(self):
        self.buildDwarf()
        self.expr_null()

    def setUp(self):
        # Call super's setUp().
        TestBase.setUp(self)
        # Find the line numbers to break in main.c.
        self.line0 = line_number('main.c', '// Set breakpoint 0 here.')
        self.line1 = line_number('main.c', '// Set breakpoint 1 here.')
        self.line2 = line_number('main.c', '// Set breakpoint 2 here.')

    def common_setup(self, line):
        exe = os.path.join(os.getcwd(), "a.out")
        self.runCmd("file " + exe, CURRENT_EXECUTABLE_SET)

        # Set breakpoints inside and outside methods that take pointers to the containing struct.
        lldbutil.run_break_set_by_file_and_line (self, "main.c", line, num_expected_locations=1, loc_exact=True)

        self.runCmd("run", RUN_SUCCEEDED)

        # The stop reason of the thread should be breakpoint.
        self.expect("thread list", STOPPED_DUE_TO_BREAKPOINT,
            substrs = ['stopped',
                       'stop reason = breakpoint'])

        # The breakpoint should have a hit count of 1.
        self.expect("breakpoint list -f", BREAKPOINT_HIT_ONCE,
            substrs = [' resolved, hit count = 1'])

    def expr_nest(self):
        self.common_setup(self.line0)

        # These should display correctly.
        self.expect("expression n->foo.d", VARIABLES_DISPLAYED_CORRECTLY,
            substrs = ["= 4"])
            
        self.expect("expression n->b", VARIABLES_DISPLAYED_CORRECTLY,
            substrs = ["= 2"])

    def expr_child(self):
        self.common_setup(self.line1)

        # These should display correctly.
        self.expect("expression c->foo.d", VARIABLES_DISPLAYED_CORRECTLY,
            substrs = ["= 4"])
            
        self.expect("expression c->grandchild.b", VARIABLES_DISPLAYED_CORRECTLY,
            substrs = ["= 2"])

    def expr_grandchild(self):
        self.common_setup(self.line2)

        # These should display correctly.
        self.expect("expression g.child.foo.d", VARIABLES_DISPLAYED_CORRECTLY,
            substrs = ["= 4"])
            
        self.expect("expression g.child.b", VARIABLES_DISPLAYED_CORRECTLY,
            substrs = ["= 2"])

    def expr_parent(self):
        self.common_setup(self.line2)

        # These should display correctly.
        self.expect("expression pz", VARIABLES_DISPLAYED_CORRECTLY,
            substrs = ["(type_z *) $0 = 0x0000"])

        self.expect("expression z.y", VARIABLES_DISPLAYED_CORRECTLY,
            substrs = ["(type_y) $1 = {"])

        self.expect("expression z", VARIABLES_DISPLAYED_CORRECTLY,
            substrs = ["dummy = 2"])

    def expr_null(self):
        self.common_setup(self.line2)

        # This should fail because pz is 0, but it succeeds on OS/X.
        # This fails on Linux with an upstream error "Couldn't dematerialize struct", as does "p *n" with "int *n = 0".
        # Note that this can also trigger llvm.org/pr15036 when run interactively at the lldb command prompt.
        self.expect("expression *(type_z *)pz",
            substrs = ["Cannot access memory at address 0x0"], error = True)


if __name__ == '__main__':
    import atexit
    lldb.SBDebugger.Initialize()
    atexit.register(lambda: lldb.SBDebugger.Terminate())
    unittest2.main()
