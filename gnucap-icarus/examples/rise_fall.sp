
load ../gnucap-icarus.so
//* /home/jp/arails/arails0809/demo/g65/simulation/testLib/ringosc/20090526122839/hspice/netlist.sp

spice
*.option 
vgnd 0 0 dc=0

.subckt inv__d (gnd vdd out in)
iin  in  gnd extpwl
+external="../icarus/verilog/vvp/libvvp.so:-M. -mbindsigs -v kreuz.vvp:%m.in"
* in ist der Name von dem Signal im verilog modul
vout out gnd extpwl
+external="../icarus/verilog/vvp/libvvp.so:-M. -mbindsigs -v kreuz.vvp:%m.out"
.ends

.include ./mos.gc

.verilog

module inv__a (gnd vdd out in);
pmos4  #(.w(1u),.l(240n),.m(1),.ad(2.05e-13),.as(2.05e-13),.pd(1.41e-06),.ps(1.41e-06)) Mmp1(out,in,vdd,vdd);
nmos4 #(.w(750n),.l(240n),.m(1),.ad(2.05e-13),.as(2.05e-13),.pd(1.41e-06),.ps(1.41e-06)) Mmn1(out,in,gnd,gnd);
endmodule


inv__d x5 (0,ndd,nout1,nin);
inv__a x6 (0,ndd,nout2,nin);

vsource #(.dc(3)) Vv1 (.p(ndd), .n(0))

spice
vpulse nin 0 pulse(iv=3,pv=0,delay=5n,rise=5n, width=1n, fall=5n)


.print tran v(nin) v(nout*) hidden(0) 
.transientt 15n 100p
*TRACE alltime
.end
