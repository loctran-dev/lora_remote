"""Built ARM date/time editor: fields, calendar rules, save/cancel and layout."""
import datetime
import struct
import unittest
import test_general_menu as m
import test_visibility as h
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_PC,UC_ARM_REG_LR

class DateTimeUiTests(m.MenuHarness):
    def setUp(self):
        super().setUp()
        self.mocks(["Rtc_Read","UG_WindowCreate","UG_WindowSetStyle","UG_WindowSetTitleHeight",
                    "UG_WindowSetBackColor","UG_WindowSetXStart","UG_WindowSetYStart","UG_WindowSetXEnd",
                    "UG_WindowSetYEnd","UG_TextboxCreate","UG_TextboxSetFont","UG_TextboxSetText",
                    "UG_TextboxSetBackColor","UG_TextboxSetForeColor","UG_TextboxSetAlignment",
                    "UG_TextboxSetScrollEnabled","UG_WindowShow","UG_Update","Main_SetBuzzerMode",
                    "UG_WindowHide","UG_WindowDelete","GSS_Init","GSS_Render","Power_WakeForError",
                    "Screen_StatusBarCreate","Screen_StatusBarUpdate"])
        self.cpu.mem_write(self.system+28,bytes([12,34,56]))
        self.cpu.mem_write(self.system+31,bytes([4,11,19,26]))
        self.cpu.mem_write(h.symbol("currentScreen")[0],struct.pack("<I",h.symbol("dateTimeScreen")[0]))
        self.save_result=0;self.saved=[]
        def save(cpu,address,size,data):
            self.saved.append(cpu.reg_read(UC_ARM_REG_R0))
            cpu.reg_write(UC_ARM_REG_R0,self.save_result)
            cpu.reg_write(UC_ARM_REG_PC,cpu.reg_read(UC_ARM_REG_LR))
        address=h.symbol("Rtc_Set")[0]&~1
        self.cpu.hook_add(UC_HOOK_CODE,save,begin=address,end=address)
        self.cpu.mem_write(h.symbol("generalSettingScreen")[0]+4,
            struct.pack("<II",h.symbol("GSS_Init")[0]|1,h.symbol("GSS_Render")[0]|1))
        self.call("DTS_Init")
    def render(self):
        self.calls.clear();self.call("DTS_Render")
        return {c[1][1]:c[3][0] for c in self.calls if c[0]=="UG_TextboxSetText"}
    def field(self,index):
        # Deterministically navigate using public button callbacks only.
        self.call("DTS_Init")
        for _ in range(index):self.call("DTS_Next")
    def set_calendar(self,year,month,day):
        self.cpu.mem_write(h.symbol("editedDate")[0],bytes([1,month,day,year-2000]))
    def test_snapshot_has_separate_date_and_time_fields(self):
        text=self.render()
        self.assertEqual([text[i] for i in range(5,11)],["19","11","2026","12","34","56"])
    def test_next_cycles_six_fields_and_only_one_is_highlighted(self):
        self.render()
        colors=[c for c in self.calls if c[0]=="UG_TextboxSetBackColor" and 5<=c[1][1]<=10]
        selected_color=colors[0][1][2]
        for selected in [1,2,3,4,5,0]:
            self.call("DTS_Next");self.render()
            colors=[c for c in self.calls if c[0]=="UG_TextboxSetBackColor" and 5<=c[1][1]<=10]
            self.assertEqual([c[1][1] for c in colors if c[1][2]==selected_color],[5+selected])
    def test_month_change_clamps_day_to_valid_month(self):
        self.field(1);self.set_calendar(2026,1,31);self.call("DTS_Increment")
        text=self.render();self.assertEqual((text[5],text[6]),("28","02"))
    def test_leap_day_is_preserved_then_clamped_when_year_changes(self):
        self.field(2);self.set_calendar(2024,2,29);self.call("DTS_Increment")
        text=self.render();self.assertEqual((text[5],text[7]),("28","2025"))
    def test_day_and_time_fields_wrap_at_their_own_bounds(self):
        self.set_calendar(2024,2,29);self.call("DTS_Increment")
        self.assertEqual(self.render()[5],"01")
        self.call("DTS_Decrement");self.assertEqual(self.render()[5],"29")
        for index,bound,box in [(3,23,8),(4,59,9),(5,59,10)]:
            self.field(index)
            self.cpu.mem_write(h.symbol("editedTime")[0]+index-3,bytes([bound]))
            self.call("DTS_Increment");self.assertEqual(self.render()[box],"00")
            self.call("DTS_Decrement");self.assertEqual(self.render()[box],str(bound))
    def test_cancel_and_editing_never_call_rtc_setter(self):
        before=bytes(self.cpu.mem_read(self.system,128))
        self.call("DTS_Increment");self.call("DTS_Next");self.call("DTS_Decrement");self.call("DTS_Cancel")
        self.assertEqual(self.saved,[])
        self.assertEqual(bytes(self.cpu.mem_read(self.system,128)),before)
    def test_save_commits_the_snapshot_once_then_returns_to_general(self):
        self.call("DTS_Save")
        self.assertEqual(self.saved,[int(datetime.datetime(2026,11,19,12,34,56,tzinfo=datetime.timezone.utc).timestamp())])
        self.assertEqual(int.from_bytes(self.cpu.mem_read(h.symbol("currentScreen")[0],4),"little"),h.symbol("generalSettingScreen")[0])
    def test_timer_guard_keeps_editor_open_with_explanation(self):
        self.save_result=2;self.call("DTS_Save")
        self.assertFalse(any(c[0]=="UG_WindowDelete" for c in self.calls))
        self.assertEqual(self.render()[15],"Timer active: blocked")
    def test_link_warning_does_not_discard_clock_editor(self):
        self.add(0);self.call("DTS_Increment")
        self.call("Screen_ShowConnectionError",0)
        self.assertFalse(any(c[0]=="UG_WindowDelete" for c in self.calls))
        self.assertEqual(self.render()[5],"20")
    def test_fields_do_not_overlap_separators_and_do_not_scroll(self):
        rects={c[1][2]:(c[1][3],*c[2][:3]) for c in self.calls if c[0]=="UG_TextboxCreate"}
        for left,sep,right in [(5,11,6),(6,12,7),(8,13,9),(9,14,10)]:
            self.assertLess(rects[left][2],rects[sep][0]);self.assertLess(rects[sep][2],rects[right][0])
        self.assertTrue(all(c[1][2]==0 for c in self.calls if c[0]=="UG_TextboxSetScrollEnabled"))

if __name__=="__main__":unittest.main(verbosity=2)
