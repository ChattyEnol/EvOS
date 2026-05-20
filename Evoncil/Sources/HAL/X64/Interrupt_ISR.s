# HAL/X64/Interrupt_ISR.s.s
#
# (C) Charity Enol
#
# 中断服务例程（ISR）汇编实现。
# 每个中断都有一个存根来保存寄存器状态并调用 C 语言处理程序。
#
# 中断部分栈的结构：
#
# +----------------------------+
# |           SS               |  <- 硬件自动压入。
# |           RSP              |  <- 硬件自动压入。
# |           RFLAGS           |  <- 硬件自动压入。
# |           CS               |  <- 硬件自动压入。
# |           RIP              |  <- 硬件自动压入。
# +----------------------------+
# |  Error Code (or manual 0)  |  <- 硬件自动/跳板手动压入。
# +----------------------------+
# |           vector           |  <- 跳板手动压入。
# +----------------------------+  <-- 此时刚进入 .common_handler。
# |            %RAX            |  \
# |            %RCX            |   |
# |            %...            |   |-- 后面通用的 15 个 push 压入。
# |            %R15            |   |
# +----------------------------+  /
# 低地址（栈顶 %rsp）
#
# 中断号定义位于 Intel 手册 3346 页。
# 又是那本长长的手册！
# [ 0...31]：CPU 定义。
# [32...47]：硬中断。
# [48..255]：软中断。
#
# Vector  Mnemonic    Description             Type        Error   Source
#                                                         Code
#
# 0       #DE         Divide Error            Fault       No      DIV and IDIV instructions.
# 1       #DB         Debug Exception         Fault/Trap  No      Instruction, data,
#                                                                 and I/O breakpoints;
#                                                                 single-step;
#                                                                 and others.
# 2       —           NMI Interrupt           Interrupt   No      Nonmaskable external interrupt.
# 3       #BP         Breakpoint              Trap        No      INT3 instruction.
# 4       #OF         Overflow                Trap        No      INTO instruction.
# 5       #BR         BOUND Range Exceeded    Fault       No      BOUND instruction.
# 6       #UD         Invalid Opcode          Fault       No      UD instruction
#                     (Undefined Opcode)                          or reserved opcode.
# 7       #NM         Device Not Available    Fault       No      Floating-point
#                     (No Math Coprocessor)                       or WAIT/FWAIT instruction.
# 8       #DF         Double Fault            Abort       Yes     Any instruction
#                                                         (zero)  that can generate an exception,
#                                                                 an NMI, or an INTR.
# 9                   Coprocessor Segment     Fault       No      Floating-point instruction. [1]
#                     Overrun (reserved)
# 10      #TS         Invalid TSS             Fault       Yes     Task switch or TSS access.
# 11      #NP         Segment Not Present     Fault       Yes     Loading segment registers
#                                                                 or accessing system segments.
# 12      #SS         Stack-Segment Fault     Fault       Yes     Stack operations
#                                                                 and SS register loads.
# 13      #GP         General Protection      Fault       Yes     Any memory reference 
#                                                                 and other protection checks.
# 14      #PF         Page Fault              Fault       Yes     Any memory reference.
# 15      -           Intel reserved.                     No
#                     Do not use.
# 16      #MF         x87 FPU Floating-Point  Fault       No      x87 FPU floating-point
#                     Error (Math Fault)                          or WAIT/FWAIT instruction.
# 17      #AC         Alignment Check         Fault       Yes     Any data reference in memory. [2]
# 18      #MC         Machine Check           Abort       No      Error codes (if any) [3]
#                                                                 and source are model dependent.
# 19      #XM         SIMD Floating-Point     Fault       No      SSE/SSE2/SSE3
#                     Exception                                   floating-point instructions. [4]
# 20      #VE         Virtualization          Fault       No      EPT violations. [5]
#                     Exception
# 21      #CP         Control Protection      Fault       Yes     RET, IRET, RSTORSSP, and SETSSBSY
#                                                                 instructions can generate
#                                                                 this exception.
#                                                                 When CET indirect branch tracking
#                                                                 is enabled, this exception
#                                                                 can be generated due to a missing
#                                                                 ENDBRANCH instruction at target
#                                                                 of an indirect call or jump.
# 22..31  —           Intel reserved.
#                     Do not use.
# 32..255 —           User Defined            Interrupt           External interrupt
#                     (Non-reserved)                              or INT n instruction.
#                     Interrupts
#
# NOTES:
#
# 1. Processors after the Intel386 processor do not generate this exception.
# 2. This exception was introduced in the Intel486 processor.
# 3. This exception was introduced in the Pentium processor and enhanced in the P6 family processors.
# 4. This exception was introduced in the Pentium III processor.
# 5. This exception can occur only on processors
#    that support the 1-setting of the "EPT-violation #VE" VM-execution control.

.section .text
.align 16

# 印刷不带 Error Code 的宏。
.macro ISR_NO_ERRORCODE vector
.global ISRStub\vector
.align 16
ISRStub\vector:
    push    $0          # 压入伪错误码，用来对齐。
    push    $\vector    # 压入硬编码的中断号。
    jmp     .common_handler
.endm

# 印刷带 Error Code 异常的宏。
.macro ISR_ERRORCODE vector
.global ISRStub\vector
.align 16
ISRStub\vector:
    # CPU 已经自动压入 Error Code 了。
    push    $\vector
    jmp     .common_handler
.endm

# 声明外部 C 函数。
.extern CommonInterruptHandler

# [0..31]
# CPU 中断。
ISR_NO_ERRORCODE 0
ISR_NO_ERRORCODE 1
ISR_NO_ERRORCODE 2
ISR_NO_ERRORCODE 3
ISR_NO_ERRORCODE 4
ISR_NO_ERRORCODE 5
ISR_NO_ERRORCODE 6
ISR_NO_ERRORCODE 7
ISR_ERRORCODE    8   # Double Fault.
ISR_NO_ERRORCODE 9
ISR_ERRORCODE    10  # Invalid TSS.
ISR_ERRORCODE    11  # Segment Not Present.
ISR_ERRORCODE    12  # Stack Fault.
ISR_ERRORCODE    13  # General Protection.
ISR_ERRORCODE    14  # Page Fault.
# 15
ISR_NO_ERRORCODE 16
ISR_ERRORCODE    17  # Alignment Check.
ISR_NO_ERRORCODE 18
ISR_NO_ERRORCODE 19
ISR_NO_ERRORCODE 20
ISR_ERRORCODE    21

# 硬中断。
ISR_NO_ERRORCODE 32  # 这啥中断啊？
ISR_NO_ERRORCODE 33  # 键盘。
ISR_NO_ERRORCODE 34  # xHCI.
ISR_NO_ERRORCODE 255 # APIC Spurious Interrupt Vector.

# 系统调用。
ISR_NO_ERRORCODE 128 # INT 0x80.

# 通用中断处理器。
.align 16
.common_handler:
    # 保存所有通用寄存器。
    push    %rax
    push    %rcx
    push    %rdx
    push    %rbx
    push    %rbp
    push    %rsi
    push    %rdi
    push    %r8
    push    %r9
    push    %r10
    push    %r11
    push    %r12
    push    %r13
    push    %r14
    push    %r15

    # 从栈上获取参数（中断号在 pushed 中的第二个位置，错误码在第三个位置）。
    mov     120(%rsp), %rcx      # 第一参数：中断号 `vector`。
    mov     128(%rsp), %rdx      # 第二参数：错误码。
    mov     %rsp, %r8            # 第三参数：保存后的完整中断帧。

    # 清空方向标志位。
    # cld

    # 关键一步：为 Windows x64 呼叫约定分配 32 字节的影子空间，且保持 16 字节对齐
    sub     $32, %rsp
    
    # 调用通用中断处理函数。
    call    CommonInterruptHandler
    
    # 呼叫完毕，回收影子空间
    add     $32, %rsp

    # 恢复所有通用寄存器。
    pop     %r15
    pop     %r14
    pop     %r13
    pop     %r12
    pop     %r11
    pop     %r10
    pop     %r9
    pop     %r8
    pop     %rdi
    pop     %rsi
    pop     %rbp
    pop     %rbx
    pop     %rdx
    pop     %rcx
    pop     %rax

    # 移除中断号和错误码（这由 ISR 例程推送）。
    add     $16, %rsp

    # 中断返回。
    iretq
