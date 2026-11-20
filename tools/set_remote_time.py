"""Read/set remote RTC using PC civil time (default fixed UTC+07:00).

Wire timestamps follow the existing firmware convention: RTC calendar interpreted
as UTC, WITHOUT another timezone conversion on the board. --set is explicit.
No motor command, Flash settings write or history rewrite. A successful set on
new firmware schedules unicast status requests carrying time to paired slaves.
"""
import argparse
import datetime
import re
import time
import serial

def command(port,text):
    port.reset_input_buffer()
    port.write((text+"\n").encode("ascii"))
    line=port.readline().decode().strip()
    if not line or line.startswith("ERR"):raise RuntimeError(f"{text}: {line!r}")
    print(line,flush=True)
    return line

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port",default="COM6")
    parser.add_argument("--utc-offset",type=int,default=7,choices=range(-12,15))
    parser.add_argument("--set",action="store_true",help="Actually set RTC; default only reads")
    args=parser.parse_args()
    offset=args.utc_offset*3600
    with serial.Serial(args.port,115200,timeout=3,write_timeout=3) as port:
        command(port,"TIME")
        if args.set:command(port,f"TIME {int(time.time()+offset)}")
        line=command(port,"TIME")
    match=re.search(r"TS=(\d+)",line)
    if not match:raise RuntimeError("Missing TIME timestamp")
    board=int(match[1])
    error=board-int(time.time()+offset)
    print("PC local:",datetime.datetime.fromtimestamp(time.time()+offset,datetime.timezone.utc).strftime("%Y-%m-%d %H:%M:%S"),
          f"UTC{args.utc_offset:+03d}:00",f"RTC error={error:+d}s",flush=True)
    if args.set and abs(error)>2:raise RuntimeError("RTC did not match PC time within 2 seconds")

if __name__=="__main__":main()
