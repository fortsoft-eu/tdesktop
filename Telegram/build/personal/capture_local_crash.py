import ctypes
from ctypes import wintypes
from datetime import datetime
import json
from pathlib import Path
import shutil
import subprocess


kernel = ctypes.WinDLL('kernel32', use_last_error=True)
dbghelp = ctypes.WinDLL('dbghelp', use_last_error=True)
DWORD = wintypes.DWORD
HANDLE = wintypes.HANDLE
POINTER = ctypes.c_void_p
ULONG64 = ctypes.c_ulonglong


class StartupInfo(ctypes.Structure):
    _fields_ = [
        ('cb', DWORD), ('reserved', wintypes.LPWSTR),
        ('desktop', wintypes.LPWSTR), ('title', wintypes.LPWSTR),
        ('x', DWORD), ('y', DWORD), ('width', DWORD), ('height', DWORD),
        ('count_x', DWORD), ('count_y', DWORD), ('fill', DWORD),
        ('flags', DWORD), ('show', wintypes.WORD), ('reserved_size', wintypes.WORD),
        ('reserved_ptr', POINTER), ('stdin', HANDLE), ('stdout', HANDLE), ('stderr', HANDLE),
    ]


class ProcessInfo(ctypes.Structure):
    _fields_ = [('process', HANDLE), ('thread', HANDLE), ('pid', DWORD), ('tid', DWORD)]


class ExceptionRecord(ctypes.Structure):
    _fields_ = [
        ('code', DWORD), ('flags', DWORD), ('record', POINTER), ('address', POINTER),
        ('count', DWORD), ('information', ULONG64 * 15),
    ]


class ExceptionInfo(ctypes.Structure):
    _fields_ = [('record', ExceptionRecord), ('first_chance', DWORD)]


class EventData(ctypes.Union):
    _fields_ = [('exception', ExceptionInfo), ('raw', ULONG64 * 20)]


class DebugEvent(ctypes.Structure):
    _fields_ = [('code', DWORD), ('pid', DWORD), ('tid', DWORD), ('data', EventData)]


class ExceptionPointers(ctypes.Structure):
    _fields_ = [('record', POINTER), ('context', POINTER)]


class DumpExceptionInfo(ctypes.Structure):
    _pack_ = 4
    _fields_ = [('thread', DWORD), ('pointers', POINTER), ('client_pointers', wintypes.BOOL)]


kernel.CreateProcessW.argtypes = [wintypes.LPCWSTR, wintypes.LPWSTR, POINTER, POINTER,
    wintypes.BOOL, DWORD, POINTER, wintypes.LPCWSTR,
    ctypes.POINTER(StartupInfo), ctypes.POINTER(ProcessInfo)]
kernel.WaitForDebugEvent.argtypes = [ctypes.POINTER(DebugEvent), DWORD]
kernel.ContinueDebugEvent.argtypes = [DWORD, DWORD, DWORD]
kernel.CloseHandle.argtypes = [HANDLE]
kernel.OpenThread.argtypes = [DWORD, wintypes.BOOL, DWORD]
kernel.OpenThread.restype = HANDLE
kernel.GetThreadContext.argtypes = [HANDLE, POINTER]
kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, DWORD, DWORD, POINTER, DWORD, DWORD, HANDLE]
kernel.CreateFileW.restype = HANDLE
kernel.DebugSetProcessKillOnExit.argtypes = [wintypes.BOOL]
kernel.DebugActiveProcessStop.argtypes = [DWORD]
dbghelp.MiniDumpWriteDump.argtypes = [HANDLE, DWORD, HANDLE, DWORD,
    ctypes.POINTER(DumpExceptionInfo), POINTER, POINTER]


def check(value):
    if not value:
        raise ctypes.WinError(ctypes.get_last_error())
    return value


def write_dump(process, event, destination):
    thread = check(kernel.OpenThread(0x0048, False, event.tid))
    storage = ctypes.create_string_buffer(1232 + 15)
    context = (ctypes.addressof(storage) + 15) & ~15
    ctypes.c_uint32.from_address(context + 48).value = 0x10001F
    try:
        check(kernel.GetThreadContext(thread, context))
    finally:
        kernel.CloseHandle(thread)
    pointers = ExceptionPointers(ctypes.addressof(event.data.exception.record), context)
    exception = DumpExceptionInfo(event.tid, ctypes.addressof(pointers), False)
    output = kernel.CreateFileW(str(destination), 0x40000000, 0, None, 1, 0x80, None)
    if output == ctypes.c_void_p(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        check(dbghelp.MiniDumpWriteDump(process, event.pid, output, 0x21822,
            ctypes.byref(exception), None, None))
    finally:
        kernel.CloseHandle(output)


def monitor(executable, arguments, working_directory, output_root):
    if ctypes.sizeof(POINTER) != 8:
        raise RuntimeError('This launcher requires 64-bit Python.')
    stamp = executable.stat().st_mtime_ns
    pdb = executable.with_suffix('.pdb')
    pdb_stamp = pdb.stat().st_mtime_ns if pdb.is_file() else None
    command = ctypes.create_unicode_buffer(subprocess.list2cmdline([str(executable), *arguments]))
    startup = StartupInfo()
    startup.cb = ctypes.sizeof(startup)
    info = ProcessInfo()
    check(kernel.CreateProcessW(str(executable), command, None, None, False,
        0x00000002, None, str(working_directory), ctypes.byref(startup), ctypes.byref(info)))
    kernel.CloseHandle(info.thread)
    check(kernel.DebugSetProcessKillOnExit(False))
    active = True
    first_breakpoint = True
    evidence = None
    try:
        while active:
            event = DebugEvent()
            if not kernel.WaitForDebugEvent(ctypes.byref(event), 1000):
                error = ctypes.get_last_error()
                if error == 121:
                    continue
                raise ctypes.WinError(error)
            status = 0x00010002
            try:
                if event.code == 1:
                    exception = event.data.exception
                    if first_breakpoint and exception.first_chance and exception.record.code == 0x80000003:
                        first_breakpoint = False
                    else:
                        status = 0x80010001
                        if not exception.first_chance and evidence is None:
                            evidence = output_root / (datetime.now().strftime('%Y%m%d-%H%M%S') + '-' + str(info.pid))
                            evidence.mkdir(parents=True)
                            record = {
                                'executable': str(executable), 'pid': info.pid, 'thread': event.tid,
                                'exception': hex(exception.record.code),
                                'address': hex(exception.record.address or 0),
                                'parameters': [hex(exception.record.information[i]) for i in range(min(exception.record.count, 15))],
                                'contains_private_memory': True,
                                'uploaded': False,
                            }
                            try:
                                write_dump(info.process, event, evidence / 'Telegram-full.dmp')
                                record['dump_complete'] = True
                            except OSError as error:
                                record['dump_error'] = str(error)
                            (evidence / 'crash.json').write_text(json.dumps(record, indent=2), encoding='utf-8')
                elif event.code == 2:
                    kernel.CloseHandle(event.data.raw[0])
                elif event.code == 3:
                    for handle in event.data.raw[:3]:
                        if handle:
                            kernel.CloseHandle(handle)
                elif event.code == 6:
                    if event.data.raw[0]:
                        kernel.CloseHandle(event.data.raw[0])
                elif event.code == 5:
                    active = False
            finally:
                check(kernel.ContinueDebugEvent(event.pid, event.tid, status))
    finally:
        if active:
            kernel.DebugActiveProcessStop(info.pid)
        kernel.CloseHandle(info.process)
    if evidence:
        record['executable_mtime_ns'] = stamp
        record['symbols_mtime_ns'] = pdb_stamp
        unchanged = executable.stat().st_mtime_ns == stamp
        if pdb_stamp is not None:
            unchanged = unchanged and pdb.is_file() and pdb.stat().st_mtime_ns == pdb_stamp
        record['matching_binaries_copied'] = unchanged
        if unchanged:
            shutil.copy2(executable, evidence / executable.name)
            if pdb_stamp is not None:
                shutil.copy2(pdb, evidence / pdb.name)
        (evidence / 'crash.json').write_text(json.dumps(record, indent=2), encoding='utf-8')
    return evidence


if __name__ == '__main__':
    repo = Path(__file__).resolve().parents[3]
    profile = repo / '.local-data'
    output = profile / 'crash-diagnostics'
    output.mkdir(parents=True, exist_ok=True)
    try:
        monitor(repo / 'out/Debug/Telegram.exe', ['-many', '-noupdate', '-workdir', str(profile)], profile, output)
    except Exception as error:
        (output / 'monitor-error.txt').write_text(str(error), encoding='utf-8')
        raise
