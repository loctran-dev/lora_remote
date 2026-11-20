"""Derive isolated MDK projects from the application snapshot; no board writes."""
from pathlib import Path
import copy
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent
APP = ROOT / "Application"


def configure(tree, name, start, size, scatter):
    target = tree.find("./Targets/Target")
    target.find("TargetName").text = name
    common = target.find("TargetOption/TargetCommonOption")
    common.find("OutputName").text = name
    common.find("OutputDirectory").text = name + "\\"
    common.find("Cpu").text = (
        f"IRAM(0x20000000-0x2000BFFF) IROM(0x{start:X}-0x{start+size-1:X}) "
        'CLOCK(8000000) CPUTYPE("Cortex-M3") TZ')
    for memory in target.findall(".//OnChipMemories/IROM") + target.findall(".//OnChipMemories/OCR_RVCT4"):
        memory.find("StartAddress").text = hex(start)
        memory.find("Size").text = hex(size)
    linker = target.find(".//LDads")
    if linker is None:
        linker = target.find(".//LDads")
    linker.find("useFile").text = "1"
    linker.find("ScatterFile").text = scatter
    for hook in common.findall(".//RunUserProg1") + common.findall(".//RunUserProg2"):
        hook.text = "0"
    return target


def main():
    path = APP / "MDK-ARM/LoRaBasedWPC.uvprojx"
    original = ET.parse(path)
    app_tree = copy.deepcopy(original)
    configure(app_tree, "USB_Application", 0x08008000, 0x36800, ".\\application.sct")
    ET.indent(app_tree)
    app_tree.write(APP / "MDK-ARM/USB_Application.uvprojx", encoding="utf-8", xml_declaration=True)
    boot = copy.deepcopy(original)
    target = configure(boot, "USB_Bootloader", 0x08000000, 0x8000, ".\\bootloader.sct")
    groups = target.find("Groups")
    groups.clear()
    files = ["../Src/boot_main.c", "../Src/boot_protocol.c",
             "../../Application/MDK-ARM/startup_stm32f103xe.s",
             "../../Application/Core/Src/system_stm32f1xx.c"]
    hal = "../../Application/Drivers/STM32F1xx_HAL_Driver/Src/"
    for name in ["hal", "hal_cortex", "hal_gpio", "hal_rcc", "hal_rcc_ex", "hal_pwr",
                 "hal_flash", "hal_flash_ex", "hal_pcd", "hal_pcd_ex", "ll_usb"]:
        files.append(hal + "stm32f1xx_" + name + ".c")
    for name in ["usb_device.c", "usbd_desc.c"]:
        files.append("../../Application/USB_DEVICE/App/" + name)
    files.append("../../Application/USB_DEVICE/Target/usbd_conf.c")
    for name in ["usbd_core.c", "usbd_ctlreq.c", "usbd_ioreq.c"]:
        files.append("../../Application/Middlewares/ST/STM32_USB_Device_Library/Core/Src/" + name)
    files.append("../../Application/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/usbd_cdc.c")
    group = ET.SubElement(groups, "Group")
    ET.SubElement(group, "GroupName").text = "USB Bootloader"
    entries = ET.SubElement(group, "Files")
    for file_path in files:
        entry = ET.SubElement(entries, "File")
        ET.SubElement(entry, "FileName").text = Path(file_path).name
        ET.SubElement(entry, "FileType").text = "2" if file_path.endswith(".s") else "1"
        ET.SubElement(entry, "FilePath").text = file_path
    compiler = target.find(".//Cads/VariousControls")
    compiler.find("Define").text = "USE_HAL_DRIVER,STM32F103xE"
    compiler.find("IncludePath").text = ";".join([
        "../Inc", "../../Application/Core/Inc", "../../Application/Drivers/STM32F1xx_HAL_Driver/Inc",
        "../../Application/Drivers/STM32F1xx_HAL_Driver/Inc/Legacy",
        "../../Application/Drivers/CMSIS/Include",
        "../../Application/Drivers/CMSIS/Device/ST/STM32F1xx/Include",
        "../../Application/USB_DEVICE/App", "../../Application/USB_DEVICE/Target",
        "../../Application/Middlewares/ST/STM32_USB_Device_Library/Core/Inc",
        "../../Application/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc"])
    ET.indent(boot)
    destination = ROOT / "Bootloader/MDK-ARM/USB_Bootloader.uvprojx"
    destination.parent.mkdir(parents=True, exist_ok=True)
    boot.write(destination, encoding="utf-8", xml_declaration=True)
    print("Prepared USB_Application and USB_Bootloader projects")


if __name__ == "__main__":
    main()
