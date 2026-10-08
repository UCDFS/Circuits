* BSPD_KR full-chain behavioural model (schematic v06: latch + sensor-fault trip)
* Sensor nodes cur, bf, br and supply node glv are driven by the including file.
.param VTL=2.5
.param VTH_I=2.66 VTH_BF=2.8 VTH_BR=2.8
* 7805 behavioural + 10u output cap
B7805 v5raw 0 V = (V(glv) > 7.5) ? 5 : (V(glv) > 0 ? V(glv)*5/7.5 : 0)
R7805 v5raw vcc 1
C7805 vcc 0 10u
Rload vcc 0 1k
* input conditioning: 100k pull-down at J1, 1k series
R1 cur 0 100k
R4 cur nA 1k
R2 bf 0 100k
R5 bf nB 1k
R3 br 0 100k
R6 br nC 1k
* under-range references: 5 V x 5.1k/105.1k = 0.243 V (R7/R8, R9/R10, R11/R12)
R7 vcc vrefI 100k
R8 vrefI 0 5.1k
R9 vcc vrefF 100k
R10 vrefF 0 5.1k
R11 vcc vrefR 100k
R12 vrefR 0 5.1k
* over-range references: 5 V x 18k/19k = 4.74 V (R33/R34 current, R35/R36 brakes)
R33 vcc vovrI 1k
R34 vovrI 0 18k
R35 vcc vovrB 1k
R36 vovrB 0 18k
* threshold pots (wiper as fraction of vcc)
BRV1 rv1 0 V = V(vcc)*{VTH_I/5}
BRV2 rv2 0 V = V(vcc)*{VTH_BF/5}
BRV3 rv3 0 V = V(vcc)*{VTH_BR/5}
* LM339/LM393 open-collector: switch to GND closes when (in-) > (in+)
.model CMP SW(RON=50 ROFF=1G VT=0 VH=2m)
* "too high" comparators (U5A, U19A, U19C) on the per-sensor wires
S5A net2 0 nA rv1 CMP
R13 vcc net2 10k
S19A net1 0 nB rv2 CMP
R14 vcc net1 10k
S19C net22 0 nC rv3 CMP
R15 vcc net22 10k
* sensor-fault comparators, all wired-OR onto FAULT_N (R31 pull-up)
S5B fault 0 vrefI nA CMP
S19B fault 0 vrefF nB CMP
S19D fault 0 vrefR nC CMP
S22A fault 0 nA vovrI CMP
S23A fault 0 nB vovrB CMP
S23B fault 0 nC vovrB CMP
R31 vcc fault 10k
* debounce R32/C15, U22B compares FAULT_F with REF_1V0 and pulls TRIP_REQ_N low
R32 fault ff 100k
C15 ff 0 1u
* U18 AND, U6 NOR
B18 and_o 0 V = ((V(net1) > {VTL}) && (V(net22) > {VTL})) ? V(vcc) : 0
R18 and_o and_y 100
C18 and_y 0 100p
B6 nor_o 0 V = ((V(net2) < {VTL}) && (V(and_y) < {VTL})) ? V(vcc) : 0
R6o nor_o nor_y 100
C6 nor_y 0 100p
* 500 ms timer: R20 39k, C10 10u, ref 3.0 V, U5C -> TRIP_REQ_N (net7)
R20 nor_y t500 39k
C10 t500 0 10u
R22 vcc vref3 10k
R23 vref3 0 15k
S5C net7 0 t500 vref3 CMP
S22B net7 0 vref1 ff CMP
R26 vcc net7 10k
* U9 inverter -> BSPD_TRIP
B9 inv_o 0 V = (V(net7) < {VTL}) ? V(vcc) : 0
R9o inv_o trip 100
C9 trip 0 100p
* SR latch U21 NOR(Q,TRIP)=QN ; U20 NOR(QN,POR)=Q
B21 qn_o 0 V = ((V(q) < {VTL}) && (V(trip) < {VTL})) ? V(vcc) : 0
R21o qn_o qn 100
C21 qn 0 100p
B20 q_o 0 V = ((V(qn) < {VTL}) && (V(por) < {VTL})) ? V(vcc) : 0
R20o q_o q 100
C20 q 0 100p
* POR: C13 10u vcc->por, R30 100k, D10 clamp
C13 vcc por 10u
R30 por 0 100k
D10 0 por DCLAMP
.model DCLAMP D(IS=1e-12)
* U5D: in-=q, in+=1.02 V (REF_1V0) -> Q1 gate (R27 pull-up)
R24 vcc vref1 39k
R25 vref1 0 10k
S5D gate 0 q vref1 CMP
R27 vcc gate 10k
* Q1 2N7002 + relay coil 12 V/400 ohm + D9 flyback
M1 coil gate 0 0 MN2N7002
.model MN2N7002 NMOS(LEVEL=1 VTO=1.5 KP=0.3 LAMBDA=0.01)
L1 glv coilL 20m
Rcoil coilL coilx 400
Vsense coilx coil 0
D9 coil glv DFLY
.model DFLY D(IS=1e-9)
* SDC contact closed (1) when coil current > 20 mA
Bsdc sdc 0 V = (i(Vsense) > 20m) ? 1 : 0
