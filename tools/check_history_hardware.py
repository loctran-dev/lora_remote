"""Explicit OFF-only end-to-end history burst test. Start on Control, motor OFF.

Never sends ON or changes settings. Three separate OFF commands are real actions
whose history must be persisted and ACKed FIFO, with >=10 s send/retry spacing.
Do not open another COM6/COM7 process while this test is running.
"""
import argparse
import re
import time
import serial
from board_console import command,button

def fields(line):
    print(line,flush=True)
    return {k:int(v) for k,v in re.findall(r"(\w+)=(\d+)",line)}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run",action="store_true",required=True)
    parser.parse_args()
    with serial.Serial("COM6",115200,timeout=4,write_timeout=3) as remote, serial.Serial("COM7",115200,timeout=4,write_timeout=3) as slave:
        assert "PWR=2 " in command(slave,"STATUS"),"Start OFF"
        before_config=command(remote,"CONFIG")
        before=fields(command(remote,"HISTORY"))
        old=fields(command(slave,"HISTORY"))
        assert old["PENDING"]==0,"Finish existing outbox before this bounded test"
        # USB BTN confirms dispatch, not RF delivery. Generate three ACTUAL slave
        # records, waiting for the durable LAST ID before sending the next command.
        last_id=old["LAST"]
        for _ in range(3):
            delivered=False
            for attempt in range(3):
                print(button(remote,"OFF","LONG","CONTROL"),flush=True)
                deadline=time.monotonic()+2
                while time.monotonic()<deadline:
                    received=fields(command(slave,"HISTORY"))
                    if received["LAST"]>last_id:
                        assert received["LAST"]==last_id+1,"Unexpected extra action"
                        last_id=received["LAST"];delivered=True;break
                    time.sleep(0.2)
                if delivered:break
            assert delivered,"OFF control command did not reach slave; history sync not yet tested"
        start=time.monotonic();last_sent=old["SENT"];observed=[]
        while time.monotonic()-start<45:
            state=fields(command(slave,"HISTORY"))
            cache=fields(command(remote,"HISTORY"))
            if state["SENT"]!=last_sent:
                observed.append((round(time.monotonic()-start,2),state["SENT"]))
                last_sent=state["SENT"]
            assert state["DROP"]==old["DROP"] and state["ERR"]==0
            if state["PENDING"]==0 and cache["STORED"]-before["STORED"]==3:break
            time.sleep(1)
        else:raise AssertionError("Three events did not finish syncing in 45 seconds")
        assert state["ACK"]-old["ACK"]==3
        assert cache["STORED"]-before["STORED"]==3,"Duplicate/lost records"
        assert before_config==command(remote,"CONFIG")
        assert "PWR=2 " in command(slave,"STATUS"),"Must stay OFF"
        print("Observed TX count changes (seconds, total):",observed,flush=True)
        print("PASS three OFF events stored once and ACKed; outbox empty; motor/config unchanged",flush=True)

if __name__=="__main__":main()
