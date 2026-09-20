# T-Head TH1520

The generic platform matches `thead,th1520`. The boot stage initializes
DDR, shared L2 and IOPMP.

## HSM

- All harts released: generic HSM uses IPIs; any hart may boot.
- Only hart 0 released: TH1520 HSM releases harts 1–3 on first use.

Other partial reset states are rejected. Later stop/start cycles use
parking and IPIs. Per-hart power-off and suspend are not supported.

## Reset

`CONFIG_FDT_RESET_TH1520` enables the RV64 driver. The SoC root node
provides WDT0 reboot; AON adds reboot and shutdown when E902 is running.

AON requires enabled `thead,th1520-aon` and `thead,th1520-mbox` nodes.
The driver follows the named `aon` mailbox on channel 1 and reads its
`local` and `remote-icu0` register windows from the device tree.

Reboot tries AON before WDT0. Failed shutdown never triggers a reboot.
