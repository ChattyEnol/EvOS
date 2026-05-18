# EvOS Log

## Coding Habits

- The user prefers Chinese comments and concise, direct naming.
- Header files should keep only outward-facing interfaces and public structures.
- Source files should declare all internal functions first, then implement public interfaces, then internal functions.
- Functions and variables should have explanatory comments above them when quick understanding matters.
- Avoid touching PowerShell build scripts unless the source layout truly requires it.
- Preferred languages are C first, then C++.

## Current Project State

- Kernel entry initializes memory, graphics, interrupt/APIC, process table, file module, xHCI, legacy PS/2 keyboard, then starts Console.
- Interrupts use IDT stubs with APIC EOI centralized in `CommonInterruptHandler`.
- Local APIC supports x2APIC through MSR access and falls back to xAPIC MMIO.
- PCIe scanning currently uses legacy CF8/CFC config access.
- xHCI discovery finds class `0x0C/0x03/0x30`, maps BAR0, resets the controller, registers MSI on vector 34, and starts the controller.
- xHCI interrupt handling only acknowledges `USBSTS.EINT`; Event Ring parsing and HID report translation are still TODO.
- Legacy PS/2 keyboard remains present as a fallback on IRQ1/vector 33.
- FAT32 code is read-only and callback-based; it still needs a block device driver for the ESP.
- Process management is currently a fixed process table, not a scheduler.

## Debug Notes

- If xHCI reports ready but no keyboard input appears, the next likely missing piece is Event Ring setup/parsing and USB HID keyboard enumeration.
- If xHCI MSI never fires, inspect MSI capability programming, APIC destination ID, and whether Hyper-V exposes MSI/MSI-X differently.
- The command `info` prints interrupt and xHCI readiness; `xhci` prints controller identity when input works.
