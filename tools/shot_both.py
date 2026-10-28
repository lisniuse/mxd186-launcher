# -*- coding: utf-8 -*-
"""截两张图：主窗 + 第二窗体（asInvoker 版）"""
import ctypes
import subprocess
import time

from PIL import ImageGrab

ROOT = r"D:\dgames\MXD186NEW\Mxd186Launcher"
EXE = ROOT + r"\_shot_test.exe"
user32 = ctypes.windll.user32


class RECT(ctypes.Structure):
    _fields_ = [("left", ctypes.c_long), ("top", ctypes.c_long),
                ("right", ctypes.c_long), ("bottom", ctypes.c_long)]


def shot(class_name, out):
    hwnd = user32.FindWindowW(class_name, None)
    if not hwnd:
        print("找不到窗口", class_name)
        return
    r = RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(r))
    w, h = r.right - r.left, r.bottom - r.top
    ImageGrab.grab(all_screens=True).crop((r.left, r.top, r.right, r.bottom)).save(out)
    print("saved", out, w, "x", h)


subprocess.Popen([EXE, "/dlg"], cwd=ROOT)
time.sleep(2.5)
shot("Mxd186LoginDlg", ROOT + r"\tools\shot_dlg_brutal.png")
subprocess.run(["taskkill", "/f", "/im", "_shot_test.exe"], capture_output=True)
time.sleep(0.8)

subprocess.Popen([EXE], cwd=ROOT)
time.sleep(2.5)
shot("Mxd186LauncherClass", ROOT + r"\tools\shot_main_brutal.png")
subprocess.run(["taskkill", "/f", "/im", "_shot_test.exe"], capture_output=True)
