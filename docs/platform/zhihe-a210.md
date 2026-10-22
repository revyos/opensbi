# Zhihe A210

The single-die A210 has four C908 and four C920v2 application harts.
Melon Pi V1 and V2 share the A210 SODIMM module. Platform matching uses
the SoC compatible, independently of the carrier revision. The previous
boot stage initializes DRAM and hart 0, leaving the other seven harts
in reset. Build the generic RV64 platform with CONFIG_PLATFORM_ZHIHE_A210
and FW_DYNAMIC=y. Coreboot passes the next-stage entry and mode through
fw_dynamic_info and places OpenSBI in the reserved region at 0x80000000.

## Secondary boot and HSM

The A210 start-only HSM device programs and reads back the requested
hart's reset vector, attaches the C920 cluster on its first use and
releases only that core. A lock serializes shared reset and attachment
state. The reset entry configures caches/coherency without using a C
stack, then jumps to the M-mode address supplied by generic HSM.

The initial reset state is checked. The implementation does not blindly
reset cores that the previous stage has released. Attachment waits are
bounded; a timeout leaves core resets asserted and can be retried.

No hardware hart_stop or platform suspend callback is supplied. After a
hart has initialized once, generic HSM handles software offline/online
through warmboot parking and IPI wakeup, not hardware power gating.

## Hardware description

Use root compatible zhihe,a210 and hart IDs 0--7. The C900 PLIC driver
handles S-mode delegation via the thead,c900-plic fallback. Use its
two-cell peripheral interrupt binding. The T-Head CLINT fallback selects
32-bit MTIMER accesses and the time CSR.

No reset-sample node or DT register script is used. A210 hardware register
locations, reset bit meanings and vendor-derived cache settings are
documented in platform/generic/include/zhihe/a210.h. CPU_SS_RSTGEN register
documentation defines cluster bit 0 and individual core bits 1--4.

Standard Sstc/Zicbom/Zicbop/Zicboz ISA properties and 64-byte block sizes
allow generic OpenSBI to configure the corresponding permissions. Linux
must receive matching properties. There is no A210 timer implementation
or platform override clearing MENVCFG.STCE.

## Other services and validation

The A210 PMU uses SSCOFPMF-style event overflow bits and private cycle and
instret event selectors, not the old C900 counter interrupt-enable scheme.
The setup is derived from vendor OpenSBI d9cfcff67e68. Values whose
revision semantics are not independently confirmed remain documented
as vendor settings.

## System reset

The A210 reset driver uses SCMI System Power for SBI SRST reboot and
power-off. Boot firmware starts the E902 service. OpenSBI finds its tx
mailbox and shared-memory channel through the arm,scmi node, using the
first (A2P) channel and ignoring a following notification channel, then
probes System Power directly. The OS does not need a protocol@12 child for SBI
reset to work. Reboot retains a native system-reset fallback if the
service is unavailable; shutdown is advertised only after a successful
System Power version query.

The generic SRST path sends asynchronous HALT IPIs. Before taking over
the OS channel, the A210 client waits for all other assigned harts to
reach STOPPED, then waits for any firmware transaction to complete.
Both waits are bounded. A failed hart rendezvous prevents a new SCMI
request; cold reset can still use the native fallback.

The SCMI client has been cross-built and RAM-loaded with the paired
boot chain. Its SRST channel takeover, shutdown and cold reset still need
hardware validation. Previous Linux SRST and fallback tests used the
legacy AON client.

D2D, an OP-TEE dispatcher, private IOPMP SBI services and CPU power gating
are not implemented.

Cross-build and mocked-MMIO tests do not replace board validation of
first boot, all eight harts, hotplug and cache coherency. In particular,
the supplied C920 manual describes V3 rather than the exact A210 C920v2.
