"""Calendar used by history timestamps, independent of host timezone."""
import datetime
import unittest
import test_visibility as h

class CalendarTests(h.FirmwareHarness):
    def test_history_dates_and_times_roundtrip(self):
        for date in [datetime.datetime(2000,1,1),datetime.datetime(2000,2,29,23,59,59),
                     datetime.datetime(2026,11,18,14,3,27),datetime.datetime(2028,2,29,1,2,3),
                     datetime.datetime(2099,12,31,23,59,59)]:
            stamp=int(date.replace(tzinfo=datetime.timezone.utc).timestamp())
            self.cpu.mem_write(0x20008000,bytes(24));self.cpu.mem_write(0x20008100,bytes(4))
            self.call("convertTimestampToRTC",stamp,0x20008000,0x20008100)
            time=bytes(self.cpu.mem_read(0x20008000,3))
            rtc_date=bytes(self.cpu.mem_read(0x20008100,4))
            self.assertEqual(time,bytes([date.hour,date.minute,date.second]))
            self.assertEqual(rtc_date[1:],bytes([date.month,date.day,date.year-2000]))
            self.assertEqual(self.call("getUnixTimestamp",0x20008000,0x20008100),stamp)
    def test_out_of_calendar_timestamp_is_clamped(self):
        self.call("convertTimestampToRTC",0,0x20008000,0x20008100)
        self.assertEqual(bytes(self.cpu.mem_read(0x20008101,3)),bytes([1,1,0]))

if __name__=="__main__":unittest.main(verbosity=2)
