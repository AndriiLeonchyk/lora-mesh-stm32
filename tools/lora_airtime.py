#!/usr/bin/env python3
"""LoRa time-on-air calculator.

Uses the formula from the Semtech SX1276/77/78/79 datasheet
("Time on air" in the LoRa modem section).

Examples:
    python tools/lora_airtime.py                     # table for SF7..SF12
    python tools/lora_airtime.py --payload 60 --bw 250
    python tools/lora_airtime.py --selftest
"""

import argparse
import math


def time_on_air_ms(payload_len, sf, bw_hz=125_000, cr=1, preamble=8,
                   crc=True, implicit_header=False, low_dr_opt=None):
    """Return the time on air of one LoRa packet in milliseconds.

    payload_len      payload length in bytes (the whole mesh frame)
    sf               spreading factor, 6..12
    bw_hz            bandwidth in Hz, e.g. 125000
    cr               coding rate 1..4 for 4/5..4/8
    preamble         preamble length in symbols (8 by default)
    crc              hardware payload CRC enabled
    implicit_header  implicit header mode (no LoRa header)
    low_dr_opt       LowDataRateOptimize; None = auto (on when a symbol > 16 ms)
    """
    if not 6 <= sf <= 12:
        raise ValueError("sf must be 6..12")
    if not 1 <= cr <= 4:
        raise ValueError("cr must be 1..4 (4/5..4/8)")
    if not 0 <= payload_len <= 255:
        raise ValueError("payload_len must be 0..255")

    t_sym = (2 ** sf) / bw_hz                     # seconds per symbol
    if low_dr_opt is None:
        low_dr_opt = t_sym > 0.016
    de = 1 if low_dr_opt else 0
    ih = 1 if implicit_header else 0
    crc_on = 1 if crc else 0

    t_preamble = (preamble + 4.25) * t_sym
    num = 8 * payload_len - 4 * sf + 28 + 16 * crc_on - 20 * ih
    den = 4 * (sf - 2 * de)
    n_payload = 8 + max(math.ceil(num / den) * (cr + 4), 0)
    return (t_preamble + n_payload * t_sym) * 1000.0


def bitrate_bps(sf, bw_hz=125_000, cr=1):
    """Raw LoRa bit rate in bit/s."""
    return sf * bw_hz / (2 ** sf) * 4 / (4 + cr)


def packets_per_hour(airtime_ms, duty_cycle=0.01):
    """How many packets fit into a duty-cycle limit per hour (1 % = 36 s/h)."""
    return int(3600_000 * duty_cycle // airtime_ms)


def selftest():
    # Reference values computed with the datasheet formula
    # (BW 125 kHz, CR 4/5, 8-symbol preamble, explicit header, CRC on).
    cases = [
        (20, 7, 56.576),
        (20, 9, 185.344),
        (20, 12, 1318.912),
        (60, 7, 112.896),
    ]
    for payload, sf, expected in cases:
        got = time_on_air_ms(payload, sf)
        assert abs(got - expected) < 0.01, (payload, sf, got, expected)
    assert time_on_air_ms(20, 12) > 20 * time_on_air_ms(20, 7)
    assert packets_per_hour(1000.0) == 36
    print("lora_airtime selftest: all checks passed")


def main():
    ap = argparse.ArgumentParser(description="LoRa time-on-air calculator")
    ap.add_argument("--payload", type=int, default=20, help="payload bytes (default 20)")
    ap.add_argument("--bw", type=int, default=125, help="bandwidth in kHz (default 125)")
    ap.add_argument("--cr", type=int, default=1, help="coding rate 1..4 = 4/5..4/8 (default 1)")
    ap.add_argument("--preamble", type=int, default=8, help="preamble symbols (default 8)")
    ap.add_argument("--duty", type=float, default=1.0, help="duty cycle in %% (default 1)")
    ap.add_argument("--selftest", action="store_true", help="run built-in checks")
    args = ap.parse_args()

    if args.selftest:
        selftest()
        return

    bw_hz = args.bw * 1000
    print(f"Payload {args.payload} B, BW {args.bw} kHz, CR 4/{args.cr + 4}, "
          f"preamble {args.preamble}, duty cycle {args.duty}%")
    print(f"{'SF':>3} {'bit/s':>8} {'airtime ms':>11} {'packets/h':>10}")
    for sf in range(7, 13):
        t = time_on_air_ms(args.payload, sf, bw_hz, args.cr, args.preamble)
        print(f"{sf:>3} {bitrate_bps(sf, bw_hz, args.cr):>8.0f} {t:>11.1f} "
              f"{packets_per_hour(t, args.duty / 100):>10}")


if __name__ == "__main__":
    main()
