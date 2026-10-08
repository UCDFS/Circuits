# BSPD: Brake System Plausibility Device

This folder holds the KiCad design for the car's BSPD board. The original design is by Nebojša Cvetković, later updated by Keelan Reilly (hence the `_KR` in the folder name). It was reworked in September and October 2026: first to fix three rule and reliability problems, then so that a faulty or unplugged sensor trips the BSPD on its own.

> **Status (8 October 2026):** the design passes KiCad's checks and behaves correctly in simulation, including the sensor-fault tests. **No board has been built or bench-tested yet.** Read [section 5](#5-open-issues-read-before-ordering) before ordering. [Issue 1](#issue-1-the-current-sensor-must-stay-below-474-v-in-normal-driving) needs an answer from the team first.

## The short version of what changed

**September 2026 (the latch rework):**

1. **The old board reset itself too early.** After tripping, it closed the shutdown circuit again by itself after roughly 7 to 14 seconds, depending on part tolerances. FSUK rule T11.6.1 only allows a self-reset after more than 10 seconds. The new board stays tripped until the LV master switch (LVMS) is turned off and on again.
2. **The old board was too slow to trip.** Its "500 ms" timer actually took about 0.51 s with exactly nominal parts, which is over the limit in T11.6.2. The new timer takes about 0.36 s (0.29 s to 0.43 s across part tolerances).
3. **One capacitor was drawn backwards.** C10 is an electrolytic capacitor, so it has a + leg and a − leg. Its + leg was connected to ground. That is fixed.

**October 2026 (sensor faults):**

4. **A single unplugged or faulty sensor now trips the BSPD.** Before, an unplugged brake sensor looked to the board like "braking hard", and an unplugged current sensor looked like "high power". The board then waited for the other condition before doing anything. Now, any sensor reading below 0.24 V (wire broken, connector unplugged, short to ground) or above 4.74 V (short to the sensor's 5 V supply) trips the BSPD after about 0.16 s. The trip is latched like any other. This is what rule T11.9.2 asks for.
5. **The "sensor missing" level moved from 0.45 V to 0.24 V.** The brake sensors output 0.5 V at rest, so the old level left only 0.05 V of margin, and a healthy sensor could have read as missing.

The PCB, BOMs, manufacturing files and 3D model were all updated to match. [Section 4](#4-exactly-what-changed) lists every change.

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

That never happens in normal driving. If it does, something is broken. The throttle might be stuck, a pedal sensor might have failed, or there might be a bug in the VCU software. The BSPD does not try to work out which. It just opens the shutdown circuit.

It also has to notice when one of its own sensors fails. A broken sensor wire could otherwise blind it without anyone knowing.

It is deliberately built without any microcontroller or code. The BSPD is the backstop for software failures, so if it ran software, the same kind of bug it is meant to catch could also disable it.

### What the rules require (FSUK 2026, sections T11.6 and T11.9)

| Rule | What it says | How this board meets it |
|---|---|---|
| T11.6.1 | Must be a standalone, non-programmable circuit. Must open the SDC when hard braking happens while at least 5 kW is going to the motors. Then it must either stay open until the LVMS is power-cycled, or reset itself only after the condition has been gone for more than 10 s. | Comparators and logic gates detect the condition, and a latch keeps the SDC open until power-cycle. There is no software anywhere on the board. |
| T11.6.2 | Must open the SDC if the implausibility lasts more than 500 ms. | An RC timer trips it after about 0.36 s. |
| T11.6.3 | Must be powered directly from the LVMS. | Power comes in on J3. That must be wired straight from the LVMS. |
| T11.6.4 | No extra functions on the board. Only power, the required sensors and the shutdown circuit connect to it. | The only connectors are J1 (sensors), J2 (shutdown circuit) and J3 (power). |
| T11.6.5 | Hard braking must be measured with a brake pressure sensor. The threshold must be 30 bar or less, and below the point where the wheels lock. | There are two pressure inputs (front and rear), each with its own adjustable threshold. |
| T11.6.6 | Power must be measured with a DC current sensor. The threshold must equal 5 kW at the maximum TS voltage. | There is one current input with an adjustable threshold. |
| T11.6.7 | It must be possible to disconnect each sensor wire separately at inspection. | Every sensor comes in through connector J1. Unplugging any one of them trips the BSPD within about 0.16 s. |
| T11.6.8, T11.9.2, T11.9.5 | The sensor signals are System Critical Signals. An open circuit, a short to ground, or (for analogue sensors) a short to the supply voltage must put the system into its safe state, which for the BSPD is an open shutdown circuit. | Six "sensor fault" comparators watch the three sensors for readings below 0.24 V or above 4.74 V and trip the latch ([stage 5](#stage-5-sensor-fault-detection)). |
| T11.6.9, IN4.1.3 | At inspection, the team proves the BSPD works by injecting a fake current signal (equivalent to 5 kW) while pressing the brake pedal. | See [section 6](#6-calibrating-and-bench-testing-a-new-board). |

The full rules PDF is at the root of this repository.

---

## 2. Background you need

Every idea in this section is used in section 3. Skip any you already know.

### Voltage and ground

Voltage is always measured between two points. When we say a wire is "at 5 V", we mean 5 V above **ground**, the shared reference wire. On this board ground is called **GLV−**. One part of the schematic labels it **GND** instead, but KiCad joins the two names, and they are the same wire.

### The two supplies on this board

- **GLV+** is the car's low-voltage supply. It comes in on J3. It powers the relay coil (a 12 V part) and all four comparator chips.
- **+5V** is made on the board from GLV+ by **U4**, an LM7805 voltage regulator. A regulator takes in a higher voltage that may wobble and puts out a steady fixed voltage. Everything that does logic runs from +5V, and so do all the reference voltages.

### Resistors and voltage dividers

Put two resistors in series between +5V and ground, and the point between them sits at a fixed fraction of 5 V:

```
V_middle = 5 V × R_bottom / (R_top + R_bottom)
```

This board uses dividers to make fixed **reference voltages** that signals get compared against. For example, R7 (100 kΩ, on top) and R8 (5.1 kΩ, on the bottom) give 5 × 5.1 / 105.1 = **0.24 V**.

A **potentiometer** (RV1, RV2, RV3) is an adjustable divider. Turning its screw moves the middle pin (the "wiper") anywhere between 0 V and 5 V. These three set the trip thresholds. They are 25-turn trimmers, so each one takes many turns to go from end to end. That makes them easy to set accurately.

### Pull-up and pull-down resistors

A wire that isn't connected to anything is "floating". Its voltage is undefined, and it picks up electrical noise. A **pull-down** resistor (to ground) or a **pull-up** resistor (to +5V) gives the wire a default voltage that anything stronger can override.

- R1, R2 and R3 (100 kΩ) pull each sensor input down to 0 V. An unplugged sensor therefore reads 0 V instead of random noise. This is what lets the board detect an unplugged sensor.
- R13, R14, R15, R26, R27 and R31 (10 kΩ) pull comparator outputs up to 5 V. The next part explains why those are needed.

### Comparators (U5, U19, U22, U23)

A comparator has two inputs, marked **+** and **−**. It answers one question: **is the + input higher than the − input?** U5 and U19 (LM339) each hold four separate comparators, labelled A, B, C and D in the schematic. U22 and U23 (LM393) are the same kind of comparator, with two per chip, labelled A and B.

All of them have what is called an **open-collector output**. This confuses everyone at first, so here it is slowly:

- If **+ is higher than −**, the output lets go of its wire. The pull-up resistor then pulls the wire up to 5 V. **Output = HIGH.**
- If **− is higher than +**, the output connects its wire to ground. **Output = LOW.**

Because an open-collector output can only ever pull *down*, several outputs can safely be joined onto the same wire. If **any** of them pulls low, the wire goes low. This is called a **wired-OR**. This board uses it twice: six fault comparators share one `FAULT_N` wire, and two comparators share the `TRIP_REQ_N` wire.

### Active-low signals

A signal is **active-low** if LOW means "yes, this is happening". Most comparator outputs on this board are active-low: the wire goes LOW when the brake is pressed hard, when the current is high, or when a sensor is faulty. Names ending in `_N` (like `FAULT_N` and `TRIP_REQ_N`) follow this convention. It is the main thing that makes the logic look backwards.

### Logic gates

Logic chips treat a voltage near 5 V as **1 (HIGH)** and a voltage near 0 V as **0 (LOW)**. Each logic chip on this board is a tiny 5-pin package holding exactly one gate.

| Gate | Chips | The output is HIGH when… |
|---|---|---|
| NOT (inverter) | U9 (SN74LVC1G04) | the input is LOW |
| AND | U18 (SN74AHCT1G08) | both inputs are HIGH |
| NOR | U6, U20, U21 (SN74AHC1G02) | both inputs are LOW |

### Capacitors, RC timers and debouncing

A capacitor stores charge. If you charge (or drain) it through a resistor, its voltage changes gradually rather than instantly. How fast it changes depends on the **time constant**, written τ (the Greek letter tau):

```
τ = R × C
charging: V(t) = V_supply × (1 − e^(−t/τ))      draining: V(t) = V_start × e^(−t/τ)
```

After one τ the capacitor has covered 63% of the distance. Add a comparator watching the capacitor's voltage and you have a **timer**. It answers the question "has this input been on for longer than X?"

The same trick with a shorter time is called **debouncing**. It means ignoring blips that are too short to matter, like a connector that loses contact for a few milliseconds over a bump. This board uses a 0.36 s timer for the brake-plus-power check and a 0.16 s debounce for sensor faults.

**Electrolytic capacitors** (C10 and C13 here, both 10 µF) store a lot of charge for their size, but they are **polarised**. One leg is + and must always be at the higher voltage. Fitted backwards, they leak, heat up, and can eventually fail or burst. On this PCB, the + pad of each one is the square pad, and it is marked with + on the silkscreen.

The small ceramic capacitors are not polarised. Most (0.1 µF: C5, C6, C7, C8, C9, C12, C14 and C16; 0.22 µF: C4) sit right next to a chip's power pins as **decoupling** capacitors. Each one is a tiny local reservoir that smooths out the current spikes a chip draws every time it switches. C15 (1 µF) is different: it is the debounce capacitor for sensor faults.

### Chip packages: DIP and SOIC

Most chips on this board are **DIP** (through-hole) parts, with two rows of legs that go through holes in the board. U22 and U23 are **SOIC-8** surface-mount parts. They are much smaller and are soldered flat onto pads on the **bottom** of the board. Each one sits in the empty space between the two rows of legs of a DIP chip (U5 and U19), because that is where its connections are.

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

### The main path: brake plus power

```
 CURRENT_SENSOR (J1 pin 3)      BRAKE_SENSOR_FRONT (J1 pin 1)     BRAKE_SENSOR_REAR (J1 pin 2)
           │                                │                                  │
 Stage 1   │  input protection (1 kΩ in series, 100 kΩ pull-down, test point)  │
           │                                │                                  │
 Stage 2   U5A                              U19A                               U19C
           "current too high"               "front pressure too high"          "rear pressure too high"
           LOW = yes  (LED1)                LOW = yes  (LED2)                  LOW = yes  (LED3)
           │                                └────────────────┬─────────────────┘
           │                                        Stage 3a: U18 AND
           │                                        "either brake pressed hard", LOW = yes
           └──────────────────────┬──────────────────────────┘
                         Stage 3b: U6 NOR
                         IMPLAUSIBLE: HIGH = hard braking AND high power, right now
                                  │
                         Stage 4: R20 + C10 timer, watched by U5C
                         "implausible for longer than ~0.36 s"
                                  │
                         TRIP_REQ_N  (LOW = trip, please; LED4)  ◄──── Stage 5: sensor faults (below)
                                  │
                         U9 NOT  →  BSPD_TRIP  (HIGH = trip now)
                                  │
                         Stage 6: U20 + U21 latch  →  LATCH_Q  (HIGH = tripped, remembered)
                                  │      ↑ POR (C13, R30, D10) forces it to "OK" at power-up
                                  │
                         Stage 7: U5D  →  Q1  →  K1 relay  →  J2  →  shutdown circuit
                         tripped: relay off, shutdown circuit open  (LED5)
```

### The fault path: is each sensor healthy?

```
 each sensor ──► "reading below 0.24 V?"  U5B (current), U19B (front), U19D (rear)  ──┐
          └────► "reading above 4.74 V?"  U22A (current), U23A (front), U23B (rear) ─┤  all six outputs joined:
                                                                                     │  FAULT_N (LOW = a sensor is faulty)
                                                                                     ▼
                                               R32 + C15 debounce (~0.16 s), watched by U22B
                                                                                     │
                                                                                     ▼
                                               pulls TRIP_REQ_N LOW  →  same latch and relay as a real trip
```

Many wires in the schematic have no name, and KiCad numbers them automatically (`Net-(U6-…)` and similar). The descriptions below name each wire by what it does and by the pins it joins.

### Stage 0: Power

| Part | Job |
|---|---|
| J3 | Power input. Pin 1 is GLV+ and pin 2 is GLV−. It must be supplied directly from the LVMS (T11.6.3). |
| U4 (LM7805) | Makes +5V from GLV+. |
| C4 (0.22 µF) | Input capacitor for U4, on GLV+. |
| C7, C12 (0.1 µF) | On GLV+. Decoupling for the comparator chips U5 and U19, which run directly from GLV+ (pin 3 on each). C7 also serves U22, which sits inside U5's footprint and takes its power straight from U5's supply pins. |
| C16 (0.1 µF) | On GLV+. Decoupling for U23. |
| C5, C6, C8, C9 (0.1 µF) | On +5V. Output capacitor for U4, and decoupling for the logic gates U6, U9 and U18. |
| C14 (0.1 µF) | On +5V. Decoupling for the two latch chips, U20 and U21. |

The comparators run from GLV+, but they only ever see 0 to 5 V on their inputs, which is well within what they accept. Their outputs are pulled up to +5V, so the logic chips also only ever see 0 to 5 V.

### Stage 1: Sensor inputs

| Signal | J1 pin | Series resistor | Pull-down | Test point (live signal) | Net name after the 1 kΩ |
|---|---|---|---|---|---|
| BRAKE_SENSOR_FRONT | 1 | R5, 1 kΩ | R2, 100 kΩ | TP2 | `SENS_FRONT` |
| BRAKE_SENSOR_REAR | 2 | R6, 1 kΩ | R3, 100 kΩ | TP3 | `SENS_REAR` |
| CURRENT_SENSOR | 3 | R4, 1 kΩ | R1, 100 kΩ | TP1 | `SENS_CUR` |

The 1 kΩ series resistors limit the current into the comparator inputs if a wire picks up a spike or gets shorted to something. The 100 kΩ pull-downs make an unplugged sensor read 0 V.

J1 carries only the three signal wires. The sensors get their power and ground from elsewhere in the car's wiring.

**What the sensors output.**
- The brake sensors are TE M7139-500PG transducers: 0.5 V at 0 bar, rising to 4.5 V at 500 psi (34.5 bar).
- The current sensor is the Orion BMS Hall-effect sensor: 2.5 V at 0 A, changing by 10 mV per amp.
- So a healthy sensor always reads somewhere between 0.5 V and 4.5 V. That is what makes it possible to treat anything outside that band as a fault.

### Stage 2: "Too high" comparators

Each sensor has one comparator that asks "is the signal above its threshold?" The sensor goes into the **−** input and the adjustable threshold (from a potentiometer) goes into the **+** input. When the signal rises above the threshold, − is higher than +, so the output pulls LOW.

| Sensor | Comparator (− pin, + pin, output pin) | Threshold set by / measured at | Output pull-up | LED (via resistor) |
|---|---|---|---|---|
| Current | U5A (4, 5, 2) | RV1 / TP4 | R13 | LED1 (R16) |
| Front brake | U19A (4, 5, 2) | RV2 / TP5 | R14 | LED2 (R17) |
| Rear brake | U19C (10, 11, 13) | RV3 / TP6 | R15 | LED3 (R18) |

Each LED has its + leg (anode) on +5V and its − leg (cathode) going through a 10 kΩ resistor to the comparator output wire. The LED lights when that wire goes LOW.

### Stage 3: Combining the signals

**U18, an AND gate**, takes the front-brake wire and the rear-brake wire. Its output is HIGH only when both brake wires are HIGH, meaning neither brake is pressed hard. If either one goes LOW, U18's output goes LOW. So U18's output is an active-low "**either brake pressed hard**" signal.

**U6, a NOR gate**, takes the current wire and U18's output. Its output is HIGH only when both inputs are LOW:

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
- **U5C** compares C10's voltage (− input, pin 10) against a fixed **3.0 V** (+ input, pin 11). The 3.0 V comes from the R22 (10 kΩ) and R23 (15 kΩ) divider: 5 × 15 / 25 = 3.0 V.
- C10 reaches 3.0 V after t = τ × ln(5 / (5 − 3)) = 0.39 × 0.916 = **0.36 s**.
- At that point U5C's output (pin 13) pulls the `TRIP_REQ_N` wire LOW and LED4 lights (through R28). **U9** inverts that, so its output, **BSPD_TRIP**, goes HIGH.
- If IMPLAUSIBLE goes away before 0.36 s, U6's output goes LOW and C10 drains back out through R20. A short blip is ignored.
- Electrolytic capacitors are only accurate to ±20%, so C10 is really somewhere between 8 µF and 12 µF. That puts the trip time between **0.29 s and 0.43 s**, always under the 500 ms limit.
- If a second implausibility comes shortly after a first one, C10 may not have fully drained yet, so the second trip comes a little sooner. That is the safe direction.

### Stage 5: Sensor fault detection

Every sensor is checked for two kinds of fault:

- **Reading too low (below 0.24 V):** the wire is broken, the connector is unplugged, or the signal is shorted to ground. All three read close to 0 V, because the 100 kΩ pull-down takes over.
- **Reading too high (above 4.74 V):** the signal is shorted to the sensor's 5 V supply.

A healthy sensor never reads outside 0.5 V to 4.5 V, so each limit sits roughly halfway between "healthy" and "faulty".

| Sensor | "Too low" comparator (+ = sensor, − = 0.24 V) | 0.24 V reference | "Too high" comparator (− = sensor, + = 4.74 V) | 4.74 V reference |
|---|---|---|---|---|
| Current | U5B (+ 7, − 6, out 1) | R7 100 kΩ / R8 5.1 kΩ | U22A (− 2, + 3, out 1) | R33 1 kΩ / R34 18 kΩ (`OVR_REF_CUR`) |
| Front brake | U19B (+ 7, − 6, out 1) | R9 100 kΩ / R10 5.1 kΩ | U23A (− 2, + 3, out 1) | R35 1 kΩ / R36 18 kΩ (`OVR_REF_BRK`) |
| Rear brake | U19D (+ 9, − 8, out 14) | R11 100 kΩ / R12 5.1 kΩ | U23B (− 6, + 5, out 7) | R35 / R36 (`OVR_REF_BRK`, shared) |

The references work out like this:
- 0.24 V = 5 V × 5.1 / 105.1
- 4.74 V = 5 V × 18 / 19

The comparators are wired so that a fault always pulls the output LOW. For "too low", the sensor goes to +, so when it drops under the reference, − is higher. For "too high", the sensor goes to −.

**All six outputs are joined onto one wire, `FAULT_N`**, which **R31 (10 kΩ)** pulls up to 5 V. If any sensor is faulty, `FAULT_N` goes LOW.

**Debounce.** `FAULT_N` feeds **R32 (100 kΩ)**, which charges **C15 (1 µF)**. The wire between them is `FAULT_F`.
- Normally C15 sits at 5 V. When `FAULT_N` goes LOW, C15 drains through R32 with τ = 100 kΩ × 1 µF = **0.1 s**.
- **U22B** compares `FAULT_F` (+ input, pin 5) with the 1.02 V reference `REF_1V0` (− input, pin 6). That is the same reference U5D uses, made by R24 and R25.
- C15 falls below 1.02 V after about 0.1 × ln(5 / 1.02) ≈ **0.16 s** (0.14 s to 0.18 s with C15's ±10% tolerance).
- At that point U22B's output (pin 7) pulls **`TRIP_REQ_N`** LOW. That is the very same wire the 500 ms timer uses, so from here a sensor fault is handled exactly like a real trip: U9 → BSPD_TRIP → latch → relay opens. LED4 and then LED5 light.
- A blip shorter than about 0.12 s doesn't drain C15 far enough to matter, so a connector that loses contact for a moment over a bump does not trip the car.

**At power-up**, C15 starts empty, so for the first ~25 ms the debounce output says "fault". The power-on reset (stage 6) holds the latch in "OK" for far longer than that, so this does nothing. Sensors that come alive a little after the board are also fine, as long as they are valid before the power-on reset lets go (at least ~0.36 s; simulated with sensors 0.2 s late).

**Where the new chips are.**
- U22 sits on the bottom of the board, inside U5's footprint. U5's pins carry almost everything it needs: `SENS_CUR`, `REF_1V0`, `TRIP_REQ_N`, `FAULT_N` (U5 pin 1) and power.
- U23 sits on the bottom inside U19's footprint, for the same reason: `SENS_FRONT`, `SENS_REAR`, `FAULT_N` (U19 pins 1 and 14) and power.
- U23 has its own decoupling capacitor, C16. U22 shares U5's supply pins and U5's capacitor C7.

### Stage 6: The latch

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
- If a real fault is present at power-up, nothing goes wrong. The trip request is still there when POR lets go, so the latch sets as soon as the reset ends.

### Stage 7: Driving the relay

- **U5D** compares LATCH_Q (− input, pin 8) against the fixed **1.02 V** `REF_1V0` (+ input, pin 9). That reference comes from the R24 (39 kΩ) and R25 (10 kΩ) divider: 5 × 10 / 49 = 1.02 V. U5D is just acting as an inverter: when LATCH_Q is HIGH (tripped), its output (pin 14) pulls LOW.
- Pin 14's wire is **Q1's gate**, pulled up to +5V by R27 (10 kΩ).
- **Not tripped:** R27 holds Q1's gate at 5 V, so Q1 conducts. Current flows from GLV+ through K1's coil and Q1 to ground. The relay is closed, so the shutdown circuit is closed through J2.
- **Tripped:** U5D pulls Q1's gate LOW, so Q1 turns off and the coil current stops. K1 opens, the shutdown circuit opens, the AIRs open, and the high voltage is disconnected. LED5 lights (through R29).
- **J2** connects to the shutdown circuit. Pin 1 is BSPD_Shutdown+ and pin 2 is BSPD_Shutdown−.

Why does a comparator drive Q1, rather than the latch driving it directly? On the old board, U5D watched the 10-second reset timer. Reusing it kept the PCB changes small, and it also makes a convenient 5 V gate driver.

### What the LEDs mean

All five LEDs are green. They run at only about 0.3 mA, so they are dim, especially in daylight.

| LED | When it is on |
|---|---|
| LED1 | The current is above its threshold. |
| LED2 | The front brake pressure is above its threshold. |
| LED3 | The rear brake pressure is above its threshold. |
| LED4 | **A trip is being requested:** either the implausibility has lasted at least ~0.36 s, or a sensor has been faulty for at least ~0.16 s. It goes off again once the cause clears. |
| LED5 | **The BSPD has tripped and latched.** The relay and the shutdown circuit are open. It stays on until the LVMS is power-cycled. |

There is no separate "sensor fault" LED; there was no room on the top of the board. If LED5 is on but LED1, LED2 and LED3 are all off, a sensor fault is the likely cause. Check TP1, TP2 and TP3 with a meter.

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

This section compares the current design with the version on GitHub `main` as of Keelan's last commit there (`f4da82b`, 9 July 2026). The work came in two batches: **A** (September, the latch rework) and **B** (October, sensor faults).

### 4.1 Behaviour before and after

| | Before | After |
|---|---|---|
| Time from implausibility to trip | ~0.51 s with nominal parts (0.41–0.62 s across tolerances). **Over the 500 ms limit.** | ~0.36 s with nominal parts (0.29–0.43 s across tolerances). |
| What happens after a trip | Resets itself roughly 7–14 s after the condition clears. **Can be under the 10 s the rules require.** | Stays tripped until the LVMS is power-cycled. |
| C10 | Fitted backwards, so it was reverse-biased (by up to 5 V) whenever an implausibility happened. | Fitted the right way round. |
| Power-up | The relay stayed open for about 16 s after every power-up. C11 connected +5V to the 10-second node, so switching on dragged that node to 5 V, and it then had to drain through R21 like a trip. Confirmed in simulation. | The POR circuit forces the latch to "OK", so the relay closes within a few milliseconds of power-up. |
| One sensor unplugged, or shorted to ground | **Did not trip.** The board treated it as "braking hard" or "high power" and waited for the other condition. | Trips after ~0.16 s and stays tripped. |
| One sensor shorted to its 5 V supply | **Did not trip**, for the same reason. | Trips after ~0.16 s and stays tripped. |
| "Sensor missing" level | 0.45 V, only 0.05 V below a brake sensor's resting 0.5 V. | 0.24 V. |

### 4.2 Batch A: the latch rework (September 2026)

**Schematic: removed (the old 10-second self-reset timer)**

| Ref | Part | What it did |
|---|---|---|
| Q3 | 2N7002 MOSFET | When the BSPD tripped, it charged up the 10-second node from +5V. |
| R19 | 1 kΩ | Series resistor between Q3 and the 10-second node. |
| C11 | 10 µF electrolytic | The 10-second capacitor, between +5V and the `10s` net. |
| R21 | 1 MΩ | Slowly drained the 10-second node. τ = 1 MΩ × 10 µF = 10 s. |

Why it didn't work: the hold time was 10 s × ln(V_start / 1.02 V), where V_start was the voltage Q3 charged the node to. Q3 could only lift the node to about 5 V minus its own gate threshold voltage, and that threshold varies from one 2N7002 to the next (roughly 1 V to 2.5 V). Combined with C11's ±20% tolerance, the hold time came out anywhere from about 7 s to about 14 s.

**Schematic: added**

| Ref | Part | Purpose |
|---|---|---|
| U20, U21 | SN74AHC1G02DBVR NOR gates, SOT-23-5 (bottom side) | The latch. Same part as the existing U6. |
| C13 | 10 µF electrolytic, Panasonic ECE-A1EKA100I | POR capacitor, from +5V to `POR`. Same part as C10. |
| R30 | 100 kΩ | POR resistor, from `POR` to ground. |
| D10 | 1N4148 diode | Clamps `POR` at power-off. |
| C14 | 0.1 µF ceramic | Decoupling for U20 and U21. |

**Schematic: changed**

| What | Before | After | Why |
|---|---|---|---|
| R20 | 56 kΩ | 39 kΩ | 56 kΩ × 10 µF × 0.916 = 0.51 s, over the limit. 39 kΩ gives 0.36 s. |
| C10 orientation | + pin on GLV−, − pin on the `500ms` node | + pin on the `500ms` node, − pin on GLV− | It was reverse-biased. |
| U9 output (pin 4) | Drove Q3's gate | Drives the new `BSPD_TRIP` net into U21 pin 2 (the latch's set input) | The latch replaces the timer. |
| U5 pin 8 (U5D's − input) | The `10s` timer node | `LATCH_Q` | Same comparator and same 1.02 V reference, now watching the latch. |

In the first fix, C13 was 220 nF and R30 was 470 kΩ. A whole-board simulation showed the reset pulse could be too weak if the 5 V supply came up slowly, so they became 10 µF and 100 kΩ. A second decoupling capacitor, also called C15 at the time, was briefly added for the latch and then removed. The name C15 is now used by batch B.

**PCB:**
- U20 and U21 are on the bottom, under R30 and D10. R30, D10 and C13 sit where R19, R21 and C11 were, with C14 just below. J2 moved 0.5 mm to make room for C13.
- New connections were routed with Freerouting; existing routing was kept.
- All tracks are 0.25 mm or wider. The default net class used to say 0.2 mm, below the board's own minimum.
- Via drill went from 0.25 mm to 0.3 mm, to meet the board's minimum hole size.
- The board outline (Edge.Cuts) was closed. Some of its segments didn't quite meet.
- The footprint text fields were synced with the schematic. For example, R30's value read 470k, and D10 carried a leftover 1N4001 part number.

**Other files:**
- `BSPD BOM.csv` and `BSPD BOM_smd.csv` were regenerated. The old ones still listed Q3, R19, R21, C11 and R20 at 56k.
- `fab/` was added.

### 4.3 Batch B: sensor faults trip the BSPD (October 2026)

**Schematic: rewired**

| Pin | Comparator | Before | After |
|---|---|---|---|
| U5 pin 1 | U5B, "current sensor too low" | Joined to the "current too high" wire (U5 pin 2, R13, LED1, U6) | `FAULT_N` |
| U19 pin 1 | U19B, "front sensor too low" | Joined to the "front too high" wire (U19 pin 2, R14, LED2, U18) | `FAULT_N` |
| U19 pin 14 | U19D, "rear sensor too low" | Joined to the "rear too high" wire (U19 pin 13, R15, LED3, U18) | `FAULT_N` |

The three short wires that made those joins were deleted, along with the three junction dots that no longer joined anything. Each output now carries a `FAULT_N` label.

**Schematic: changed values**

| Ref | Before | After | Why |
|---|---|---|---|
| R8, R10, R12 | 10 kΩ (MCF 0.25W 10K) | 5.1 kΩ (MCF 0.25W 5K1) | "Sensor missing" level from 0.45 V to 0.24 V. |

**Schematic: added**

| Ref | Part (part number) | Footprint | Purpose |
|---|---|---|---|
| U22 | LM393 dual comparator (LM393DR) | SOIC-8, bottom | A: current sensor above 4.74 V. B: debounce comparator that pulls `TRIP_REQ_N`. |
| U23 | LM393 dual comparator (LM393DR) | SOIC-8, bottom | A: front sensor above 4.74 V. B: rear sensor above 4.74 V. |
| R31 | 10 kΩ (RC0805FR-0710KL) | 0805, bottom | `FAULT_N` pull-up. |
| R32 | 100 kΩ (RC0805FR-07100KL) | 0805, bottom | Debounce resistor. |
| C15 | 1 µF X7R 50 V (CL21B105KBFNNNE) | 0805, bottom | Debounce capacitor. |
| R33, R34 | 1 kΩ, 18 kΩ (RC0805FR-071KL, RC0805FR-0718KL) | 0805, bottom | 4.74 V reference for the current sensor (`OVR_REF_CUR`). |
| R35, R36 | 1 kΩ, 18 kΩ (same parts) | 0805, bottom | 4.74 V reference for the brake sensors (`OVR_REF_BRK`). |
| C16 | 0.1 µF X7R 50 V (CL21B104KBCNNNC) | 0805, bottom | Decoupling for U23. |

**Schematic: nets**
- New: `FAULT_N`, `FAULT_F`, `OVR_REF_CUR`, `OVR_REF_BRK`.
- Five existing wires that used to be unnamed were given names so the new parts could join them. Their connections didn't change:

| New name | Was | What it is |
|---|---|---|
| `SENS_CUR` | `Net-(U5A--)` | Current sensor after R4 |
| `SENS_FRONT` | `Net-(U19A--)` | Front sensor after R5 |
| `SENS_REAR` | `Net-(U19C--)` | Rear sensor after R6 |
| `REF_1V0` | `Net-(U5D-+)` | The 1.02 V reference from R24/R25 |
| `TRIP_REQ_N` | `Net-(R26-Pad1)` | U5C's output, U9's input, R26, LED4 |

**Schematic: other**
- KiCad's LM393 symbol was embedded in the schematic.
- A "Sensor Fault Detection" block was added below the existing circuit.
- The note "Anything below 0.455 volts is invalid" now reads "Below 0.24 V or above 4.74 V = sensor fault (trips the BSPD)".
- The title block changed from rev v05.1 to v06, dated 2026-10-08.

**PCB:**
- **10 new footprints, all on the bottom side.**
  - Inside U19's footprint: U23, C16, R35 and R36.
  - Inside U5's footprint: U22 and R31.
  - Just west of U5, near TP1: R32, C15, R33 and R34.
  - Footprints went from 73 to 83.
- **Three hand reroutes.** U5 pin 1, U19 pin 1 and U19 pin 14 each used to be the point where their sensor's wire branched. Each branch was moved one pin over so the moved pin could be freed:
  - **Current wire:** the junction moved from U5 pin 1 to U5 pin 2. That's a top-side diagonal into pin 2, plus a bottom-side jog that keeps clear of pin 1.
  - **Front wire:** U19 pin 2 now joins the existing bottom-side run at y = 49.05 mm by passing between R2's pad and U19 pin 1.
  - **Rear wire:** U19 pin 13 now jogs round the outside of pin 14 on the bottom side to reach the existing run north of it.
  - 13 old segments were removed (two of them were zero-length stubs) and 10 hand-drawn ones added.
- **New connections routed with Freerouting,** with every existing track locked so it couldn't move. That added 132 segments and 6 vias. Totals: segments 379 → 508, vias 8 → 14. All tracks are 0.25 mm and all vias 0.65 mm / 0.3 mm, the same as the rest of the board.
- **R8, R10 and R12** now show 5.1k and part number MCF 0.25W 5K1.
- **Silkscreen.** The new parts' reference labels are 0.8 mm tall. U22 and U23 have theirs in the middle of the chip body. C15, R32 and R36 have no room on the silkscreen, so their labels are on the bottom fab layer, which shows on the assembly drawing.
- **Checks:**
  - DRC: 0 errors, 0 unconnected.
  - Schematic versus PCB: only the mounting holes H1–H4, which are PCB-only by design.
  - The warning list (library, silkscreen and text-size notes) is identical to before batch B.

**Simulation:**

| File | Status | What it is |
|---|---|---|
| `analysis/helpers/bspd_core.inc` | New | The shared model of the whole board, including the fault path. |
| `analysis/helpers/bspd_system.cir` | Updated | Now uses the shared model. It re-runs the original trip, latch and power-cycle timeline. |
| `analysis/helpers/bspd_fault_tests.cir` | New | Eleven sensor-fault scenarios. |

**Other files:**
- `BSPD BOM.csv`, `BSPD BOM_smd.csv` and everything in `fab/` were regenerated.
- `BSPD KiCAD.step` was regenerated. It now shows the current board; see [issue 9](#issue-9-small-known-nits-none-of-these-stop-the-board-working) for its missing part models.
- This README.

### 4.4 Commits

On the branch `bspd-latch-fix`:

| Commit | Date | What it did |
|---|---|---|
| `2082113` | 20 Sep 2026 | A: replaced the 10 s self-reset with the SR latch, and changed R20 from 56k to 39k. |
| `09bb96b` | 20 Sep 2026 | A: placed and routed the latch, closed the board outline, and moved to 0.3 mm vias. |
| `986bf25` | 22 Sep 2026 | A: fixed C10's polarity, strengthened the POR, and made every track 0.25 mm or wider. |
| `bad0fb1` | 7 Oct 2026 | A: synced the PCB text fields, regenerated the BOMs, and added the fab outputs and the simulation. |
| `25e2ff8` | 8 Oct 2026 | First version of this README. |
| (the commit that adds this text) | 8 Oct 2026 | B: sensor faults trip the BSPD, the "missing" level is now 0.24 V, the STEP is regenerated, and this README is updated. |

### 4.5 How the changes were checked

- **KiCad DRC:** 0 errors, 0 unconnected items.
- **KiCad ERC:** only the 2 errors that were there before any of this work ([issue 9](#issue-9-small-known-nits-none-of-these-stop-the-board-working)).
- **Schematic versus PCB:** checked with both KiCad and the kicad-happy cross-check. Clean apart from the mounting holes.
- **Pinouts:**
  - The existing chips were checked against their manufacturer datasheets.
  - The LM393 uses TI's standard SOIC-8 pinout, which matches KiCad's symbol: pin 1 output A, pins 2/3 inputs A (−/+), pin 4 ground, pins 5/6 inputs B (+/−), pin 7 output B, pin 8 supply.
- **Simulation of the whole board** (`analysis/helpers/bspd_system.cir`, ngspice). These results are the same as before batch B, so nothing regressed:

| Scenario | Should… | Result |
|---|---|---|
| Power on | close the relay | Closed 3.5 ms after power-up |
| Brake and current together for 300 ms | not trip | No trip. The timer peaked at 2.67 V, below the 3.0 V trip level. |
| Brake and current together for 1 s | trip within 500 ms | Tripped 0.32 s after it started |
| Condition removed after the trip | stay tripped | Still tripped 0.5 s and 1.9 s later |
| LV power-cycle | reset and close the relay | Relay closed 3.5 ms after power returned |
| All of the above | never show a false sensor fault | `FAULT_N` never dropped below 4.96 V |

- **Sensor-fault tests** (`analysis/helpers/bspd_fault_tests.cir`). The car is idle, with the current sensor at 2.5 V and the brakes at 0.6 V. Each fault starts at 1.0 s:

| Scenario | Should… | Result |
|---|---|---|
| Front brake sensor unplugged | trip | Tripped 0.16 s after the fault |
| Rear brake sensor unplugged | trip | Tripped 0.16 s after the fault |
| Current sensor unplugged | trip | Tripped 0.16 s after the fault |
| Front brake signal shorted to 5 V | trip | Tripped 0.16 s after the fault |
| Rear brake signal shorted to 5 V | trip | Tripped 0.16 s after the fault |
| Current signal shorted to 5 V | trip | Tripped 0.16 s after the fault |
| Front sensor drops out for 50 ms | not trip | No trip |
| Front sensor drops out for 120 ms | not trip | No trip |
| Both brakes at 4.0 V (about 30 bar), no current | not trip | No trip, no false fault |
| Both brakes at 4.5 V (sensor full scale), no current | not trip | No trip, no false fault |
| All sensors come alive 0.2 s after the board | not trip | No trip |

To run them yourself:

```
cd BSPD_KR/analysis/helpers
ngspice -b bspd_system.cir
ngspice -b bspd_fault_tests.cir
```

In the fault tests, "trip_at" is when the relay opened. Where a scenario shouldn't trip, ngspice prints a "measure … out of interval" message instead, because the relay never opened.

What hasn't been done: **no physical board has been built or tested.**

---

## 5. Open issues: read before ordering

### Issue 1: The current sensor must stay below 4.74 V in normal driving

The new "shorted to supply" check trips the BSPD if any sensor reads above 4.74 V. That limit actually lands somewhere between 4.55 V and 4.93 V, depending on the exact output of the 5 V regulator.

- **Current sensor:** at 10 mV per amp and 2.5 V at 0 A, 4.74 V is about **224 A** (205–243 A across the regulator tolerance). **If the car can draw more DC current than that, the BSPD will trip at full throttle.** 80 kW at 315 V is about 254 A. The firmware doesn't record the car's actual current limit, so this needs checking with the team.
- **Brake sensors:** 4.74 V is about **36.5 bar** (35–38 bar). If a driver can press harder than that, the BSPD will trip during a very hard stop. The VCU logs brake pressure in bar (`brake_f_bar`, `brake_r_bar`), so check the logged peaks.

If either limit can be reached, there are three options:
- **Use a sensor with a bigger range**, so that normal driving stays below 4.5 V.
- **Raise the limit** by changing R34 (current) or R36 (brakes). A bigger bottom resistor gives a higher limit. It must stay below the sensor's supply voltage, or a short to supply won't be caught.
- **Turn off the check for the current sensor only:** fit R33 as 0 Ω and leave R34 empty. That sets the limit to 5 V, which a sensor never reaches. It also means a current sensor shorted to supply is no longer caught, so the team would need to justify that in the ESF.

### Issue 2: The sensors must be valid within about 0.3 s of LV power-up

If a sensor still reads 0 V when the power-on reset lets go (at least ~0.36 s after power-up), the board treats it as unplugged and trips. A second power-cycle then clears it. The sensors get their power from elsewhere in the car's wiring, so check that their supply comes up with the LVMS rather than after something slower, like the VCU booting.

### Issue 3: The thresholds have to be set on the car

RV1, RV2 and RV3 are adjustable, and none of them has been set yet.

- **Current (RV1, measure at TP4):** set this to the sensor's output voltage at the current equivalent to 5 kW at maximum pack voltage. With the current pack (315 V maximum), that is 5000 W ÷ 315 V ≈ **15.9 A**, so about 2.5 V + 15.9 × 0.010 V ≈ **2.66 V**. Recalculate if the pack changes.
- **Brakes (RV2 at TP5, RV3 at TP6):** set these to each sensor's output voltage at your chosen pressure. The pressure must be 30 bar or less, and low enough that the wheels don't lock (T11.6.5). For the 0.5–4.5 V, 34.5 bar sensors: 20 bar ≈ 2.82 V, 25 bar ≈ 3.40 V, 30 bar ≈ 3.98 V.

### Issue 4: Not built or tested

So far the design has only been checked by DRC, ERC and simulation.

### Issue 5: Old physical boards don't match these files

Boards built from the earlier design have hand modifications that were never written down, such as cut tracks and added jumper wires. Don't use them as a reference for this design, and don't assume these files describe them.

### Issue 6: Assembly, including 12 parts on the bottom

There are now 12 surface-mount parts on the bottom: U20, U21, U22, U23, C15, C16 and R31–R36.
- **JLCPCB assembly:** this means two-sided assembly, which costs more.
- **Hand assembly:** solder everything on the bottom first.
- **Fit U22 and U23 before U5 and U19.** They sit between U5's and U19's legs, with about 1 mm to spare. Once U5 and U19 are in, those legs are in the way.
- **Trim U5's and U19's legs short** on the bottom, and keep their solder joints small.

The pick-and-place file marks the bottom parts as `bottom`.

### Issue 7: The inspection test must be set up before power-up

Unplugging the current sensor now trips the BSPD by itself, which is correct. That changes how to run the inspection test (T11.6.9):
1. With the LVMS off, connect the test voltage source to J1 pin 3 in place of the current sensor. Set it to the zero-current voltage, 2.5 V.
2. Power up.
3. Raise the source above the 5 kW threshold while pressing the brake pedal. The BSPD should trip within half a second.

### Issue 8: No sensor-fault LED

There was no room for one on the top. See "[What the LEDs mean](#what-the-leds-mean)" for how to tell a sensor fault apart from a real trip.

### Issue 9: Small known nits (none of these stop the board working)

- **2 ERC errors:** U4's input pin and one ground symbol report "power pin not driven". KiCad wants a PWR_FLAG symbol on GLV+ and GLV−. This is cosmetic.
- **1 ERC warning:** GLV− and GND are two names for the same wire. This is intended.
- **D9 is 0.84 mm from the board edge.** That works but is tighter than ideal for manufacturing.
- **C10 and C13 use the unpolarised capacitor symbol.** They are electrolytic, but the schematic doesn't show which side is +. The PCB footprints do show it, and both are the right way round.
- **No ground stitching vias.** The kicad-happy check notes this, but it's fine for a slow, simple 2-layer board.
- **The STEP model is missing 15 part bodies:** C4–C9, C12, C14, K1, LED1–LED5 and U19. Their footprints point to 3D files that only ever existed on Keelan's laptop (for example `/Users/keelan/Downloads/LIB_G5Q-1A4-DC12/...`). The board outline, every pad and every other part are correct. To fix it, add those 3D files to `Kicad_Libraries/` and point the footprints at them.

---

## 6. Calibrating and bench-testing a new board

### What you need

- A bench power supply set to 12 V, with its current limit at about 200 mA.
- A multimeter.
- Three adjustable 0 to 5 V sources to stand in for the sensors. For example, three 10 kΩ potentiometers across a separate 5 V supply, with that supply's ground joined to the board's GLV−. One of them needs to reach a solid 5 V, for the short-to-supply test.
- A continuity tester (or the multimeter's beep mode) across J2.
- Optionally, an oscilloscope, to measure the trip times.

### Steps

1. **Visual check before power.** Confirm that:
   - the + legs of C10 and C13 are in the square + pads
   - the stripes on D9 and D10 match the silkscreen
   - pin 1 of U5, U19, U22 and U23 is where the silkscreen dot says
   - U5's and U19's legs don't touch U22 or U23 underneath
2. **Power up with nothing in J1.** +5V (measured across C5) should read 4.9 to 5.1 V. All three sensor inputs read 0 V, which counts as "unplugged". Within about a second, once the power-on reset has let go, LED4 and LED5 should light and J2 should go open. That's a correct sensor-fault trip.
3. **Connect the stand-in sensors at resting values,** for example 0.6 V for each brake and 2.5 V for current. Power-cycle. All LEDs should be off and J2 should be closed.
4. **Check the fault references.** Measure U5 pin 6, U19 pin 6 and U19 pin 8 (the 0.24 V references): about 0.24 V each. Then, on the bottom, measure U22 pin 3 and U23 pin 3 (the 4.74 V references): about 4.74 V each.
5. **Set the thresholds.** Measure TP4, TP5 and TP6, and adjust RV1, RV2 and RV3 to the values from [issue 3](#issue-3-the-thresholds-have-to-be-set-on-the-car).
6. **Brake only.** Raise the front brake input above TP5. LED2 should light, with no trip. Lower it, then do the same for the rear (LED3).
7. **Current only.** Raise the current input above TP4. LED1 should light, with no trip.
8. **Both together, held.** LED4 should light within about 0.4 s, then LED5 should light and J2 should open.
9. **Remove both.** LED4 should go off. LED5 should stay on and J2 should stay open. Wait at least 15 s and check that it is still open.
10. **Power-cycle.** LED5 should go off and J2 should close.
11. **Sensor faults, one at a time.** Repeat each of these for front, rear and current, power-cycling after each trip:
    - **Unplug the sensor.** LED5 should light and J2 should open within about 0.2 s.
    - **Turn the stand-in up to 5 V.** Same result.
    - **Turn it to 4.5 V.** No trip. 4.5 V is the most a healthy sensor puts out.
12. **Timing (optional).** Put a scope across J2.
    - With both conditions applied, the trip should come 0.29–0.43 s after they start.
    - With a sensor fault, it should come 0.14–0.18 s after the fault starts.

### At technical inspection

T11.6.9 and IN4.1.3 describe the test: the team injects a signal on the current sensor input that represents 5 kW, while someone presses the brake pedal, and the BSPD must open the shutdown circuit. Follow [issue 7](#issue-7-the-inspection-test-must-be-set-up-before-power-up): connect the test source to J1 pin 3 before powering up.

Scrutineers may also unplug each sensor (T11.6.7). Each one should trip the BSPD.

---

## 7. Files in this folder and how to regenerate them

| Path | What it is |
|---|---|
| `BSPD KiCAD.kicad_sch` | The schematic (a single sheet). |
| `BSPD KiCAD.kicad_pcb` | The PCB layout. |
| `BSPD KiCAD.kicad_pro` | Project settings: design rules and net classes. |
| `BSPD KiCAD.step` | 3D model of the assembled board. Missing 15 part bodies; see issue 9. |
| `BSPD BOM.csv` | Full BOM, generated from the schematic. |
| `BSPD BOM_smd.csv` | The surface-mount parts only. |
| `fab/gerbers/` | Gerber and drill files for making the bare PCB. |
| `fab/BSPD_BOM.csv` | BOM for assembly. |
| `fab/BSPD_CPL.csv` | Pick-and-place (component positions) for assembly. |
| `analysis/helpers/bspd_core.inc` | The shared simulation model of the whole board. |
| `analysis/helpers/bspd_system.cir` | Simulation: trip timing, latch and power-cycle. |
| `analysis/helpers/bspd_fault_tests.cir` | Simulation: eleven sensor-fault scenarios. |

The `datasheets/` folder and the rest of `analysis/` exist on some team members' machines but aren't in git.

**After any change to the schematic or PCB, regenerate everything in `fab/`, both root BOMs and the STEP.** The commands below reproduce the committed files exactly, apart from timestamps. Run them from inside `BSPD_KR/`. On macOS, `kicad-cli` lives at `/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli`.

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

# 3D model
kicad-cli pcb export step --subst-models -f -o "BSPD KiCAD.step" "BSPD KiCAD.kicad_pcb"
```

`BSPD BOM_smd.csv` is the root BOM filtered down to the surface-mount parts: C15, C16, Q1, R31–R36, U4, U6, U9, U18, U20, U21, U22 and U23.

---

## 8. Glossary

| Term | Meaning |
|---|---|
| AIR | Accumulator Isolation Relay. One of the big relays that connect the high-voltage battery to the car. They are held closed by the shutdown circuit. |
| AMS | Accumulator Management System. Monitors the high-voltage battery. Also in the shutdown circuit. |
| BOM | Bill of Materials. The parts list. |
| BSPD | Brake System Plausibility Device. This board. |
| CPL | Component Placement List. Where each part goes on the board, for assembly machines. Also called pick-and-place. |
| Debounce | Ignoring a signal until it has stayed the same for long enough, so brief blips don't count. |
| DIP | Dual In-line Package. A through-hole chip with two rows of legs. U5 and U19 are DIP-14. |
| DRC | Design Rule Check. KiCad's check that the PCB layout follows the manufacturing rules. |
| ERC | Electrical Rule Check. KiCad's check that the schematic is wired sensibly. |
| ESF | Electrical System Form. The document the team submits describing the car's electrical systems and failure modes. |
| Footprint | The copper pads and outline for one part on the PCB. |
| Gerber | The standard file format PCB manufacturers use to make bare boards. |
| GLV | Grounded Low Voltage. The car's low-voltage system. |
| IMD | Insulation Monitoring Device. Detects leakage between the high-voltage system and the chassis. |
| LVMS | Low Voltage Master Switch. Turns the whole low-voltage system on and off. Power-cycling it resets the BSPD. |
| Net | One electrical connection in the design: every pin and track that is joined together. |
| Over-range / under-range | A sensor reading above the highest (or below the lowest) voltage a healthy sensor can produce. Used here to detect wiring faults. |
| POR | Power-On Reset. The circuit that puts the latch into a known state when power comes on. |
| SCS | System Critical Signal. A signal whose failure must put the car into a safe state (T11.9). |
| SDC | Shutdown Circuit. The safety loop that holds the AIRs closed. |
| SOIC | Small Outline IC. A small surface-mount chip package. U22 and U23 are SOIC-8. |
| TS | Tractive System. Everything on the high-voltage side. |
| TSAC | Tractive System Accumulator Container. The high-voltage battery box. |
| VCU | Vehicle Control Unit. The car's main controller (a Teensy). The BSPD does not depend on it. |
