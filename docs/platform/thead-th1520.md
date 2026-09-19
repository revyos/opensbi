# T-Head TH1520

The generic platform matches `thead,th1520`. The boot stage initializes
DDR, shared L2 and IOPMP.

## HSM

- All harts released: generic HSM uses IPIs; any hart may boot.
- Only hart 0 released: TH1520 HSM releases harts 1–3 on first use.

Other partial reset states are rejected. Later stop/start cycles use
parking and IPIs. Per-hart power-off and suspend are not supported.
