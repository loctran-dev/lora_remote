"""Native ARM history navigation with LCD primitives mocked; never connects a board."""
import struct
import unittest
import test_history as h
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_SP,UC_ARM_REG_PC,UC_ARM_REG_LR

class HistoryUiTests(unittest.TestCase):
    def setUp(self):
        self.hw=h.JournalTests();self.hw.setUp();self.cpu=self.hw.cpu;self.call=self.hw.call
        self.hw.globals_for_sync()
        self.text={};self.bg={};self.fg={};self.visible={};self.beeps=[]
        self.cpu.mem_write(h.symbol("mainMenuScreen")+4,struct.pack("<II",h.symbol("screen_mms.o:init")|1,h.symbol("screen_mms.o:render")|1))
        self.cpu.mem_write(h.symbol("deviceControlScreen")+4,struct.pack("<II",h.symbol("screen_dcs.o:init")|1,h.symbol("screen_dcs.o:render")|1))
        names=["UG_WindowCreate","UG_WindowHide","UG_WindowDelete","UG_WindowSetStyle","UG_WindowSetTitleHeight",
               "UG_WindowSetBackColor","UG_WindowSetXStart","UG_WindowSetXEnd","UG_WindowSetYStart","UG_WindowSetYEnd",
               "UG_WindowShow","UG_TextboxCreate","UG_TextboxSetFont","UG_TextboxSetText","UG_TextboxSetBackColor",
               "UG_TextboxSetForeColor","UG_TextboxSetAlignment","UG_TextboxSetScrollEnabled","UG_TextboxCheckVisible",
               "UG_TextboxShow","UG_TextboxHide","UG_Update","screen_mms.o:init","screen_mms.o:render","screen_dcs.o:init","screen_dcs.o:render",
               "ScrnCommon_StatusBarCreate","ScrnCommon_StatusBarUpdate","Main_SetBuzzerMode"]
        hooks={h.symbol(n)&~1:n for n in names}
        def intercept(cpu,address,size,data):
            if address not in hooks:return
            n=hooks[address];a=[cpu.reg_read(r) for r in [UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3]];result=0
            if n=="UG_TextboxSetText":self.text[a[1]]=bytes(cpu.mem_read(a[2],80)).split(b"\0")[0].decode()
            if n=="UG_TextboxSetBackColor":self.bg[a[1]]=a[2]
            if n=="UG_TextboxSetForeColor":self.fg[a[1]]=a[2]
            if n=="UG_TextboxCheckVisible":result=int(self.visible.get(a[1],True))
            if n=="Main_SetBuzzerMode":self.beeps.append(a[0])
            if n=="UG_TextboxShow":self.visible[a[1]]=True
            if n=="UG_TextboxHide":self.visible[a[1]]=False
            cpu.reg_write(UC_ARM_REG_R0,result);cpu.reg_write(UC_ARM_REG_PC,cpu.reg_read(UC_ARM_REG_LR))
        self.cpu.hook_add(UC_HOOK_CODE,intercept)
    def test_every_history_button_beeps(self):
        for i in range(2):self.hw.append()
        self.open_list()
        for name in ["leftClicked","rightClicked","centerClicked","centerPressed","rightPressed"]:
            del self.beeps[:]
            self.call("screen_hs.o:"+name)
            self.assertEqual(len(self.beeps),1,name)
    def screen(self):
        return struct.unpack("<I",bytes(self.cpu.mem_read(h.symbol("currentScreen"),4)))[0]
    def test_history_from_device_info_returns_to_control(self):
        self.hw.append()
        self.assertEqual(self.call("ScrnHs_OpenDeviceHistory",0),1)
        self.assertEqual(self.text[5],"M11 History")
        self.call("screen_hs.o:centerClicked");self.call("screen_hs.o:rightPressed")
        self.assertEqual(self.screen(),h.symbol("historyScreen"))  # Detail -> list stays in history.
        self.call("screen_hs.o:rightPressed")
        self.assertEqual(self.screen(),h.symbol("deviceControlScreen"))
    def test_history_from_menu_still_returns_to_picker(self):
        self.hw.append();self.open_list()
        self.call("screen_hs.o:rightPressed")
        self.assertEqual(self.screen(),h.symbol("historyScreen"))
        self.assertEqual(self.text[5],"Select device")
    def test_list_colours_by_action(self):
        # RGB565: ON green, OFF chocolate brown, other actions white. Newest record is row 0.
        for action in (3,2,1):self.hw.append(action=action)
        self.open_list()
        self.assertEqual([self.fg[7],self.fg[9],self.fg[11]],[0x07E0,0xD344,0xFFFF])
    def open_list(self):
        self.call("ScrnHs_OpenHistory");self.call("screen_hs.o:centerClicked")
    def test_menu_picker_list_details_and_back_preserve_selection(self):
        for i in range(3):self.hw.append()
        self.open_list()
        self.assertEqual(self.text[6],"1/1")
        self.call("screen_hs.o:rightClicked")
        self.assertNotEqual(self.bg[9],self.bg[7])
        self.call("screen_hs.o:centerClicked")
        self.assertEqual(self.text[6],"2/3")
        self.assertEqual(self.text[8],"Action: ON")
        self.assertEqual(self.text[9],"Source: Remote")
        self.assertEqual(self.text[10],"Before: OFF")
        self.assertEqual(self.text[11],"After: ON")
        self.call("screen_hs.o:rightPressed");self.call("screen_hs.o:centerClicked")
        self.assertEqual(self.text[6],"2/3")
    def test_empty_history_does_not_open_bogus_detail(self):
        self.open_list();self.assertEqual(self.text[7],"No history yet")
        self.call("screen_hs.o:centerClicked")
        self.assertEqual(self.cpu.mem_read(h.symbol("historyScreenMode"),1),b"\x01")
    def test_hidden_device_is_still_selectable(self):
        self.cpu.mem_write(h.symbol("deviceBuff")+36+69,b"\x01")
        self.call("ScrnHs_OpenHistory")
        self.assertIn("[Hidden]",self.text[7])
        self.call("screen_hs.o:centerClicked");self.assertEqual(self.text[7],"No history yet")
    def test_new_records_do_not_move_frozen_selection_until_refresh(self):
        self.hw.append();self.open_list()
        self.hw.append()
        self.call("screen_hs.o:render")
        self.assertEqual(self.text[6],"1/1")
        self.call("screen_hs.o:centerPressed");self.call("screen_hs.o:centerClicked")
        self.assertEqual(self.text[6],"2/2")  # Refresh keeps the formerly selected event.
    def test_unknown_time_is_labeled_instead_of_fabricated_date(self):
        self.hw.append(flags=1);self.open_list();self.call("screen_hs.o:centerClicked")
        self.assertEqual(self.text[7],"Time not synced")

if __name__=="__main__":unittest.main(verbosity=2)
