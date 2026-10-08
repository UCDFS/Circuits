# BSPD: Brake System Plausibility Device

This folder holds the KiCad design for the car's BSPD board. The original design is by Keelan Reilly (hence the `_KR` in the folder name). It was reworked in September and October 2026 to fix three rule and reliability problems.

> **Status (8 October 2026):** the design passes KiCad's checks and behaves correctly in simulation. **No board has been built or bench-tested yet.** Read [section 5](#5-open-issues-read-before-ordering) before ordering, because one issue there may need a design change.

## The short version of what changed

1. **The old board reset itself too early.** After tripping, it closed the shutdown circuit again by itself after roughly 7 to 14 seconds, depending on part tolerances. FSUK rule T11.6.1 only allows a self-reset after more than 10 seconds, so some boards would have broken the rule. The new board stays tripped until the LV master switch (LVMS) is turned off and on again.
2. **The old board was too slow to trip.** Its "500 ms" timer actually took about 0.51 s with exactly nominal parts, which is over the 500 ms limit in T11.6.2. The new timer takes about 0.36 s, and between 0.29 s and 0.43 s across part tolerances.
3. **One capacitor was drawn backwards.** C10 is an electrolytic capacitor, so it has a + leg and a − leg. Its + leg was connected to ground. That is fixed.

The PCB, BOMs and manufacturing files were all updated to match. [Section 4](#4-exactly-what-changed) lists every change.

## Contents

1. [What a BSPD is and why the car needs one](#1-what-a-bspd-is-and-why-the-car-needs-one)
2. [Background you need](#2-background-you-need)
3. [How this board works, stage by stage](#3-how-this-board-works-stage-by-stage)
4. [Exactly what changed](#4-exactly-what-changed)
5. [Open issues: read before ordering](#5-open-issues-read-before-ordering)
6. [Calibrating and bench-testing a new board](#6-calibrating-and-bench-testing-a-new-board)
7. [Files in this folder and how to regenerate them](#7-files-in-this-folder-and-how-to-regenerate-them)
8. [Glossary](#8-glossary)

---

## 1. What a BSPD is and why the car needs one

### The shutdown circuit

The car has a **shutdown circuit (SDC)**. It is one long loop of wire that passes through every safety device on the car: the shutdown buttons, the inertia switch, the brake over-travel switch, the IMD, the AMS, this board, and others. Current flows round the loop and holds the **AIRs** (accumulator isolation relays) closed. The AIRs are the big relays that connect the high-voltage battery to the rest of the car.

If any device in the loop opens its contact, current stops flowing round the loop, the AIRs drop open, and the high-voltage battery is disconnected. The car stops making power.

### What the BSPD is looking for

The BSPD is one of the devices in that loop. It watches for one dangerous situation:

> **The driver is braking hard while the motor is still being given a lot of power.**

That never happens in normal driving. If it does, something is broken. The throttle might be stuck, a pedal sensor might have failed, or there might be a bug in the VCU software. The BSPD does not try to work out which one. It just opens the shutdown circuit.

It is deliberately built without any microcontroller or code. The BSPD is the backstop for software failures, so if it ran software, the same kind of bug it is meant to catch could also disable it.

### What the rules require (FSUK 2026, section T11.6)

| Rule | What it says | How this board meets it |
|---|---|---|
| T11.6.1 | Must be a standalone, non-programmable circuit. Must open the SDC when hard braking happens while at least 5 kW is going to the motors. Then it must either stay open until the LVMS is power-cycled, or reset itself only after the condition has been gone for more than 10 s. | Comparators and logic gates detect the condition, and a latch keeps the SDC open until power-cycle. There is no software anywhere on the board. |
| T11.6.2 | Must open the SDC if the implausibility lasts more than 500 ms. | An RC timer trips it after about 0.36 s. |
| T11.6.3 | Must be powered directly from the LVMS. | Power comes in on J3. That must be wired straight from the LVMS. |
| T11.6.4 | No extra functions on the board. Only power, the required sensors and the shutdown circuit connect to it. | The only connectors are J1 (sensors), J2 (shutdown circuit) and J3 (power). |
| T11.6.5 | Hard braking must be measured with a brake pressure sensor. The threshold must be 30 bar or less, and below the point where the wheels lock. | There are two pressure inputs (front and rear), each with its own adjustable threshold. |
| T11.6.6 | Power must be measured with a DC current sensor. The threshold must equal 5 kW at the maximum TS voltage. | There is one current input with an adjustable threshold. |
| T11.6.7 | It must be possible to disconnect each sensor wire separately at inspection. | Every sensor comes in through connector J1. See [section 5, issue 1](#issue-1-a-single-unplugged-or-shorted-sensor-does-not-trip-the-bspd-on-its-own) for what happens when you unplug one. |
| T11.6.9, IN4.1.3 | At inspection, the team proves the BSPD works by injecting a fake current signal (equivalent to 5 kW) while pressing the brake pedal. | See [section 6](#6-calibrating-and-bench-testing-a-new-board). |

The full rules PDF is at the root of this repository.

---

## 2. Background you need

Every idea in this section is used in section 3. Skip any you already know.

### Voltage and ground

Voltage is always measured between two points. When we say a wire is "at 5 V", we mean 5 V above **ground**, the shared reference wire. On this board ground is called **GLV−**. One part of the schematic labels it **GND** instead, but KiCad joins the two names, and they are the same wire.

### The two supplies on this board

- **GLV+** is the car's low-voltage supply. It comes in on J3. It powers the relay coil (a 12 V part) and the two comparator chips.
- **+5V** is made on the board from GLV+ by **U4**, an LM7805 voltage regulator. A regulator takes in a higher voltage that may wobble and puts out a steady fixed voltage. Everything that does logic runs from +5V.

### Resistors and voltage dividers

Put two resistors in series between +5V and ground, and the point between them sits at a fixed fraction of 5 V:

```
V_middle = 5 V × R_bottom / (R_top + R_bottom)
```

This board uses dividers to make fixed **reference voltages** that signals get compared against. For example, R7 (100 kΩ, on top) and R8 (10 kΩ, on the bottom) give 5 × 10 / 110 = **0.45 V**.

A **potentiometer** (RV1, RV2, RV3) is an adjustable divider. Turning its screw moves the middle pin (the "wiper") anywhere between 0 V and 5 V. These three set the trip thresholds. They are 25-turn trimmers, so each one takes many turns to go from end to end. That makes them easy to set accurately.

### Pull-up and pull-down resistors

A wire that isn't connected to anything is "floating". Its voltage is undefined, and it picks up electrical noise. A **pull-down** resistor (to ground) or a **pull-up** resistor (to +5V) gives the wire a default voltage that anything stronger can override.

- R1, R2 and R3 (100 kΩ) pull each sensor input down to 0 V. An unplugged sensor therefore reads 0 V instead of random noise.
- R13, R14, R15, R26 and R27 (10 kΩ) pull comparator outputs up to 5 V. The next part explains why those are needed.

### Comparators (U5 and U19, LM339)

A comparator has two inputs, marked **+** and **−**. It answers one question: **is the + input higher than the − input?** Each LM339 chip holds four separate comparators, labelled A, B, C and D in the schematic.

The LM339 has what is called an **open-collector output**. This confuses everyone at first, so here it is slowly:

- If **+ is higher than −**, the output lets go of its wire. The pull-up resistor then pulls the wire up to 5 V. **Output = HIGH.**
- If **− is higher than +**, the output connects its wire to ground. **Output = LOW.**

Because an open-collector output can only ever pull *down*, two outputs can safely be joined onto the same wire. If **either** of them pulls low, the wire goes low. This is called a **wired-OR**. This board uses it three times, to combine "signal too high" and "signal missing" onto one wire for each sensor.

### Active-low signals

A signal is **active-low** if LOW means "yes, this is happening". Most of the comparator outputs on this board are active-low. The wire goes LOW when the brake is pressed hard, when the current is high, or when a sensor looks faulty. Keep this in mind when reading section 3, because it is the main thing that makes the logic look backwards.

### Logic gates

Logic chips treat a voltage near 5 V as **1 (HIGH)** and a voltage near 0 V as **0 (LOW)**. Each logic chip on this board is a tiny 5-pin package holding exactly one gate.

| Gate | Chips | The output is HIGH when… |
|---|---|---|
| NOT (inverter) | U9 (SN74LVC1G04) | the input is LOW |
| AND | U18 (SN74AHCT1G08) | both inputs are HIGH |
| NOR | U6, U20, U21 (SN74AHC1G02) | both inputs are LOW |

### Capacitors and RC timers

A capacitor stores charge. If you charge it through a resistor, its voltage rises gradually rather than instantly. How fast it rises depends on the **time constant**, written τ (the Greek letter tau):

```
τ = R × C
V(t) = V_supply × (1 − e^(−t/τ))
```

After one τ the capacitor is 63% of the way to the supply voltage. Add a comparator watching the capacitor's voltage and you have a **timer**. It answers the question "has the input been HIGH for longer than X?"

**Electrolytic capacitors** (C10 and C13 here, both 10 µF) store a lot of charge for their size, but they are **polarised**. One leg is + and must always be at the higher voltage. Fitted backwards, they leak, heat up, and can eventually fail or burst. On this PCB, the + pad of each one is the square pad, and it is marked with + on the silkscreen.

The small ceramic capacitors (0.1 µF: C5, C6, C7, C8, C9, C12 and C14; 0.22 µF: C4) are not polarised. They sit right next to each chip's power pins as **decoupling** capacitors. Each one is a tiny local reservoir that smooths out the current spikes a chip draws every time it switches.

### The SR latch (a one-bit memory)

Take two NOR gates and connect each gate's output to one input of the other. The result *remembers* things. It has two control inputs:

- **S (set):** a brief HIGH here puts it into the "tripped" state.
- **R (reset):** a brief HIGH here puts it back into the "OK" state.

When both inputs are LOW, it stays in whatever state it was last put in, for as long as it has power. That is exactly the behaviour "stay open until power-cycled" needs.

### A MOSFET used as a switch (Q1, 2N7002)

A MOSFET is a switch controlled by a voltage. Q1 is an N-channel MOSFET wired as a **low-side switch**. When its gate (control pin) is at 5 V, it conducts and connects the bottom of the relay coil to ground. When its gate is at 0 V, it is off.

### The relay and its flyback diode (K1 and D9)

A relay is a switch moved by an electromagnet. Current through its **coil** pulls its contacts together. K1 is a **normally open** relay: with no coil current, the contacts are apart. Those contacts sit in the shutdown circuit, through connector J2. So:

> **Relay powered = shutdown circuit closed = car can drive.**
> **Relay unpowered = shutdown circuit open = car is shut down.**

This arrangement is **fail-safe**. If the board loses power, a wire breaks, or a chip dies with its output stuck low, the relay drops out and the car shuts down.

When the current through a coil is switched off, the coil's collapsing magnetic field produces a voltage spike big enough to destroy Q1. **D9** (a 1N4001 diode) sits across the coil and gives that energy a safe path. This is called a **flyback diode**.

---

## 3. How this board works, stage by stage

### The overall flow

```
 CURRENT_SENSOR (J1 pin 3)      BRAKE_SENSOR_FRONT (J1 pin 1)     BRAKE_SENSOR_REAR (J1 pin 2)
           │                                │                                  │
 Stage 1   │  input protection (1 kΩ in series, 100 kΩ pull-down, test point)  │
           │                                │                                  │
 Stage 2   U5 A + U5 B                      U19 A + U19 B                      U19 C + U19 D
           "current too high                "front pressure too high           "rear pressure too high
            OR sensor missing"               OR sensor missing"                 OR sensor missing"
           LOW = yes  (LED1)                LOW = yes  (LED2)                  LOW = yes  (LED3)
           │                                └────────────────┬─────────────────┘
           │                                        Stage 3a: U18 AND
           │                                        "either brake asserted", LOW = yes
           └──────────────────────┬──────────────────────────┘
                         Stage 3b: U6 NOR
                         IMPLAUSIBLE: HIGH = hard braking AND high power, right now
                                  │
                         Stage 4: R20 + C10 timer, watched by U5 D
                         "implausible for longer than ~0.36 s"  (LED4)
                                  │
                         U9 NOT  →  BSPD_TRIP  (HIGH = trip now)
                                  │
                         Stage 5: U20 + U21 latch  →  LATCH_Q  (HIGH = tripped, remembered)
                                  │      ↑ POR (C13, R30, D10) forces it to "OK" at power-up
                                  │
                         Stage 6: U5 C  →  Q1  →  K1 relay  →  J2  →  shutdown circuit
                         tripped: relay off, shutdown circuit open  (LED5)
```

Many wires in the schematic have no name, and KiCad numbers them automatically (`Net-(U5-…)` and similar). The descriptions below name each wire by what it does and by the pins it joins.

### Stage 0: Power

| Part | Job |
|---|---|
| J3 | Power input. Pin 1 is GLV+ and pin 2 is GLV−. It must be supplied directly from the LVMS (T11.6.3). |
| U4 (LM7805) | Makes +5V from GLV+. |
| C4 (0.22 µF) | Input capacitor for U4, on GLV+. |
| C7, C12 (0.1 µF) | On GLV+. Decoupling for the two comparator chips, which run directly from GLV+ (pin 3 on each). |
| C5, C6, C8, C9 (0.1 µF) | On +5V. Output capacitor for U4, and decoupling for the logic gates U6, U9 and U18. |
| C14 (0.1 µF) | On +5V. Decoupling for the two latch chips, U20 and U21. |

The comparators run from GLV+, but they only ever see 0 to 5 V on their inputs, which is well within what an LM339 accepts. Their outputs are pulled up to +5V, so the logic chips also only ever see 0 to 5 V.

### Stage 1: Sensor inputs

| Signal | J1 pin | Series resistor | Pull-down | Test point (live signal) |
|---|---|---|---|---|
| BRAKE_SENSOR_FRONT | 1 | R5, 1 kΩ | R2, 100 kΩ | TP2 |
| BRAKE_SENSOR_REAR | 2 | R6, 1 kΩ | R3, 100 kΩ | TP3 |
| CURRENT_SENSOR | 3 | R4, 1 kΩ | R1, 100 kΩ | TP1 |

The 1 kΩ series resistors limit the current into the comparator inputs if a wire picks up a spike or gets shorted to something. The 100 kΩ pull-downs make an unplugged sensor read 0 V.

J1 carries only the three signal wires. The sensors get their power and ground from elsewhere in the car's wiring.

### Stage 2: Comparators, two per sensor

Each sensor is watched by two comparators whose outputs are joined (a wired-OR):

- **"Too high":** the sensor goes into the **−** input and the adjustable threshold (from a potentiometer) goes into the **+** input. When the signal rises above the threshold, − is higher than +, so the output pulls LOW.
- **"Missing":** the sensor goes into the **+** input and a fixed 0.45 V reference goes into the **−** input. When the signal drops below 0.45 V, − is higher than +, so the output pulls LOW. An unplugged sensor, a broken wire, or a short to ground all read close to 0 V, so all of them trigger this.

So each shared output wire goes LOW for "too high" **or** "missing".

| Sensor | "Too high" comparator (pins) | Threshold set by / measured at | "Missing" comparator (pins) | 0.45 V reference from | Output pull-up | LED (via resistor) |
|---|---|---|---|---|---|---|
| Current | U5 A (− 4, + 5, out 2) | RV1 / TP4 | U5 B (− 6, + 7, out 1) | R7 + R8 | R13 | LED1 (R16) |
| Front brake | U19 A (− 4, + 5, out 2) | RV2 / TP5 | U19 B (− 6, + 7, out 1) | R9 + R10 | R14 | LED2 (R17) |
| Rear brake | U19 D (− 10, + 11, out 13) | RV3 / TP6 | U19 C (− 8, + 9, out 14) | R11 + R12 | R15 | LED3 (R18) |

Each LED has its + leg (anode) on +5V and its − leg (cathode) going through a 10 kΩ resistor to the comparator output wire. The LED lights when that wire goes LOW.

### Stage 3: Combining the signals

**U18, an AND gate**, takes the front-brake wire and the rear-brake wire. Its output is HIGH only when both brake wires are HIGH, meaning neither brake is pressed hard and neither sensor is missing. If either one goes LOW, U18's output goes LOW. So U18's output is an active-low "**either brake asserted**" signal.

**U6, a NOR gate**, takes the current wire and U18's "either brake asserted" wire. Its output is HIGH only when both inputs are LOW:

| Current wire | Brake wire (from U18) | U6 output | Meaning |
|---|---|---|---|
| HIGH (current OK) | HIGH (brakes OK) | LOW | Normal driving |
| LOW (current high) | HIGH (brakes OK) | LOW | Accelerating, which is fine |
| HIGH (current OK) | LOW (braking hard) | LOW | Braking, which is fine |
| **LOW (current high)** | **LOW (braking hard)** | **HIGH** | **Implausible: braking hard with power on** |

U6's output is the **IMPLAUSIBLE** signal. It is HIGH whenever the dangerous combination is happening at that instant.

### Stage 4: The 500 ms timer

The rules give the car up to 500 ms of implausibility before the BSPD must act. That allows for things like the driver stabbing the brake for a moment while lifting off the throttle.

- U6's output feeds **R20 (39 kΩ)**, which charges **C10 (10 µF)**. The wire between them is the net called `500ms`.
- When IMPLAUSIBLE is HIGH, C10 charges towards 5 V with τ = 39 kΩ × 10 µF = **0.39 s**.
- **U5 D** compares C10's voltage (− input, pin 10) against a fixed **3.0 V** (+ input, pin 11). The 3.0 V comes from the R22 (10 kΩ) and R23 (15 kΩ) divider: 5 × 15 / 25 = 3.0 V.
- C10 reaches 3.0 V after t = τ × ln(5 / (5 − 3)) = 0.39 × 0.916 = **0.36 s**.
- At that point U5 D's output (pin 13) goes LOW and LED4 lights (through R28). **U9** inverts the signal, so its output, **BSPD_TRIP**, goes HIGH.
- If IMPLAUSIBLE goes away before 0.36 s, U6's output goes LOW and C10 drains back out through R20. A short blip is ignored.
- Electrolytic capacitors are only accurate to ±20%, so C10 is really somewhere between 8 µF and 12 µF. That puts the trip time between **0.29 s and 0.43 s**, always under the 500 ms limit.
- If a second implausibility comes shortly after a first one, C10 may not have fully drained yet, so the second trip comes a little sooner. That is the safe direction.

### Stage 5: The latch

**U20 and U21** are two NOR gates wired as an SR latch:

- **U21:** inputs `LATCH_Q` and `BSPD_TRIP`, output `LATCH_QN`
- **U20:** inputs `LATCH_QN` and `POR`, output `LATCH_Q`

BSPD_TRIP is the **set** input and POR is the **reset** input. LATCH_Q is HIGH when the BSPD is tripped.

Here is what happens during a trip:

1. Normally LATCH_Q = 0 and LATCH_QN = 1.
2. BSPD_TRIP goes HIGH. U21 now has a HIGH input, so its output, LATCH_QN, goes to 0.
3. U20 now sees LATCH_QN = 0 and POR = 0. Both inputs are LOW, so its output, LATCH_Q, goes to 1.
4. U21 now sees LATCH_Q = 1, so LATCH_QN stays 0 even after BSPD_TRIP goes LOW again. **The latch is locked in the tripped state.**

There are only two ways out: POR going HIGH (which happens at power-up), or losing the 5 V supply altogether. In practice, both mean turning the LVMS off and on.

#### The power-on reset (POR): C13, R30 and D10

When a latch first gets power, it lands in a random state. Without help, the car would sometimes power up already tripped. The POR circuit forces the latch into "OK" for a moment every time power comes on.

- **C13 (10 µF)** connects +5V to the `POR` wire. **R30 (100 kΩ)** connects `POR` to ground.
- When power comes on, +5V jumps up. C13 starts out empty, so it drags `POR` up to about 5 V along with it.
- C13 then charges through R30, and `POR` falls back towards 0 V with τ = 100 kΩ × 10 µF = **1 s**.
- The latch chip reads POR as HIGH (reset active) until it falls below about 3.5 V, which takes about 0.36 s. It reads POR as definitely LOW once it falls below about 1.5 V, which takes about 1.2 s. During that window the latch is held in "OK".
- **D10 (1N4148)** matters at power-off. When +5V drops, C13 would try to push `POR` below 0 V, which can damage U20's input. D10 stops it going much below −0.6 V and empties C13 quickly, so a quick off-and-on still gives a clean reset.
- If a real fault is present at power-up, nothing goes wrong. BSPD_TRIP can't go HIGH until the 0.36 s timer has run, and once POR has dropped, the latch sets normally.

### Stage 6: Driving the relay

- **U5 C** compares LATCH_Q (− input, pin 8) against a fixed **1.02 V** (+ input, pin 9). The 1.02 V comes from the R24 (39 kΩ) and R25 (10 kΩ) divider: 5 × 10 / 49 = 1.02 V. U5 C is just acting as an inverter: when LATCH_Q is HIGH (tripped), its output (pin 14) pulls LOW.
- Pin 14's wire is **Q1's gate**, pulled up to +5V by R27 (10 kΩ).
- **Not tripped:** R27 holds Q1's gate at 5 V, so Q1 conducts. Current flows from GLV+ through K1's coil and Q1 to ground. The relay is closed, so the shutdown circuit is closed through J2.
- **Tripped:** U5 C pulls Q1's gate LOW, so Q1 turns off and the coil current stops. K1 opens, the shutdown circuit opens, the AIRs open, and the high voltage is disconnected. LED5 lights (through R29).
- **J2** connects to the shutdown circuit. Pin 1 is BSPD_Shutdown+ and pin 2 is BSPD_Shutdown−.

Why does a comparator drive Q1, rather than the latch driving it directly? On the old board, U5 C watched the 10-second reset timer. Reusing it kept the PCB changes small, and it also makes a convenient 5 V gate driver.

### What the LEDs mean

All five LEDs are green. They run at only about 0.3 mA, so they are dim, especially in daylight.

| LED | When it is on |
|---|---|
| LED1 | The current is above its threshold, **or** the current sensor reads below 0.45 V (unplugged or shorted to ground). |
| LED2 | The front brake pressure is above its threshold, **or** the front sensor reads below 0.45 V. |
| LED3 | The rear brake pressure is above its threshold, **or** the rear sensor reads below 0.45 V. |
| LED4 | The implausibility has lasted at least ~0.36 s, so the timer has expired. It goes off again once the condition clears. |
| LED5 | **The BSPD has tripped and latched.** The relay and the shutdown circuit are open. It stays on until the LVMS is power-cycled. |

### Test points

Measure all of these against GLV− (ground).

| Test point | What it measures |
|---|---|
| TP1 | The current sensor signal |
| TP2 | The front brake sensor signal |
| TP3 | The rear brake sensor signal |
| TP4 | The current threshold, set with RV1 |
| TP5 | The front brake threshold, set with RV2 |
| TP6 | The rear brake threshold, set with RV3 |

---

## 4. Exactly what changed

This section compares the current design with the version on GitHub `main` as of Keelan's last commit there (`f4da82b`, 9 July 2026).

### 4.1 Behaviour before and after

| | Before | After |
|---|---|---|
| Time from implausibility to trip | ~0.51 s with nominal parts (0.41–0.62 s across tolerances). **Over the 500 ms limit.** | ~0.36 s with nominal parts (0.29–0.43 s across tolerances). |
| What happens after a trip | Resets itself roughly 7–14 s after the condition clears. **Can be under the 10 s the rules require.** | Stays tripped until the LVMS is power-cycled. |
| C10 | Fitted backwards, so it was reverse-biased (by up to 5 V) whenever an implausibility happened. | Fitted the right way round. |
| Power-up | The relay stayed open for about 16 s after every power-up. C11 connected +5V to the 10-second node, so switching on dragged that node to 5 V, and it then had to drain through R21 like a trip. This was confirmed in simulation. | The POR circuit forces the latch to "OK", so the relay closes within a few milliseconds of power-up. |

### 4.2 Schematic (`BSPD KiCAD.kicad_sch`)

**Removed: the old 10-second self-reset timer**

| Ref | Part | What it did |
|---|---|---|
| Q3 | 2N7002 MOSFET | When the BSPD tripped, it charged up the 10-second node from +5V. |
| R19 | 1 kΩ | Series resistor between Q3 and the 10-second node. |
| C11 | 10 µF electrolytic | The 10-second capacitor, between +5V and the `10s` net. |
| R21 | 1 MΩ | Slowly drained the 10-second node. τ = 1 MΩ × 10 µF = 10 s. |

Why it didn't work: the hold time was 10 s × ln(V_start / 1.02 V), where V_start was the voltage Q3 charged the node to. Q3 could only lift the node to about 5 V minus its own gate threshold voltage, and that threshold varies from one 2N7002 to the next (roughly 1 V to 2.5 V). Combined with C11's ±20% tolerance, the hold time came out anywhere from about 7 s to about 14 s. Anything under 10 s breaks T11.6.1.

**Added**

| Ref | Part | Purpose |
|---|---|---|
| U20 | SN74AHC1G02DBVR NOR gate, SOT-23-5 | One half of the latch. Produces `LATCH_Q`. |
| U21 | SN74AHC1G02DBVR NOR gate, SOT-23-5 | The other half of the latch. Produces `LATCH_QN`. |
| C13 | 10 µF electrolytic, Panasonic ECE-A1EKA100I | POR capacitor, from +5V to `POR`. Same part as C10. |
| R30 | 100 kΩ | POR resistor, from `POR` to ground. |
| D10 | 1N4148 diode | Clamps `POR` at power-off. |
| C14 | 0.1 µF ceramic | Decoupling for U20 and U21. |

U20 and U21 are the same part as the existing U6, so the parts list didn't gain a new chip type.

**Changed**

| What | Before | After | Why |
|---|---|---|---|
| R20 | 56 kΩ | 39 kΩ | 56 kΩ × 10 µF × 0.916 = 0.51 s, over the 500 ms limit. 39 kΩ gives 0.36 s. |
| C10 orientation | + pin on GLV−, − pin on the `500ms` node | + pin on the `500ms` node, − pin on GLV− | It was reverse-biased. |
| U9 output (pin 4) | Drove Q3's gate | Drives the new `BSPD_TRIP` net, which goes into U21 pin 2 (the latch's set input) | The latch replaces the timer. |
| U5 pin 8 (U5 C's − input) | Connected to the `10s` timer node | Connected to `LATCH_Q` | Same comparator and same 1.02 V reference, but it now watches the latch instead of the timer. |

There are four new named nets: `BSPD_TRIP`, `LATCH_Q`, `LATCH_QN` and `POR`. The `10s` net no longer exists.

Everything else is unchanged: the sensor inputs, all comparator thresholds, the LEDs, the relay, the connectors and the power supply. A raw diff of the schematic file looks much bigger than this, because KiCad renumbered many of the unnamed nets. The connections themselves are the same.

**Intermediate steps, for the record.** In the first fix, C13 was 220 nF and R30 was 470 kΩ. A whole-board simulation then showed that if the 5 V supply came up slowly, the reset pulse might not get high enough for the logic chips to see it. So C13 and R30 became 10 µF and 100 kΩ. A second decoupling capacitor, C15, was briefly added and then removed, because one capacitor (C14) is enough for two neighbouring chips.

### 4.3 PCB (`BSPD KiCAD.kicad_pcb`)

- **Same board size and shape:** 2 copper layers, about 52 × 81 mm, with a ground (GLV−) pour on the top layer.
- **Footprints:** 71 → 73 (4 removed, 6 added).
- **U20 and U21 are on the bottom of the board**, under R30 and D10. Every other part is on the top.
- R30, D10 and C13 sit where R19, R21 and C11 used to be. C14 is just below them.
- J2 moved 0.5 mm to make room for the larger C13.
- The new connections were routed with Freerouting. The existing routing was kept, and checked afterwards to be unchanged to within 1.5 mm.
- **Tracks:** all at least 0.25 mm wide. The default net class used to say 0.2 mm, which is below the board's own minimum, so it was raised to 0.25 mm.
- **Vias:** the drill went from 0.25 mm to 0.3 mm to meet the board's own minimum hole size. The via count went from 5 to 8.
- **Board outline (Edge.Cuts):** closed up. Some of its line segments didn't quite meet, which made some tools (including the Freerouting export) reject the outline.
- **Footprint text fields** now match the schematic: R30's value changed from 470k to 100k; R20's part number changed from 56K to 39K; D10's leftover 1N4001 part number, LCSC number and datasheet link were cleared; Q1 gained its part number; and the datasheet and description text on C13, C14, U20 and U21 now matches. This is text only. No copper changed.
- **Checks:** DRC reports 0 errors and 0 unconnected items. The schematic-versus-PCB check flags only H1 to H4, which are mounting holes and are meant to exist only on the PCB.

### 4.4 Project file (`BSPD KiCAD.kicad_pro`)

- Default net class: track width 0.2 mm → 0.25 mm, and via drill 0.25 mm → 0.3 mm.
- Everything else in that file's diff is KiCad 10 adding new setting names. None of it affects the design.

### 4.5 Other files

| File | Change |
|---|---|
| `BSPD BOM.csv` | Regenerated from the schematic. The old one still listed Q3, R19, R21 (1 MΩ), C11 and R20 at 56k, and had no U20, U21, C13, C14, D10 or R30. |
| `BSPD BOM_smd.csv` | Regenerated. Q3 removed, U20 and U21 added. |
| `fab/` | New. Contains the gerber and drill files (`fab/gerbers/`), the BOM (`fab/BSPD_BOM.csv`) and the pick-and-place file (`fab/BSPD_CPL.csv`). |
| `analysis/helpers/bspd_system.cir` | New. An ngspice simulation of the whole board, from sensor inputs to relay. |
| `README.md` | New. This file. |

### 4.6 Commits

These are on the branch `bspd-latch-fix`:

| Commit | Date | What it did |
|---|---|---|
| `2082113` | 20 Sep 2026 | Schematic: replaced the 10 s self-reset with the SR latch, and changed R20 from 56k to 39k. |
| `09bb96b` | 20 Sep 2026 | PCB: placed and routed the latch, closed the board outline, and moved to 0.3 mm vias. |
| `986bf25` | 22 Sep 2026 | Fixed C10's polarity, strengthened the POR (10 µF and 100 kΩ), and made every track 0.25 mm or wider. |
| `bad0fb1` | 7 Oct 2026 | Synced the PCB text fields with the schematic, regenerated the BOMs, and added the fab outputs and the simulation. |

The README commit comes after these.

### 4.7 How the changes were checked

- **KiCad DRC:** 0 errors, 0 unconnected items.
- **KiCad ERC:** the same 2 errors as before the changes. Both are cosmetic; see section 5.
- **Schematic versus PCB:** checked with both KiCad and the kicad-happy cross-check. Both are clean apart from the mounting holes.
- **Pinouts:** every chip's pinout was checked against its manufacturer datasheet.
- **Simulation:** `analysis/helpers/bspd_system.cir`, run with ngspice:

| Scenario | Should… | Result |
|---|---|---|
| Power on | close the relay | Closed 3.5 ms after power-up |
| Brake and current together for 300 ms | not trip | No trip. The timer peaked at 2.67 V, below the 3.0 V trip level. |
| Brake and current together for 1 s | trip within 500 ms | Tripped 0.32 s after it started |
| Condition removed after the trip | stay tripped | Still tripped 0.5 s and 1.9 s later |
| LV power-cycle | reset and close the relay | Relay closed 3.5 ms after power returned |

To run the simulation yourself:

```
cd BSPD_KR/analysis/helpers
ngspice -b bspd_system.cir
```

What hasn't been done: **no physical board has been built or tested.**

---

## 5. Open issues: read before ordering

### Issue 1: A single unplugged or shorted sensor does not trip the BSPD on its own

This comes from the original design. The September changes did not cause it, and did not fix it.

**What happens.** "Sensor missing" goes onto the same wire as "pressure too high" or "current too high". So to the board, an unplugged brake sensor looks exactly like hard braking, and an unplugged current sensor looks exactly like high power. The board then still waits for the *other* condition before it trips.

Simulation confirms this. With the car idle, each of these left the relay closed:

- unplugging the front brake sensor
- unplugging the current sensor
- shorting a brake sensor signal to 5 V

The board only trips if the other condition also happens: for example, the driver accelerating with a brake sensor unplugged. Unplugging *everything* does trip it, because then both conditions are present.

**Why it matters.** T11.9.2 says that an open circuit, a short to ground, or (for analogue sensors) a short to the supply voltage on any system-critical signal must put the system into its safe state. For the BSPD's signals, T11.9.5 defines the safe state as an open shutdown circuit. T11.6.7 requires each sensor wire to be unpluggable at inspection, so expect scrutineers to unplug them one at a time.

**A possible fix.** This needs a team decision. Send the "sensor missing" comparators straight to the latch's set input, so they trip the board directly instead of feeding the AND and NOR logic. Then add a "sensor above about 4.5 V" check on each input to catch shorts to supply. This changes both the schematic and the PCB.

### Issue 2: Check the 0.45 V "sensor missing" level against your real sensors

Many pressure sensors output 0.5 V at zero pressure. That leaves only 0.05 V of margin above the 0.45 V "missing" level. With normal sensor and resistor tolerances, a healthy sensor at rest could read as "missing". Because of issue 1, that would count as hard braking, so the BSPD could trip whenever the car accelerates.

Fit the real sensors and measure their resting voltages: TP2 and TP3 with the brake released, and TP1 with zero current.

### Issue 3: The thresholds have to be set on the car

RV1, RV2 and RV3 are adjustable, and none of them has been set yet.

- **Current (RV1, measure at TP4):** set this to the sensor's output voltage at the current equivalent to 5 kW at maximum pack voltage. With the current pack (315 V maximum), that is 5000 W ÷ 315 V ≈ **15.9 A**. Recalculate if the pack changes.
- **Brakes (RV2 at TP5, RV3 at TP6):** set these to each sensor's output voltage at your chosen pressure. The pressure must be 30 bar or less, and low enough that the wheels don't lock (T11.6.5).

### Issue 4: Not built or tested

So far the design has only been checked by DRC, ERC and simulation.

### Issue 5: Old physical boards don't match these files

Boards built from the earlier design have hand modifications that were never written down, such as cut tracks and added jumper wires. Don't use them as a reference for this design, and don't assume these files describe them.

### Issue 6: U20 and U21 are on the bottom of the board

If the board is assembled by JLCPCB, that means two-sided assembly, which costs more. Alternatively, hand-solder those two chips. The pick-and-place file marks them as `bottom`.

### Issue 7: Small known nits (none of these stop the board working)

- **2 ERC errors:** U4's input pin and one ground symbol report "power pin not driven". KiCad wants a PWR_FLAG symbol on GLV+ and GLV−. This is cosmetic.
- **1 ERC warning:** GLV− and GND are two names for the same wire. This is intended.
- **D9 is 0.84 mm from the board edge.** That works but is tighter than ideal for manufacturing.
- **C10 and C13 use the unpolarised capacitor symbol.** They are electrolytic, but the schematic doesn't show which side is +. The PCB footprints do show it, and both are now the right way round.
- **No ground stitching vias.** The kicad-happy check notes this, but it's fine for a slow, simple 2-layer board like this one.

---

## 6. Calibrating and bench-testing a new board

### What you need

- A bench power supply set to 12 V, with its current limit at about 200 mA.
- A multimeter.
- Three adjustable 0 to 5 V sources to stand in for the sensors. For example, three 10 kΩ potentiometers across a separate 5 V supply, with that supply's ground joined to the board's GLV−.
- A continuity tester (or the multimeter's beep mode) across J2.
- Optionally, an oscilloscope, to measure the trip time.

### Steps

1. **Visual check before power.** Confirm that the + legs of C10 and C13 are in the square + pads, that the stripes on D9 and D10 match the silkscreen, and that pin 1 of U5 and U19 is in the right place.
2. **Power up with nothing in J1.** +5V (measured across C5) should read 4.9 to 5.1 V. All three sensor inputs read 0 V, so LED1, LED2 and LED3 should light ("missing"). LED4 should light after about 0.4 s. Within about a second, once the power-on reset has let go, LED5 should light and J2 should go open. Both conditions are present here, so this is a correct trip. It's a good first sign that the whole chain works.
3. **Connect the stand-in sensors at resting values.** Use whatever your real sensors output at rest: for example 0.6 V for each brake, and the current sensor's zero-current voltage. Power-cycle. All LEDs should be off and J2 should be closed.
4. **Set the thresholds.** Measure TP4, TP5 and TP6, and adjust RV1, RV2 and RV3 to the values from [issue 3](#issue-3-the-thresholds-have-to-be-set-on-the-car).
5. **Brake only.** Raise the front brake input above TP5. LED2 should light, with no trip. Lower it, then do the same for the rear (LED3).
6. **Current only.** Raise the current input above TP4. LED1 should light, with no trip.
7. **Both together, held.** LED4 should light within about 0.4 s, then LED5 should light and J2 should open.
8. **Remove both.** LED4 should go off. LED5 should stay on and J2 should stay open. Wait at least 15 s and check that it is still open.
9. **Power-cycle.** LED5 should go off and J2 should close.
10. **Unplug each sensor one at a time** and note what happens. With the current design, the board won't trip from this alone (see [issue 1](#issue-1-a-single-unplugged-or-shorted-sensor-does-not-trip-the-bspd-on-its-own)).
11. **Timing (optional).** Put a scope on the + leg of C10, or across J2, and confirm the trip comes between 0.29 s and 0.43 s after both conditions start.

### At technical inspection

T11.6.9 and IN4.1.3 describe the test: the team injects a signal on the current sensor input that represents 5 kW, while someone presses the brake pedal, and the BSPD must open the shutdown circuit. Plan a simple test lead for this. It unplugs the current sensor at J1 and feeds in an adjustable voltage instead.

---

## 7. Files in this folder and how to regenerate them

| Path | What it is |
|---|---|
| `BSPD KiCAD.kicad_sch` | The schematic (a single sheet). |
| `BSPD KiCAD.kicad_pcb` | The PCB layout. |
| `BSPD KiCAD.kicad_pro` | Project settings: design rules and net classes. |
| `BSPD KiCAD.step` | 3D model. **Out of date:** last exported in April 2026, so it shows the old board without the latch. |
| `BSPD BOM.csv` | Full BOM, generated from the schematic. |
| `BSPD BOM_smd.csv` | The surface-mount parts only. |
| `fab/gerbers/` | Gerber and drill files for making the bare PCB. |
| `fab/BSPD_BOM.csv` | BOM for assembly. |
| `fab/BSPD_CPL.csv` | Pick-and-place (component positions) for assembly. |
| `analysis/helpers/bspd_system.cir` | The whole-board simulation. |

The `datasheets/` folder and the rest of `analysis/` exist on some team members' machines but aren't in git.

**After any change to the schematic or PCB, regenerate everything in `fab/` and both root BOMs.** The commands below reproduce the committed files exactly, apart from timestamps. Run them from inside `BSPD_KR/`. On macOS, `kicad-cli` lives at `/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli`.

```sh
# Gerbers (copper, mask, paste, silkscreen, outline) and the gerber job file
kicad-cli pcb export gerbers --no-protel-ext --subtract-soldermask \
  -l F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts \
  -o fab/gerbers/ "BSPD KiCAD.kicad_pcb"

# Drill file and drill map
kicad-cli pcb export drill --generate-map --map-format gerberx2 \
  -o fab/gerbers/ "BSPD KiCAD.kicad_pcb"

# Pick-and-place
kicad-cli pcb export pos --format csv --units mm --side both \
  -o fab/BSPD_CPL.csv "BSPD KiCAD.kicad_pcb"

# Assembly BOM
kicad-cli sch export bom \
  --fields 'Reference,Value,Footprint,Part Number,Datasheet,${QUANTITY}' \
  --labels 'Reference,Value,Footprint,MPN,Datasheet,Qty' \
  --group-by 'Value,Footprint,Part Number' --ref-range-delimiter '-' \
  -o fab/BSPD_BOM.csv "BSPD KiCAD.kicad_sch"

# Root BOM (same column layout as the original)
kicad-cli sch export bom \
  --fields 'Reference,Value,Part Number,Datasheet,Footprint,${QUANTITY},${DNP}' \
  --labels 'Reference,Value,Part Number,Datasheet,Footprint,Qty,DNP' \
  --group-by 'Value,Footprint,Part Number' --ref-range-delimiter '' \
  -o "BSPD BOM.csv" "BSPD KiCAD.kicad_sch"
```

`BSPD BOM_smd.csv` is the root BOM filtered down to the surface-mount parts: Q1, U4, U6, U9, U18, U20 and U21.

---

## 8. Glossary

| Term | Meaning |
|---|---|
| AIR | Accumulator Isolation Relay. One of the big relays that connect the high-voltage battery to the car. They are held closed by the shutdown circuit. |
| AMS | Accumulator Management System. Monitors the high-voltage battery. Also in the shutdown circuit. |
| BOM | Bill of Materials. The parts list. |
| BSPD | Brake System Plausibility Device. This board. |
| CPL | Component Placement List. Where each part goes on the board, for assembly machines. Also called pick-and-place. |
| DRC | Design Rule Check. KiCad's check that the PCB layout follows the manufacturing rules. |
| ERC | Electrical Rule Check. KiCad's check that the schematic is wired sensibly. |
| ESF | Electrical System Form. The document the team submits describing the car's electrical systems and failure modes. |
| Footprint | The copper pads and outline for one part on the PCB. |
| Gerber | The standard file format PCB manufacturers use to make bare boards. |
| GLV | Grounded Low Voltage. The car's low-voltage system. |
| IMD | Insulation Monitoring Device. Detects leakage between the high-voltage system and the chassis. |
| LVMS | Low Voltage Master Switch. Turns the whole low-voltage system on and off. Power-cycling it resets the BSPD. |
| Net | One electrical connection in the design: every pin and track that is joined together. |
| POR | Power-On Reset. The circuit that puts the latch into a known state when power comes on. |
| SCS | System Critical Signal. A signal whose failure must put the car into a safe state (T11.9). |
| SDC | Shutdown Circuit. The safety loop that holds the AIRs closed. |
| TS | Tractive System. Everything on the high-voltage side. |
| TSAC | Tractive System Accumulator Container. The high-voltage battery box. |
| VCU | Vehicle Control Unit. The car's main controller (a Teensy). The BSPD does not depend on it. |
