"""Execute all built UI variants with display I/O mocked. Never connects boards."""
import re
import struct
import subprocess
import unittest
import test_visibility as h
from unicorn import UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_SP,UC_ARM_REG_PC,UC_ARM_REG_LR

class UiConsistencyTests(h.FirmwareHarness):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        dwarf=subprocess.check_output([str(h.FROMELF),'--text','-g',str(h.BUILD/'USB_Application.axf')],text=True)
        cls.members={}
        for name in ['batteryUsage','batteryVoltage','isChanging','time','date','currentTime','findDevicefactoryInfo']:
            cls.members[name]=max(map(int,re.findall(r'DW_AT_name '+name+r'\s+[^\n]*\n[^\n]*DW_AT_data_member_location[^\n]*DW_OP_plus_uconst (\d+)',dwarf)))
    def setUp(self):
        super().setUp()
        # The lightweight base harness skips C runtime startup. Float printf
        # requires the same numeric locale pointer initialized by __rt_lib_init.
        locale=self.call('__rt_locale')
        numeric=self.call('_get_lc_numeric')
        self.cpu.mem_write(locale+12,struct.pack('<I',numeric))
        self.add(0);self.select(0)
        self.cpu.mem_write(self.device+1,b'\x01')
        self.cpu.mem_write(self.device+4,struct.pack('<IBBB',0xabcdef01,1,ord('M'),1))
        self.cpu.mem_write(self.device+16,struct.pack('<i',-120))
        self.cpu.mem_write(self.device+36+40,b'Pump very long name\0')
        self.cpu.mem_write(self.system+self.members['time'],bytes([23,58,59]))
        self.cpu.mem_write(self.system+self.members['date'],bytes([4,11,19,26]))
        self.cpu.mem_write(self.system+self.members['currentTime'],struct.pack('<I',1795132739))
        self.cpu.mem_write(self.system+self.members['batteryVoltage'],struct.pack('<f',4.1))
        self.put(self.system+self.members['batteryUsage'],100)
        self.tick=0;self.rect={};self.text={};self.scroll={};self.align={};self.color={};self.created=[];self.frames=[]
        files=list((h.ROOT/'Application/Library').glob('screen_*.c'))+[h.ROOT/'Application/Library/screens.c']
        names=set(re.findall(r'\b(_?UG_\w+)\s*\(',''.join(f.read_text() for f in files)))
        names.update(['Rtc_Read','Main_SetBuzzerMode','Screen_EditCharactersCreate','Screen_EditCharactersUpdate','HAL_GetTick'])
        hooks={}
        for name in names:
            try:hooks[h.symbol(name)[0]&~1]=name
            except AssertionError:pass  # Removed unused rendering APIs need no mock.
        def intercept(cpu,address,size,data):
            if address not in hooks:return
            name=hooks[address];args=[cpu.reg_read(r) for r in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)]
            result=0
            if name=='UG_WindowCreate':
                self.rect={};self.text={};self.scroll={};self.align={};self.color={};self.created=[];self.frames=[]
            if name in ['UG_TextboxCreate','UG_ImageCreate','UG_RealEditCreate']:
                extra=struct.unpack('<3I',bytes(cpu.mem_read(cpu.reg_read(UC_ARM_REG_SP),12)))
                key=(name,args[2]);self.created.append(key)
                self.rect[key]=(args[3],*extra)
            if name=='UG_TextboxSetText':self.text[args[1]]=bytes(cpu.mem_read(args[2],96)).split(b'\0')[0].decode()
            if name=='UG_TextboxSetScrollEnabled':self.scroll[args[1]]=args[2]
            if name=='UG_TextboxSetAlignment':self.align[args[1]]=args[2]
            if name=='UG_TextboxSetForeColor':self.color[args[1]]=args[2]
            if name=='UG_ImageSetBMP':self.frames.append(args[2])
            if name=='UG_TextboxCheckVisible':result=1
            if name=='HAL_GetTick':result=self.tick
            cpu.reg_write(UC_ARM_REG_R0,result);cpu.reg_write(UC_ARM_REG_PC,cpu.reg_read(UC_ARM_REG_LR))
        self.cpu.hook_add(UC_HOOK_CODE,intercept)
        # Model scatter-loaded bitmap pointer table for the common battery renderer.
        self.cpu.mem_write(h.symbol('batteryGIF')[0],struct.pack('<5I',*[h.symbol('bmp_battery_'+str(i))[0] for i in range(5)]))
        self.call('Link_Init',0)
    def call(self,name,argument=0,argument1=0,argument2=0):
        stop=0x0807fff0
        self.cpu.reg_write(UC_ARM_REG_SP,0x2000f000)
        self.cpu.reg_write(UC_ARM_REG_LR,stop|1)
        for reg,value in [(UC_ARM_REG_R0,argument),(UC_ARM_REG_R1,argument1),(UC_ARM_REG_R2,argument2)]:
            self.cpu.reg_write(reg,value)
        self.cpu.emu_start(h.symbol(name)[0]|1,stop,count=2000000)
        self.assertEqual(self.cpu.reg_read(UC_ARM_REG_PC),stop)
        return self.cpu.reg_read(UC_ARM_REG_R0)
    def header(self):
        self.assertEqual(self.rect[('UG_TextboxCreate',0)],(0,0,50,15))
        self.assertEqual(self.rect[('UG_TextboxCreate',1)],(104,0,127,15))
        self.assertEqual(self.rect[('UG_ImageCreate',0)],(87,3,101,11))
        self.assertEqual((self.text[0],self.text[1]),('23:58','100%'))
        self.assertEqual((self.scroll[0],self.scroll[1]),(0,0))
        self.assertNotEqual(self.align[0],self.align[1])
    def layout(self):
        self.assertEqual(len(self.created),len(set(self.created)),'duplicate type/id in the same window')
        self.assertLessEqual(len(self.created),30)
        for rect in self.rect.values():
            x,y,xe,ye=rect
            self.assertTrue(0<=x<=xe<128 and 0<=y<=ye<160,rect)
    def test_every_screen_uses_identical_live_clock_and_battery_header(self):
        for init,render in [('MMS_Init','MMS_Render'),('GSS_Init','GSS_Render'),('screen_dss.o:init','screen_dss.o:render'),
                            ('DMS_Init','DMS_Render'),('DTS_Init','DTS_Render'),('HS_Init','HS_Render'),
                            ('managerInit','managerRender'),('Screen_EmptyControlInit','Screen_EmptyControlRender'),
                            ('DCS_Init','DCS_Render')]:
            with self.subTest(screen=init):
                self.call(init);self.call(render);self.header();self.layout()
    def test_main_menu_and_pairing_clock_advances_without_reentering_screen(self):
        for init,render in [('MMS_Init','MMS_Render'),('DPS_Init','DPS_Render')]:
            self.call(init)
            self.cpu.mem_write(self.system+self.members['time'],bytes([0,1,0]))
            self.call(render);self.assertEqual(self.text[0],'00:01')
            self.cpu.mem_write(self.system+self.members['time'],bytes([23,58,59]))
    def test_all_general_editors_share_header_and_help_fits_without_scrolling(self):
        for item in range(8):
            with self.subTest(item=item):
                self.put(h.symbol('indexOfGeneralSettingMenu')[0],item)
                self.call('EGSS_Init');self.call('EGSS_Render');self.header();self.layout()
                if item!=5:self.check_help()
    def test_all_device_editors_share_header_and_help_fits_without_scrolling(self):
        for item in range(7):
            with self.subTest(item=item):
                self.put(h.symbol('indexOfSettingMenu')[0],item)
                self.call('EDSS_Init');self.call('EDSS_Render');self.header();self.layout();self.check_help()
    def check_help(self):
        for id in (60,61):
            x,y,xe,ye=self.rect[('UG_TextboxCreate',id)]
            lines=self.text[id].split('\n')
            self.assertLessEqual(max(map(len,lines))*6,xe-x+1,self.text[id])
            self.assertLessEqual(len(lines)*8,ye-y+1)
            self.assertEqual(self.scroll[id],0)
    def test_both_timer_picker_and_custom_editor_use_the_same_header(self):
        for custom in (0,1):
            self.put(h.symbol('inScheduleSelection')[0],1)
            self.put(h.symbol('adjustingScheduleValue')[0],custom)
            self.call('DCS_Init');self.call('DCS_Render');self.header();self.layout()
    def test_pairing_states_have_no_duplicate_footer_or_overlapping_data_rows(self):
        for state in range(6):
            with self.subTest(state=state):
                self.put(h.symbol('pairingUI_Step')[0],state)
                self.cpu.mem_write(self.system+self.members['findDevicefactoryInfo'],struct.pack('<IBBB',0xabcdef01,1,ord('M'),1))
                self.call('DPS_Init');self.call('DPS_Render');self.header();self.layout()
                if state in [1,3,4,5]:
                    self.assertEqual(self.text[8],'0xABCDEF01')
                    for label,value in [(7,8),(9,10),(11,12)]:
                        self.assertLess(self.rect[('UG_TextboxCreate',label)][2],self.rect[('UG_TextboxCreate',value)][0])
    def test_monitor_full_serial_and_columns_fit(self):
        self.call('DMS_Init');self.call('DMS_Render')
        self.assertEqual(self.text[7],'0xABCDEF01')
        self.assertEqual(self.text[15],'1')
        for label in range(6,20,2):
            left=self.rect[('UG_TextboxCreate',label)];right=self.rect[('UG_TextboxCreate',label+1)]
            self.assertLess(left[2],right[0])
            self.assertLessEqual(len(self.text[label])*6,left[2]-left[0]+1)
            self.assertLessEqual(len(self.text[label+1])*6,right[2]-right[0]+1)
    def test_monitor_last_rx_uses_uptime_not_editable_wall_time(self):
        self.call('Link_ClockStep',61)
        self.call('Link_RecordPacket',0,1)
        self.call('Link_ClockStep',64)
        self.cpu.mem_write(self.system+self.members['currentTime'],struct.pack('<I',946684800))
        self.call('DMS_Render');self.assertEqual(self.text[19],'3 s')
    def test_device_manager_pages_do_not_overlap_header_or_help(self):
        for id in range(20):self.add(id)
        self.call('managerInit');self.call('managerRender');self.header();self.layout()
        for id in (6,8,10,12):
            rect=self.rect[('UG_TextboxCreate',id)]
            self.assertGreater(rect[1],37);self.assertLess(rect[3],130)
        self.assertIn('1-4/20',self.text[20])
        self.put(h.symbol('managerSelected')[0],19)
        self.call('managerRender');self.assertIn('17-20/20',self.text[20])
        self.assertEqual(self.text[1],'100%')
    def test_empty_battery_uses_frame_zero_and_unknown_voltage_is_not_100_percent(self):
        self.call('Screen_StatusBarCreate')
        self.assertEqual(self.frames[-1],h.symbol('bmp_battery_4')[0])
        self.put(self.system+self.members['batteryUsage'],0);self.call('Screen_StatusBarUpdate')
        self.assertEqual(self.frames[-1],h.symbol('bmp_battery_0')[0])
        self.assertEqual(self.text[1],'0%')
        self.cpu.mem_write(self.system+self.members['batteryVoltage'],struct.pack('<f',0))
        self.call('Screen_StatusBarUpdate');self.assertEqual(self.text[1],'--%')

if __name__=='__main__':unittest.main(verbosity=2)
