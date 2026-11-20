"""Explicit native UI audit from CONTROL; no Save, pairing, hide or power actions.

--run required. Captures are optional; each is a real LCD read over USB.
Ends on MENU, with CONFIG and timer/power unchanged.
"""
import argparse
import re
import subprocess
import sys
import time
from pathlib import Path
import serial
from board_console import command,button

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run',action='store_true',required=True)
    parser.add_argument('--port',default='COM6')
    parser.add_argument('--capture-dir',type=Path)
    args=parser.parse_args()
    def query(text):
        with serial.Serial(args.port,115200,timeout=4,write_timeout=3) as port:return command(port,text)
    def key(name,action='CLICK',screen=None):
        with serial.Serial(args.port,115200,timeout=4,write_timeout=3) as port:
            return button(port,name,action,screen)
    def shot(name):
        print(name,query('TIME'),flush=True)
        if args.capture_dir:
            args.capture_dir.mkdir(parents=True,exist_ok=True)
            subprocess.run([sys.executable,str(Path(__file__).with_name('capture_screen.py')),
                            str(args.capture_dir/(name+'.bmp')),'--port',args.port],check=True,timeout=30)
    before=query('CONFIG');timer_before=query('TIMER')
    assert 'SCREEN=CONTROL' in key('OFF'), 'Start this audit from normal CONTROL, not a timer editor'
    print(before,timer_before,sep='\n',flush=True)
    key('LEFT','LONG','MONITOR');shot('monitor');key('RIGHT','LONG','CONTROL')
    key('CENTER',screen='CONTROL');shot('timer-picker')
    key('CENTER',screen='CONTROL');shot('timer-custom');key('RIGHT','LONG','CONTROL')
    key('RIGHT','LONG','DEVICE_SETTING');shot('device-page1')
    for item in range(7):
        key('CENTER',screen='EDIT_DEVICE_SETTING')
        if item in (0,5,6):shot('device-editor-'+str(item))
        key('RIGHT','LONG','DEVICE_SETTING')
        if item<6:key('RIGHT',screen='DEVICE_SETTING')
    shot('device-page2');key('RIGHT','LONG','CONTROL')
    key('CENTER','LONG','MENU');shot('menu')
    key('CENTER',screen='DEVICE_MANAGER');shot('device-manager');key('RIGHT','LONG','MENU')
    for _ in range(2):key('RIGHT',screen='MENU')
    key('CENTER',screen='GENERAL_SETTING');shot('general-page1')
    for position in range(8):
        key('CENTER',screen='EDIT_GENERAL_SETTING')
        if position in (0,1,7):shot('general-editor-'+str(position))
        key('RIGHT','LONG','GENERAL_SETTING');key('RIGHT',screen='GENERAL_SETTING')
    shot('general-page3');key('CENTER',screen='DATE_TIME');shot('date-time')
    for _ in range(6):key('CENTER',screen='DATE_TIME')
    key('RIGHT','LONG','GENERAL_SETTING');key('RIGHT','LONG','MENU')
    for _ in range(4):key('RIGHT',screen='MENU')
    key('CENTER',screen='HISTORY');shot('history-picker')
    key('CENTER',screen='HISTORY');shot('history-list')
    key('CENTER',screen='HISTORY');shot('history-detail')
    for _ in range(3):key('RIGHT','LONG')
    shot('menu-final')
    after=query('CONFIG');timer_after=query('TIMER')
    assert before==after,(before,after)
    for field in ['PWR','DEADLINE']:
        pattern=field+r'=(\d+)'
        assert re.search(pattern,timer_before)[1]==re.search(pattern,timer_after)[1],(timer_before,timer_after)
    print(after,timer_after,query('HISTORY'),query('DIAG'),'PASS: UI audit, configuration/power/timer unchanged',sep='\n',flush=True)

if __name__=='__main__':main()
