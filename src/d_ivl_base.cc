/*$Id: d_logic.cc,v 1.2 2009-12-13 17:55:01 felix Exp $ -*- C++ -*-
 * vim:ts=8:sw=2:et:
 * Copyright (C) 2001 Albert Davis
 * Author: Albert Davis <aldavis@gnu.org>
 *         Felix Salfelder 2011
 *
 * This file is part of "Gnucap", the Gnu Circuit Analysis Package
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *------------------------------------------------------------------*/
#include "d_subckt.h"
#include "u_xprobe.h"
#include "d_ivl.h"
// #include "vvp/schedule.h"
//#include "d_ivl_ports.h"
#include "d_logic.h"
#include <dlfcn.h>
// #include "vvp/vvp_net.h"
#include "vvp/compile.h"
//
//
/// CALLBACK STUFF. not here? ///
// including headers, not #including vpi_priv.
/*
struct __vpiHandle {
        const struct __vpirt *vpi_type;
};

struct __vpiSignal {
      struct __vpiHandle base;
#ifdef CHECK_WITH_VALGRIND
      struct __vpiSignal *pool;
#endif
      union { // The scope or parent array that contains me.
	    vpiHandle parent;
	    struct __vpiScope* scope;
      } within;
      union { // The name of this reg/net, or the index for array words.
            const char*name;
            vpiHandle index;
      } id;
	// The indices that define the width and access offset. 
      int msb, lsb;
      unsigned signed_flag  : 1;
      unsigned is_netarray  : 1; // This is word of a net array
      vvp_net_t*node;
};
*/
#define DLLINK(a,b)  a = (typeof(a))dlsym(vvpso, b);  \
                        if((e=dlerror())) error(bDANGER, "so: %s\n", e); \
                           assert(a);


// move to ivl_ports!
static PLI_INT32 callback(t_cb_data*x){
  assert(x);
  DEV_LOGIC_DA* c= reinterpret_cast<DEV_LOGIC_DA*>(x->user_data);
  assert(c);
  const COMPONENT* o = prechecked_cast<const COMPONENT*>(c->owner());
  const COMMON_IVL* oc = prechecked_cast<const COMMON_IVL*>(o->common());
  assert(oc);
  const EVAL_IVL* sc = prechecked_cast<const EVAL_IVL*>(c->common());
  assert(sc);
  trace1("EVAL CALLBACK", hp(sc));

  __vpiSignal* signal = (__vpiSignal*) x->obj;
  assert(signal);
 // vvp_sub_pointer_t<vvp_net_t> ptr(signal->node,0);    

  struct t_vpi_value argval;  
  argval.format = vpiIntVal; 
  vpi_get_value(x->obj, &argval); // static member of vpi_priv.cc
  const vvp_vector4_t bit(1,(vvp_bit4_t)argval.value.integer);

  trace5("callback called", bit.value(0), sc->digital_time() ,
      x->value->value.integer, c->lvfromivl, CKT_BASE::_sim->_time0);
  assert(bit.value(0)==0 || bit.value(0)==1);

  if (c->lvfromivl == lvUNKNOWN){
    trace0("CALLBack init.");
    c->lvfromivl  = _LOGICVAL(3*bit.value(0)) ;
    assert(CKT_BASE::_sim->_time0 == 0 );
    return 0 ;
  }

  trace1("CALLB new_event",   sc->digital_time() );
  CKT_BASE::_sim->new_event( double( sc->digital_time()) );

  switch (bit.value(0)) {
    case 0: c->lvfromivl = lvFALLING; break;
    case 1: c->lvfromivl = lvRISING;  break;
    default: 
             error(bDANGER, "got bogus value %i from ivl\n", bit.value(0));
             break;
  }
  trace1("callback done ", c->lvfromivl);
  c->qe();
  return 0;
}
/*------------------------------------------------------------------*/
int DEV_IVL_BASE::_count = -1;
static EVAL_IVL Default_EVAL_IVL(CC_STATIC);
static COMMON_IVL Default_COMMON_IVL(CC_STATIC); // dummy COMMON
typeof(COMMON_IVL::_commons) COMMON_IVL::_commons;
int EVAL_IVL::_count = -1;
typeof(EVAL_IVL::_commons) EVAL_IVL::_commons;
/*--------------------------------------------------------------------------*/
/*------------------------------------------------------------------*/
int COMMON_IVL::_count = -1;

// enum smode_t   {moUNKNOWN=0, moANALOG=1, moDIGITAL, moMIXED};

DEV_IVL_BASE::DEV_IVL_BASE(): BASE_SUBCKT() , vvpso(0)
{
  debug_file.open( "/tmp/fooh" ); // make vvp happy.
  _n = _nodes;
  ++_count;
  attach_common(&Default_COMMON_IVL);
}
/*-------------------------------------------*/
DEV_IVL_BASE::DEV_IVL_BASE(const DEV_IVL_BASE& p):
  BASE_SUBCKT(p),vvpso(0){
  for (uint_t ii = 0;  ii < max_nodes();  ++ii) {
    _nodes[ii] = p._nodes[ii];
  }
  debug_file.open( "/tmp/fooh" );
  _n = _nodes;
  ++_count;
}

//bug?
DEV_IVL_BASE::~DEV_IVL_BASE(){
  --_count;
}

/*-------------------------------------------*/
void DEV_IVL_BASE::tr_accept(){
  static double lastaccept;
  const COMMON_IVL* c = prechecked_cast<const COMMON_IVL*>(common());
  assert(c);
  const EVAL_IVL* sc = prechecked_cast<const EVAL_IVL*>(subcommon());
  assert(sc);

  if ( lastaccept == _sim->_time0 && _sim->_time0!=0){
    return;
  }

  trace3("DEV_IVL_BASE::tr_accept", _sim->_time0,  lastaccept, hp(sc));
  lastaccept = _sim->_time0;

  // first queue events.
  // FIXME: just outports.
  subckt()->tr_accept();

  // then execute anything until now.
  trace2("DEV_LOGIC_VVP::tr_accept calling cont", _sim->_time0, hp(sc));
  sc->contsim_set(_sim->_time0);

  // accept again (the other nodes might have changed.)
  // FIXME: just inports
  subckt()->tr_accept();
  //uint_t incount=1;
  // node_t* n=&_n[2];
  
  // copy next event to master queue
  event_time_s* ctim = sc->schedule_list();
  double evt;
  if (ctim){
    evt = sc->event_(ctim);
    double eventtime = sc->event_absolute(ctim);
    trace5("DEV_LOGIC_VVP::tr_accept,  fetching event",_sim->_time0,
        ctim->delay, eventtime, sc->schedule_time(), hp(sc));
    trace_queue(ctim);

    assert(eventtime>_sim->_time0 - OPT::dtmin);

    assert(evt>=0);
    if(evt==0) { untested(); };
    _sim->new_event(eventtime);
  }

  // CHECK: implement tr_needs_eval?
  q_eval();
}

/*------------------------------------------------------------------*/
void DEV_IVL_BASE::tr_begin()
{
  const COMMON_IVL* c = prechecked_cast<const COMMON_IVL*>(common());
  assert(c);
  const EVAL_IVL* sc = prechecked_cast<const EVAL_IVL*>(subcommon());
  assert(sc);

  trace0("DEV_IVL_BASE::tr_begin " + short_label());

  // fixme. only once per common
  sc->tr_begin();
  sc->contsim("TRAN",0);

  // exchange initial conditions?
  // maybe not necessary (done during dc)

  subckt()->tr_begin();
  q_eval();
}
/*--------------------------------------------------------------------------*/
void DEV_IVL_BASE::precalc_first()
{
  trace0("DEV_IVL_BASE::precalc_first");
  COMPONENT::precalc_first();
  assert(common());

  if(subckt()){
    subckt()->precalc_first();
  }
}
/*--------------------------------------------------------------------------*/
void DEV_IVL_BASE::precalc_last()
{
  COMPONENT::precalc_last();
  if (subckt()) {subckt()->precalc_last();}
  assert(common()->model());
}
/*--------------------------------------------------------------------------*/
std::string DEV_IVL_BASE::port_name(uint_t i)const{
  const COMMON_IVL* c = prechecked_cast<const COMMON_IVL*>(common());
  assert(c);
  const MODEL_IVL_BASE* m = prechecked_cast<const MODEL_IVL_BASE*>(c->model());
  assert(m);
  return m->port_name(i);
}
/*--------------------------------------------------------------------------*/
void DEV_IVL_BASE::compile_design(COMPILE* compile){
  const COMMON_COMPONENT* c = prechecked_cast<const COMMON_COMPONENT*>(common());
  assert(c);
  const MODEL_IVL_BASE* m = dynamic_cast<const MODEL_IVL_BASE*>(c->model());
  trace0("COMMON_IVL::compile_design");

  m->compile_design(compile, short_label());
}
/*--------------------------------------------------------------------------*/
void DEV_IVL_BASE::expand()
{
  trace0("DEV_IVL_BASE::expand " + short_label());
  BASE_SUBCKT::expand();
  assert(_n);
  const COMMON_IVL* c = prechecked_cast<const COMMON_IVL*>(common());
  assert(c);
  const MODEL_IVL_BASE* m = prechecked_cast<const MODEL_IVL_BASE*>(c->model());
  assert(m);

  if (!subckt()) {
    new_subckt();
  }else{
  }
  
  if (_sim->is_first_expand()) {
    trace1 ("First expanding " +long_label(), net_nodes());
    precalc_first();
    precalc_last();
    uint_t n=2;

    COMPILE* comp = new COMPILE();
    assert(comp);
    trace0("compiling...");
    //comp->ca();
    // c->init();
    compile_design(comp);
    trace0("cleanup...");

    comp->cleanup();

    assert(comp->vpi_mode_flag() == VPI_MODE_NONE);
    trace1("vpimode...", comp->vpi_mode_flag() );
    comp->vpi_mode_flag(VPI_MODE_COMPILETF);

    vpiHandle item;

    trace0("looking up in vhbn");
    vpiHandle module = (comp->vpi_handle_by_name)(short_label().c_str(),NULL);
    assert(module);

    vpiHandle vvp_device = (comp->vpi_handle_by_name)(short_label().c_str(),module);
    assert(vvp_device);

    vvp_device=module;

    /// old code from d_vvp

    assert ((_n[0].n_()));
    assert ((_n[1].n_()));
    assert ((_n[2].n_()));
    assert ((_n[3].n_()));
    char src;
    EVAL_IVL* logic_common = 0;

    node_t lnodes[] = {_n[n], _n[0], _n[1], _n[1], _n[0]};
//  vpiParameter  holds fall, rise etc.
    string name;
    // not implemented
    // vpiHandle net_iterator = vpi_iterate(vpiPorts,vvp_device);

    CARD* logicdevice;
    node_t* x;
    trace1("DEV_IVL_BASE::expand "+ short_label() + " entering loop", net_nodes());

    // expand_nodes(); ?

    // vpiHandle net_iterator = vpi_iterate(vpiScope,vvp_device);
    //while ((item = vpi_scan(net_iterator))) 
    //
    while ( n < net_nodes() ) {
      item = comp->vpi_handle_by_name( port_name(n).c_str() , vvp_device );

      int type = vpi_get(vpiType,item);
      name = vpi_get_str(vpiName,item);
      COMPONENT* P;


      trace2("==> "+ short_label() + " item name: " + string(name), item->vpi_type->type_code, n );
      trace0("==>  looking for " + port_name(n));
      logic_common = (EVAL_IVL*) c->_eval_ivl;
      assert(logic_common);

      switch(type){
        case vpiNet: // <- ivl
          {
          trace0("  ==> Net: " + string(name) );
          src='V';
          x = new node_t();
          x->new_model_node("i_"+name, this);
          lnodes[0] = _n[n];
          lnodes[1] = _n[0];
          lnodes[2] = _n[1];
          lnodes[3] = _n[1];
          lnodes[4] = *x;
          logicdevice = device_dispatcher["port_from_ivl"];

          P = dynamic_cast<COMPONENT*>(logicdevice->clone());

          t_cb_data cbd = {
            cbValueChange, //reason
            callback, //cb_rtn
            item, //obj
            0, //time
            0, //value
            0, //index
            (char*)P //user_data
          };

          vpi_register_cb(&cbd);

          break;
          }
        case vpiReg: // -> ivl
          src='I';
          trace0("  ==> Reg: " + string(name) );
          x = new node_t();
          x->new_model_node("i_"+name, this);
          lnodes[0] = *x;
          lnodes[1] = _n[0];
          lnodes[2] = _n[1];
          lnodes[3] = _n[1];
          lnodes[4] = _n[n];
          logicdevice = device_dispatcher["port_to_ivl"];
          ((DEV_LOGIC_AD*) logicdevice)->H = item;
          P = dynamic_cast<COMPONENT*>(logicdevice->clone());
          break;

        default:
          // which other types would make sense?
          continue;
      }

      trace2("DEV_LOGIC_IVL::expand "+ name + " " + short_label(), n, (intptr_t)logic_common);
      assert(_n[n].n_());

      assert(P);
      subckt()->push_front(P);

      trace3("setting parameters", intptr_t(logic_common), logic_common->attach_count(), n );
      trace0("modelname: " + logic_common->modelname());

      COMMON_LOGIC* L = (COMMON_LOGIC*) logic_common;
      if (src=='I'){
        // to ivl
        P->set_parameters(name, this, L, 0, 0, NULL, 5 , lnodes);
      }else{
        // from ivl.
        P->set_parameters(name, this, L, 0, 0, NULL, 5 , lnodes);
      }

      assert(logic_common);

      trace1("loop end for "+name, n);
      n++;
    } 

    for (CARD_LIST::const_iterator i = subckt()->begin(); i != subckt()->end(); ++i) {
      const     COMMON_COMPONENT* com = ((const COMPONENT*)(*i))->common() ;
      _subcommon = com;
      trace2("have subdevice ", (*i)->long_label(), hp(com));
    }

    comp->vpi_mode_flag(VPI_MODE_NONE);


    init_vvp();
    trace2("DEV_LOGIC_IVL::expand called init_vvp", hp(vvpso), n);

    // copies stuff to dlmopened sim... (for now)
    assert(logic_common);
    assert(vvpso);
    logic_common->vvpinit(comp, vvpso);

  } // 1st expand

  std::string subckt_name(c->modelname()+"."+string(c->vvpfile));

  assert(!is_constant());
// is this an option?
// subckt()->set_slave();
  subckt()->expand();
}
/*---------------------*/
bool DEV_IVL_BASE::do_tr(){
  trace0("DEV_IVL_BASE::do_tr");
  //q_accept();
  return BASE_SUBCKT::do_tr();
}
/*--------------------------------------------------------------------------*/
double DEV_IVL_BASE::tr_probe_num(const std::string& x)const
{
  const EVAL_IVL* sc = prechecked_cast<const EVAL_IVL*>(subcommon());
  assert(sc);
  if (Umatch(x, "delay ")) {
    return ( sc->schedule_list() )?  sc->schedule_list()->delay : -1 ;
  }else if (Umatch(x, "st ")) {
    return ( sc->schedule_time() );
  }else{
    return BASE_SUBCKT::tr_probe_num(x);
  }
}
/*--------------------------------------------------------------------------*/
TIME_PAIR DEV_IVL_BASE::tr_review(){
  const COMMON_IVL* c = prechecked_cast<const COMMON_IVL*>(common());
  assert(c);
  const EVAL_IVL* sc = prechecked_cast<const EVAL_IVL*>(subcommon());
  assert(sc);
   _time_by =  BASE_SUBCKT::tr_review();

  event_time_s* ctim = sc->schedule_list();
  trace4("DEV_IVL_BASE::tr_review", ctim->delay, hp(sc), hp(ctim), _sim->_time0 );

  if (ctim){
    double delay = sc->event_absolute(ctim);
    trace1("DEV_LOGIC_VVP::tr_review", delay );
    _time_by.min_event(delay + _sim->_time0);
  }
  q_accept();
  return _time_by;
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
COMMON_IVL::COMMON_IVL(int c)
      :COMMON_COMPONENT(c), 
      vvpso(0),
      incount(0),
      vvpfile(""),
      module(""),
      _eval_ivl(0)
{
  trace1("COMMON_IVL::COMMON_IVL", c);
  ++_count;
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
COMMON_IVL::COMMON_IVL(const COMMON_IVL& p)
      :COMMON_COMPONENT(p),
      vvpso(p.vvpso), // correct?
      incount(p.incount),
      vvpfile(p.vvpfile),
      module(p.module),
      _eval_ivl(p._eval_ivl)
{
  trace0("COMMON_IVL::COMMON_IVL( COMMON_IVL )");
  ++_count;
}
/*--------------------------------------------------------------------------*/
void EVAL_IVL::contsim_set(double time) const
{
  trace1("EVAL contsim_set", time);
  SimTimeA  = time;
  SimDelayD = -1;
}
/*--------------------------------------------------------------------------*/
double EVAL_IVL::contsim(const char *analysis,double time) const
{
  trace1("contsim", time);
  SimTimeA  = time;
  SimDelayD = -1;

  assert(!strcmp("TRAN",analysis));

  SimState = SIM_CONT0;
  while (SimTimeDlast < time) {
    trace0("EVAL contsim");
    SimState = this->schedule_simulate_m(SimState);
    SimTimeDlast = SimTimeD;
    if (SIM_PREM <= SimState) break;
  }

  return SimDelayD;
} 
/*--------------------------------------------------------------------------*/
double EVAL_IVL::tr_begin()const
{
  trace0("startsim -> schedule_simulate_m(SIM_INIT)");
  SimDelayD  = -1;
  SimState = schedule_simulate_m(SIM_INIT);
  SimTimeDlast = SimTimeD;

  return SimDelayD;
} 
/*--------------------------------------------------------------------------*/
void EVAL_IVL::schedule_assign_plucked_vector(vpiHandle H,
    vvp_time64_t  dly, vvp_vector4_t val, int a, int b)const
{
  __vpiSignal* HS = (__vpiSignal*) H;
  vvp_sub_pointer_t<vvp_net_t> ptr(HS->node,0);
  assert(s_a_p_v);

  (*s_a_p_v)(ptr,dly,val,a,b);
}
/*--------------------------------------------------------------------------
sim_mode EVAL_IVL::schedule_simulate_init(sim_mode mode) const
{

}
--------------------------------------------------------------------------*/

void EVAL_IVL::do_some_precalc_last_stuff() const {
  // FIXME: move where it belongs...
  const EVAL_IVL* sim = this;
  assert(sim->schedule_time()==0);

  trace0("Execute end of compile callbacks"); // move away
  sim->vpiEndOfCompile();
  trace0("Done EOC");

  // Execute initialization events.
  sim->exec_init_list();
  trace0("Done init events...");

  // Execute start of simulation callbacks

  trace0("calling vpiStartOfSim");
  sim->vpiStartOfSim();
}
/*--------------------------------------------------------------------------*/
sim_mode EVAL_IVL::schedule_simulate_m(sim_mode mode) const
{
  const EVAL_IVL* sim = this;

  trace1("schedule_simulate_m", mode);
  trace_queue( sim->schedule_list());
  struct event_s      *cur  = 0;
  struct event_time_s *ctim = 0;
  double               d_dly;

  switch (mode) {
    case SIM_CONT0: if ((ctim = sim->schedule_list())) goto sim_cont0;
                      goto done;
    case SIM_CONT1: goto sim_cont1;
    default:
                    break;

  }

  do_some_precalc_last_stuff();

  // do i need signals here?
  // signals_capture();
  // trace1("signals_capture Done", schedule_runnable());

  trace1("EVAL schedule_list?", sim->schedule_runnable() );
  assert(sim->schedule_runnable());
  assert(! sim->schedule_stopped());
  if (sim->schedule_runnable())
    while (sim->schedule_list()) {
      trace0("EVAL schedule_list");

      if (sim->schedule_stopped()) {
        assert(false);
        sim->schedule_start();
        incomplete();
        // stop_handler(0);
        // You can finish from the debugger without a time change.
        if (!sim->schedule_runnable()) break;
        goto cycle_done;
      }

      /* ctim is the current time step. */
      ctim = sim->schedule_list();

      /* If the time is advancing, then first run the
         postponed sync events. Run them all. */
      if (ctim->delay > 0) {
        switch (mode) {
          case SIM_CONT0:
          case SIM_CONT1:
          case SIM_INIT:

            trace3("EVAL SIM_SOME", SimTimeD, SimTimeA, CKT_BASE::_sim->_time0);
            d_dly = sim->getdtime(ctim);
            if (d_dly > 0) {
              trace5("EVAL ", d_dly, CKT_BASE::_sim->_time0, ctim->delay, hp(this), hp(ctim));
              //doExtPwl(sched_list->nbassign,ctim);
              SimDelayD = d_dly; return SIM_CONT0; 
sim_cont0:
              double dly = sim->getdtime(ctim),
                     te  = SimTimeDlast + dly;
              if (te > SimTimeA) {
                SimDelayD = te - SimTimeA;
                return SIM_PREM; 
              }
              SimTimeD  = SimTimeDlast + dly;
            }
            break;
          default:
            fprintf(stderr,"deffault?\n");
        }

        trace2("EVAL simulate", sim->schedule_runnable(), ctim->delay );
        if (!sim->schedule_runnable()) break;
        sim->schedule_time(sim->schedule_time() + ctim->delay);
        ctim->delay = 0;

        sim->vpiNextSimTime(); // execute queued callbacks 
      } else 
        trace0("EVAL delay <= 0" );


      /* If there are no more active events, advance the event
         queues. If there are not events at all, then release
         the event_time object. */
      if (ctim->active == 0) {
        ctim->active = ctim->nbassign;
        ctim->nbassign = 0;

        if (ctim->active == 0) {
          ctim->active = ctim->rwsync;
          ctim->rwsync = 0;

          /* If out of rw events, then run the rosync
             events and delete this time step. This also
             deletes threads as needed. */
          if (ctim->active == 0) {
            trace0("EVAL rosync");
            sim->run_rosync( ctim );
            trace0("EVAL pop  schedule_list");
            sim->schedule_list( ctim->next );
            switch (mode) {
              case SIM_CONT0:
              case SIM_CONT1:
              case SIM_INIT: 

                d_dly = sim->getdtime(ctim);
                if (d_dly > 0) {
                  trace0("noextPWL again");
                  // doExtPwl(sched_list->nbassign,ctim);
                  SimDelayD = d_dly;
                  delete ctim;
                  return SIM_CONT1;
sim_cont1:
                  // SimTimeD += ???;
                  goto cycle_done;
                }
              default:
                fprintf(stderr,"default 2\n");
            }
            delete ctim;
            goto cycle_done;
          }
        }
      }

      /* Pull the first item off the list. If this is the last
         cell in the list, then clear the list. Execute that
         event type, and delete it. */
      cur = ctim->active->next;
      if (cur->next == cur) {
        ctim->active = 0;
      } else {
        ctim->active->next = cur->next;
      }
      assert(cur);

      trace0("EVAL run_run");
      cur->run_run();

      delete (cur);

cycle_done:;
    }

  if (SIM_ALL == mode) {


    // signals_revert();

    // Execute post-simulation callbacks
    sim->vpiPostsim();
  }

done:
  return SIM_DONE;
}
/*--------------------------------------------------------------------------*/
EVAL_IVL::EVAL_IVL(int c)
      :COMMON_COMPONENT(c), 
      vvpso(0),
      incount(0),
      g_s_t(0),
      s_s_t(0),
      g_s_l(0),
      s_s_l(0),
      s_a_p_v(0),
      s_st(0),
      n_s(0),
      r_r(0),
      s_e(0)
{
  trace1("EVAL_IVL::EVAL_IVL", c);
  ++_count;
}
/*--------------------------------------------------------------------------*/
EVAL_IVL::EVAL_IVL(const EVAL_IVL& p)
      :COMMON_COMPONENT(p),
      vvpso(p.vvpso), // correct?
      incount(p.incount),
      g_s_t(p.g_s_t),
      s_s_t(p.s_s_t),
      g_s_l(p.g_s_l),
      s_s_l(p.s_s_l),
      s_a_p_v(p.s_a_p_v),
      s_st(p.s_st),
      n_s(p.n_s),
      r_r(p.r_r),
      s_e(p.s_e)
{
  trace2("EVAL_IVL::EVAL_IVL(p) "+ modelname(), hp(this),  p.attach_count() );
  ++_count;
}
/*--------------------------------------------------------------------------*/
EVAL_IVL::~EVAL_IVL()	{
  --_count;

  // ?
//  detach_common(&_eval_ivl);

}
/*--------------------------------------------------------------------------*/

// theres a bug somewhere triggered if the 1st attached common turns out equal.
// (is this possible?)
COMMON_COMPONENT* EVAL_IVL::deflate()
{
  trace1("EVAL_IVL::deflate", _commons.size());
  // return this;
  for( list<const COMMON_COMPONENT*>::iterator i = _commons.begin();
      i != _commons.end(); ++i ){

    assert(*i);
    if (*this == **i){
      trace1("EVAL_IVL::deflate hit", (*i)->modelname());
      return const_cast<COMMON_COMPONENT*>( *i );
    }
    trace1("EVAL_IVL::deflate miss", (*i)->modelname());
  }
  trace1("EVAL_IVL::deflate pushing back ", hp(this));
  _commons.push_back(this);
  return this;
}
/*--------------------------------------------------------------------------*/
void EVAL_IVL::precalc_first(const CARD_LIST* par_scope)
{
  trace0("EVAL_IVL::precalc_first ");
  assert(par_scope);

  COMMON_COMPONENT::precalc_first(par_scope);
}
/*--------------------------------------------------------------------------*/
void EVAL_IVL::expand(const COMPONENT* dev )
{
  COMMON_COMPONENT::expand(dev);
  attach_model(dev);

  //fetch simulator from device (good idea?)
  //needed to tell commons apart.
  const DEV_IVL_BASE* c = dynamic_cast<const DEV_IVL_BASE*>(dev);
//  assert(c->vvpso());
//  vvpso = c->vvpso();



  const MODEL_LOGIC* m = dynamic_cast<const MODEL_LOGIC*>(model());
  if (!m) {
    throw Exception_Model_Type_Mismatch(dev->long_label(), modelname(), name());
  }

}
/*--------------------------------------------------------------------------*/
void EVAL_IVL::precalc_last(const CARD_LIST* par_scope)
{
  COMMON_COMPONENT::precalc_last(par_scope);
}
/*--------------------------------------------------------------------------*/
bool EVAL_IVL::operator==(const COMMON_COMPONENT& x )const{
  trace1("EVAL_IVL::operator== ", x.is_trivial());
  trace1("EVAL_IVL::operator==" + x.modelname(), hp(this));
  const EVAL_IVL* p = dynamic_cast<const EVAL_IVL*>(&x);
  if (!p){
    trace0("EVAL_IVL::operator== ???");
    return false;
  }

  // includes "same model".
  bool cr = COMMON_COMPONENT::operator==(x);

  bool ret = (vvpso == p->vvpso) && cr;
  if (ret){
    trace2("these are equal ", hp(this), hp(&x) );
  }


}
/*--------------------------------------------------------------------------*/
int EVAL_IVL::vvpinit(COMPILE* compile, void* _vvpso) {
  trace2("COMMON_IVL::vvpinit from compile", hp(_vvpso), _vvpso);
  SimTimeD = 0;
  SimTimeA = 0;
  SimTimeDlast = 0;
  SimDelayD = -1;
  if(vvpso){
    trace0("already done??");
    return -1;
  }
  vvpso=_vvpso;
  char* e;
  assert(vvpso);


#if 1
  // const char* e;
  vhbn  = (typeof(vhbn))dlsym(vvpso,"vpi_handle_by_name");
  if((e=dlerror())) error(bDANGER, "so: %s\n", e);
  assert(vhbn);

  DLLINK(r_r,"run_rosync");
  // DLLINK(s_e,"schedule_enlist");
  DLLINK(g_s_t,"get_schedule_time");
  DLLINK(s_s_t,"set_schedule_time");
  DLLINK(g_s_l,"get_schedule_list");
  DLLINK(s_s_l,"set_schedule_list");
  DLLINK(_vpi_mode,"vpi_mode_flag");

  s_a_p_v = (typeof(s_a_p_v))dlsym(vvpso,"schedule_assign_plucked_vector");
  if((e=dlerror())) error(bDANGER, "so: %s\n", e);
  assert(s_a_p_v);
  //contsim  = (typeof(contsim))dlsym(vvpso,"contsim");
  //ssert(contsim);
  //
  void (*s_s_i_l)(event_s*);

  DLLINK(s_s_i_l,"set_schedule_init_list");
  DLLINK(e_c,"vpiEndOfCompile"); 
  DLLINK(v_p,"vpiPostsim");
  DLLINK(v_s,"vpiStartOfSim");
  DLLINK(s_r,"schedule_runnable");
  DLLINK(e_i,"exec_init_list"); 
  DLLINK(v_t,"vpip_set_time_precision"); 
  DLLINK(s_x,"schedule_stopped");

  event_time_s* my_sched_list = compile->schedule_list();

  this->schedule_list(my_sched_list);

  //memcpy( sched_list, compile->schedule_list(), sizeof(event_time_s));

  trace0("EVAL copying simdata");
  trace_queue(my_sched_list);
  (*s_s_i_l)(compile->schedule_init_list());
  compile->dump_stuff();
  this->schedule_time(0);
  

  // fetching time prec. probably not necessary (we have dtmin)
  int (*gtp)();
  gtp = (int(*)())  dlsym(vvpso,"vpip_get_time_precision");
  if((e=dlerror())) error(bDANGER, "so: %s\n", e);
  assert(gtp);

   //vvp_time64_t (*s_s)();
  _time_prec = (*gtp)();

  if(!_time_prec){
    error(bDANGER,"no time precision?\n");
    this->vpip_set_time_precision(10);
    _time_prec = (*gtp)();
  }
  trace1("vvp init", _time_prec);

  assert( _time_prec );


  //  so_main =  (typeof(so_main))dlsym(vvpso,"so_main");
  //   if(!so_main){
  //     error(bDANGER, "so: %s\n", dlerror());
  //   }
  //activate = (typeof(activate))SetActive;
  //assert(activate);
#else
#endif

  return 0;
  //return( (*so_main)(args) );
}
/*--------------------------------------------------------------------------*/
void COMMON_IVL::precalc_first(const CARD_LIST* par_scope)
{
  trace0("COMMON_IVL::precalc_first " + (string) module + " " + (string) vvpfile );
  assert(par_scope);

  COMMON_COMPONENT::precalc_first(par_scope);
  vvpfile.e_val("UNSET", par_scope);
  module.e_val("UNSET", par_scope);

  //something hosed here.
}
/*--------------------------------------------------------------------------*/
void COMMON_IVL::expand(const COMPONENT* dev ){
  trace1("COMMON_IVL::expand" + dev->long_label(), (intptr_t) dev % PRIME);

  COMMON_COMPONENT::expand(dev);
  attach_model(dev);

  const MODEL_IVL_BASE* m = dynamic_cast<const MODEL_IVL_BASE*>(model());
  if (!m) {
    throw Exception_Model_Type_Mismatch(dev->long_label(), modelname(), name());
  }

  COMMON_LOGIC* eval = (COMMON_LOGIC*) new EVAL_IVL();
  trace1("COMMON_IVL::expand model "+ modelname(), hp(eval));
  eval->set_modelname(modelname());

  //eval->attach(model());
  //
  eval->attach(
      (MODEL_CARD*) (
        ((const MODEL_IVL_BASE*)model())->logic_model()
        )
      );
  attach_common(eval, &_eval_ivl);
}
/*--------------------------------------------------------------------------*/
void COMMON_IVL::precalc_last(const CARD_LIST* par_scope)
{
  COMMON_COMPONENT::precalc_last(par_scope);
  vvpfile.e_val("UNSET" , par_scope);
  module.e_val("UNSET" , par_scope);
}
/*--------------------------------------------------------------------------*/
int COMMON_IVL::compile_design(COMPILE*c, COMPONENT* p)const{
  const MODEL_IVL_BASE* m = dynamic_cast<const MODEL_IVL_BASE*>(model());
  trace0("COMMON_IVL::compile_design");

  //c->init();
  // c->ca();
  return m->compile_design(c, p->short_label());
}
/*--------------------------------------------------------------------------*/
void DEV_IVL_BASE::init_vvp() // const CARD_LIST* par_scope)
{
  trace1("DEV_IVL_BASE::init_vvp", hp(vvpso));
// FIXME: move to EVAL_IVL?


#define USE_DLM 1
 
  if(!vvpso){

    /// dlopen has been redeclared. md.h
#ifdef USE_DLM
    vvpso = dlmopen(LM_ID_NEWLM,"libvvp.so",RTLD_LAZY); 
    // vvpso = dlmopen(LM_ID_BASE,"libvvp.so",RTLD_LAZY); 
#else
    vvpso = dlopen("libvvp.so",RTLD_LAZY|RTLD_LOCAL); //|RTLD_GLOBAL);
#endif
    if(vvpso == NULL) throw Exception("cannot open libvvp: %s: ", dlerror());
    trace0("=========== LOADED libvvp.so ==========");
    dlerror();

    incomplete();
    // ret = ports_common->vvpinit();
    //(string(vvpfile)+".vvp").c_str());   
    if(dlerror()) throw Exception("cannot init vvp %s", dlerror());


  } else {
    trace0("COMMON_IVL::precalc_last already done vvpso");
  }
  trace0("COMMON_IVL::precalc_last done");
}
/*--------------------------------------------------------------------------*/
COMMON_IVL::~COMMON_IVL()	{
  --_count;

  trace1("COMMON_IVL::~ detaching ", hp(_eval_ivl));
  detach_common(&_eval_ivl);

  for( vector<COMMON_COMPONENT*>::iterator i = _subcommons.begin();
      i!=_subcommons.end(); ++i){
    trace1("COMMON_IVL::~ deleting ", hp((*i)));
    delete (&(*i));
  }
}
/*--------------------------------------------------------------------------*/
std::string COMMON_IVL::param_name(int i) const{
  trace1("COMMON_IVL::param_name ", i);
  switch (COMMON_COMPONENT::param_count() - 1 - i) {
    case 0: return "file";
    case 1: return "module";
    default: return COMMON_COMPONENT::param_name(i);
  }
}
/*--------------------------------------------------------------------------*/
bool COMMON_IVL::param_is_printable(int i) const{
  return (COMMON_COMPONENT::param_count() - 1 - i)  < param_count();
}
/*--------------------------------------------------------------------------*/
std::string COMMON_IVL::param_name(int i, int j)const
{
  if (j == 0) {
    return param_name(i);
  }else if (i >= COMMON_COMPONENT::param_count()) {
    return "";
  }else{
    return COMMON_COMPONENT::param_name(i, j);
  }
}
/*--------------------------------------------------------------------------*/
std::string COMMON_IVL::param_value(int i)const
{
  switch (COMMON_COMPONENT::param_count() - 1 - i) {
    case 0: return vvpfile.string();
    case 1: return module.string();
    default: return COMMON_COMPONENT::param_value(i);
  }
}
/*--------------------------------------------------------------------------*/
void COMMON_IVL::set_param_by_index(int i, std::string& value, int offset)
{
  switch (COMMON_COMPONENT::param_count() - 1 - i) {
    case 0: vvpfile = value; 
            break;
    case 1: module = value;
            break;
    default: COMMON_COMPONENT::set_param_by_index(i, value, offset); break;
  }
}
/*--------------------------------------------------------------------------*/
COMMON_COMPONENT* COMMON_IVL::clone()const
{
  return new COMMON_IVL(*this);
}
/*--------------------------------------------------------------------------*/
bool COMMON_IVL::operator==(const COMMON_COMPONENT& x )const{
  trace2("COMMON_IVL::operator==", hp(this), hp(&x) );
  bool cr = COMMON_COMPONENT::operator==(x);
  trace1("COMMON_IVL::operator==", cr );

  const COMMON_IVL* p = dynamic_cast<const COMMON_IVL*>(&x);
  if (!p){
    return false;
  }

  trace0("COMMON_IVL::operator== debug " + string(vvpfile) + " == " + string(p->vvpfile) );
  bool ret = (vvpfile==p->vvpfile)
    && (module==p->module); // bad idea...?

  return ret && cr;
}
/*--------------------------------------------------------------------------*/
COMMON_COMPONENT* COMMON_IVL::deflate()
{
  trace0("COMMON_IVL::deflate");
  // return this;
  for( list<const COMMON_COMPONENT*>::iterator i = _commons.begin();
      i != _commons.end(); ++i ){
    assert(*i);
    if (*this == **i){
      trace0("COMMON_IVL::deflate hit");
      return const_cast<COMMON_COMPONENT*>( *i );
    }
    trace0("COMMON_IVL::deflate miss");
  }
  _commons.push_back(this);
  return this;
}
/*--------------------------------------------------------------------------*/
std::string MODEL_IVL_BASE::port_name(uint_t i)const{
  stringstream a;
  a << "port" << i;
  return a.str();
}
/*--------------------------------------------------------------------------*/
void MODEL_IVL_BASE::precalc_first(){
  MODEL_LOGIC::precalc_first();
  if(!_logic_model)
    _logic_model = MODEL_LOGIC::clone();
}
/*--------------------------------------------------------------------------*/
void MODEL_IVL_BASE::precalc_last(){
  MODEL_LOGIC::precalc_last();
}
/*--------------------------------------------------------------------------*/
MODEL_IVL_BASE::MODEL_IVL_BASE(const BASE_SUBCKT* p)
  :MODEL_LOGIC((const COMPONENT*) p),
  _logic_model(0) {
    trace0("MODEL_IVL_BASE::MODEL_IVL_BASE");
  }
/*--------------------------------------------------------------------------*/
MODEL_IVL_BASE::MODEL_IVL_BASE(const MODEL_IVL_BASE& p)
  :MODEL_LOGIC(p){
    file=p.file;
    output=p.output;
    input=p.input;
    _logic_model=p._logic_model;
    trace0("MODEL_IVL_BASE::MODEL_IVL_BASE");
  }
/*--------------------------------------------------------------------------*/
int MODEL_IVL_BASE::_count = -1;
/*--------------------------------------------------------------------------*/

void yyerror(const char*msg){
        error(bDANGER, "%s\n",msg);
}

ofstream debug_file;

