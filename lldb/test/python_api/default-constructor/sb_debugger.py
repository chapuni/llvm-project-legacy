"""
Fuzz tests an object after the default construction to make sure it does not crash lldb.
"""

import sys
import lldb

def fuzz_obj(obj):
    obj.SetAsync(True)
    obj.SetAsync(False)
    obj.GetAsync()
    obj.SkipLLDBInitFiles(True)
    obj.SetInputFileHandle(None, True)
    obj.SetOutputFileHandle(None, True)
    obj.SetErrorFileHandle(None, True)
    obj.GetInputFileHandle()
    obj.GetOutputFileHandle()
    obj.GetErrorFileHandle()
    obj.GetCommandInterpreter()
    obj.HandleCommand("nothing here")
    listener = obj.GetListener()
    obj.HandleProcessEvent(lldb.SBProcess(), lldb.SBEvent(), None, None)
    obj.CreateTargetWithFileAndTargetTriple("a.out", "A-B-C")
    obj.CreateTargetWithFileAndArch("b.out", "arm")
    obj.CreateTarget("c.out")
    obj.DeleteTarget(lldb.SBTarget())
    obj.GetTargetAtIndex(0xffffffff)
    obj.FindTargetWithProcessID(0)
    obj.FindTargetWithFileAndArch("a.out", "arm")
    obj.GetNumTargets()
    obj.GetSelectedTarget()
    obj.GetSourceManager()
    obj.SetSelectedTarget(lldb.SBTarget())
    obj.SetCurrentPlatformSDKRoot("tmp/sdk-root")
    try:
        obj.DispatchInput(None)
    except Exception:
        pass
    obj.DispatchInputInterrupt()
    obj.DispatchInputEndOfFile()
    obj.PushInputReader(lldb.SBInputReader())
    obj.NotifyTopInputReader(lldb.eInputReaderActivate)
    obj.InputReaderIsTopReader(lldb.SBInputReader())
    obj.GetInstanceName()
    obj.GetDescription(lldb.SBStream())
    obj.GetTerminalWidth()
    obj.SetTerminalWidth(0xffffffff)
    obj.GetID()
    obj.GetPrompt()
    obj.SetPrompt("Hi, Mom!")
    obj.GetScriptLanguage()
    obj.SetScriptLanguage(lldb.eScriptLanguageNone)
    obj.SetScriptLanguage(lldb.eScriptLanguagePython)
    obj.GetCloseInputOnEOF()
    obj.SetCloseInputOnEOF(True)
    obj.SetCloseInputOnEOF(False)
    obj.Clear()
    for target in obj:
        print target
