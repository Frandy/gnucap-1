* AND check.

.parameter vdd=3
.model my_logic logic vmax=vdd delay=.07n

.subckt my_logicand1 Y gnd ndd ndd2  A
R1 Y A 5
.ends


U1 nout 0 ndd ndd nin my_logic and
R1 nout 0 100k

vdd ndd 0 vdd
*.model cmos LOGIC vmax=vdd vmin=0 fall=1e-10 rise=1e-10 delay=5e-11
*comment

V1 nin 0 pulse  iv=0 pv=vdd rise=50p fall=50p period=200p width=50p delay=40p

.print tran v(nin) v(nout) logic(nin) v(ndd) 
.transient 0.0 .3n 5p trace=a
.end