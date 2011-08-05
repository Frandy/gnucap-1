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
#include "d_logic.h"
#include <dlfcn.h>
#include "e_ivl_compile.h"
//
#ifndef NDEBUG
//#define USE_CALLBACK
#endif


#ifdef USE_CALLBACK
// this is an old hack.
static PLI_INT32 callback(t_cb_data*x){
  assert(x);
  DEV_LOGIC_DA* port= reinterpret_cast<DEV_LOGIC_DA*>(x->user_data);
  assert(port);
  const DEV_IVL_BASE* o = prechecked_cast<const DEV_IVL_BASE*>(port->owner());

  const EVAL_IVL* sc = prechecked_cast<const EVAL_IVL*>(port->common());
  assert(sc);
  trace2("EVAL CALLBACK", hp(sc),  CKT_BASE::_sim->_time0 );
  // assert(false);
  assert( x->value);

  assert(sc==o->subcommon());

  __vpiSignal* signal = (__vpiSignal*) x->obj;
  assert(signal);
 // vvp_sub_pointer_t<vvp_net_t> ptr(signal->node,0);    

  struct t_vpi_value argval;  
  argval.format = vpiIntVal; 
  vpi_get_value(x->obj, &argval); // static member of vpi_priv.cc
  const vvp_vector4_t bit(1,(vvp_bit4_t)argval.value.integer);

  trace2("callback called",  sc->digital_time(), CKT_BASE::_sim->_time0);
  trace4("callback called", bit.value(0), sc->digital_time(),
      port->lvfromivl,
      CKT_BASE::_sim->_time0);
  trace1("callback called",  sc->time_precision());

  assert(bit.value(0) == 0 || bit.value(0) == 1);

  if (port->lvfromivl == lvUNKNOWN){
    trace0("CALLBack init."); // ??
    port->lvfromivl  = _LOGICVAL(3*bit.value(0)) ;
    assert(CKT_BASE::_sim->_time0 == 0 );
    return 0;
  }

  trace1("CALLB new_event",   sc->digital_time() );
  port->edge(bit.value(0)); // name?
  return 0;
}
#endif

COMPILE_WRAP* EVAL_IVL::_comp;
int EVAL_IVL::_time_prec;
double EVAL_IVL::timescale;
/*------------------------------------------------------------------*/
inline vvp_time64_t EVAL_IVL::discrete_floor(double abs_t) const
{
  assert(is_number(  pow(10.0, _time_prec ))) ;
  return static_cast<vvp_time64_t>( abs_t / pow(10.0, _time_prec ));
}
/*------------------------------------------------------------------*/
int DEV_IVL_BASE::_count = -1;
int COMMON_IVL::_count = -1;
int EVAL_IVL::_count = -1;
int MODEL_IVL_BASE::_count = 0;
static EVAL_IVL Default_EVAL_IVL(CC_STATIC);
static COMMON_IVL Default_COMMON_IVL(CC_STATIC); // dummy COMMON
typeof(COMMON_IVL::_commons) COMMON_IVL::_commons;
typeof(EVAL_IVL::_commons) EVAL_IVL::_commons;
/*--------------------------------------------------------------------------*/
/*------------------------------------------------------------------*/

// enum smode_t   {moUNKNOWN=0, moANALOG=1, moDIGITAL, moMIXED};

DEV_IVL_BASE::DEV_IVL_BASE(): BASE_SUBCKT() ,
  _subcommon(0),
  _comp(0)
{
//   debug_file.open( "/tmp/fooh" ); // make vvp happy.
  _n = _nodes;
  ++_count;
}
/*-------------------------------------------*/
DEV_IVL_BASE::DEV_IVL_BASE(const DEV_IVL_BASE& p):
  BASE_SUBCKT(p),
  _subcommon(p._subcommon),
  _comp(p._comp){
  for (uint_t ii = 0;  ii < max_nodes();  ++ii) {
    _nodes[ii] = p._nodes[ii];
  }
  debug_file.open( "/tmp/fooh" );
  _n = _nodes;
  ++_count;
}

//bug?
DEV_IVL_BASE::~DEV_IVL_BASE(){
  trace1("DEV_IVL_BASE::~DEV_IVL_BASE", hp(this));
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

  sc->catch_up(_sim->_time0);

  // first queue events.
  // FIXME: just outports.
  subckt()->tr_accept();

  // then execute anything until now.
  trace2("DEV_LOGIC_VVP::tr_accept calling cont", _sim->_time0, hp(sc));
  // sc->contsim_set(_sim->_time0);

  // lookahead is the minimum number of ticks the fastest signal needs to
  // travel through ivl ( good idea? )
  vvp_time64_t lookahead = 10;

  sc->contsim( lookahead );

  // accept again (the other nodes might have changed.)
  // FIXME: just inports
  subckt()->tr_accept();
  
  // copy next event to master queue
  event_time_s* ctim = schedule_list();
  double evt;
  if (ctim){
    evt = sc->event_(ctim);
    double eventtime = sc->event_absolute(ctim);
    trace5("DEV_LOGIC_VVP::tr_accept, EVAL_IVL  fetching event",_sim->_time0,
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

  static int done;
  if(!done) {
  done=1;

  trace0("DEV_IVL_BASE::tr_begin " + short_label());

  // fixme. only once per common
  sc->tr_begin();
  sc->contsim("TRAN",0);

  // exchange initial conditions?
  // maybe not necessary (done during dc)
  }
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

  trace0("DEV_IVL_BASE::precalc_last");


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
void DEV_IVL_BASE::compile_design(COMPILE_WRAP* compile, COMPONENT** da){
  const COMMON_IVL* c = prechecked_cast<const COMMON_IVL*>(common());
  const MODEL_IVL_BASE* m = prechecked_cast<const MODEL_IVL_BASE*>(c->model());
  trace0("COMMON_IVL::compile_design");

  m->compile_design(compile, short_label(), da, common());
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
//  const EVAL_IVL* sc = prechecked_cast<const EVAL_IVL*>(subcommon());
//  assert(sc);

  if (!subckt()) {
    new_subckt();
  }else{
  }

  

  if (_sim->is_first_expand()) {
    trace1 ("First expanding " +long_label(), net_nodes());
  //  FIXME EVAL_IVL has to do the compiler...
    _comp = EVAL_IVL::get_comp();

    precalc_first();

    CARD* logicdevice;
    logicdevice = device_dispatcher["port_from_ivl"];
    COMPONENT* daports[m->da_nodes()];
    for ( unsigned i=0 ; i < m->da_nodes(); i++ ) {
      daports[i] = dynamic_cast<COMPONENT*>(logicdevice->clone());
    }
    logicdevice = device_dispatcher["port_to_ivl"];
    unsigned ad_nodes = net_nodes()-m->da_nodes();
    trace1("expand", ad_nodes);
    COMPONENT* adports[ad_nodes];
    for ( unsigned i=0 ; i < ad_nodes; i++ ) {
      adports[i] = dynamic_cast<COMPONENT*>(logicdevice->clone());
    }
    precalc_last();
    uint_t n=2;

    assert(_comp);


        // stupid:
        // expand needs ports, 
        // ports need compile
        // compile needs timescale
        // timescale needs expand,
        //
        // --> integer delay=realdelay/$timescale doesnt make sense.

    trace0("compiling...");
    compile_design(_comp, daports);
    trace0("compile done...");

    assert(_comp->vpi_mode_flag() == VPI_MODE_NONE);
    _comp->vpi_mode_flag(VPI_MODE_COMPILETF);
    vpiHandle module = (_comp->vpi_handle_by_name)(short_label().c_str(),NULL);
    assert(module);

    assert ((_n[0].n_())); // gnd
    assert ((_n[1].n_())); // vdd
    assert ((_n[2].n_())); // wouldnt make sense to have no i/o pin
    // FIXME: theres no warning, if a port has been forgotten :( (why?)
    char src;

    node_t lnodes[] = {_n[n], _n[0], _n[1], _n[1], _n[0]};
    string name;

    node_t* x;
    unsigned daportno = 0;
    unsigned adportno = 0;

    while ( n < net_nodes() ) {
      vpiHandle item = _comp->vpi_handle_by_name( port_name(n).c_str(), module );

      int type = vpi_get(vpiType,item);
      name = vpi_get_str(vpiName,item);
      COMPONENT* P;

      switch(type) {
        case vpiReg: // <- ivl
          trace0("DEV_IVL  ==> Net: " + string(name) + " placing DEV_LOGIC_DA");
          src='V';
          x = new node_t();
          x->new_model_node("i_"+name, this);
          lnodes[0] = _n[n];
          lnodes[1] = _n[0];
          lnodes[2] = _n[1];
          lnodes[3] = _n[1];
          lnodes[4] = *x;

          P = daports[daportno++];
          break;
        case vpiNet: // -> ivl
          src='I';
          trace1("DEV_IVL  ==> Reg: " + string(name) + " placing DEV_LOGIC_AD", n);
          x = new node_t();
          x->new_model_node("i_"+name, this); // internal node hack. FIXME (remove)
          lnodes[0] = *x;
          lnodes[1] = _n[0];
          lnodes[2] = _n[1];
          lnodes[3] = _n[1];
          lnodes[4] = _n[n];
          P = adports[adportno++];
          ((DEV_LOGIC_AD*) P)->H = item;
          break;

        default:
          // which other types would make sense?
          continue;
      }

      assert(_n[n].n_());

      assert(P);
      subckt()->push_front(P);

      trace0("modelname: " + c->_eval_ivl->modelname());

      COMMON_LOGIC* L = (COMMON_LOGIC*) c->_eval_ivl;
      P->set_parameters(name, this, L, 0, 0, NULL, 5 , lnodes);

      assert(c->_eval_ivl);

      trace1("DEV_IVL loop end for "+name, n);
      n++;
    } 

    _comp->vpi_mode_flag(VPI_MODE_NONE);

    trace1 ("First expanding done" +long_label(), net_nodes());
  } // 1st expand

//  std::string subckt_name(c->modelname()+"."+string(c->vvpfile));
  assert(!is_constant());
// is this an option?
// subckt()->set_slave();
  subckt()->expand();

  trace1("had ivl subcommon ", hp(c->_eval_ivl));

  for (CARD_LIST::const_iterator i = subckt()->begin(); i != subckt()->end(); ++i) {
    _subcommon = ((const COMPONENT*)(*i))->common() ;
    trace2("have ivl subcommon ", (*i)->long_label(), hp(_subcommon));

    //assert(all_equal...)
  }

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
    return ( schedule_list() )?
      static_cast<double>(schedule_list()->delay) : -1.0 ;
  }else if (Umatch(x, "prec ")) {
    return ( sc->time_precision() );
  }else if (Umatch(x, "st ")) {
    return  static_cast<double> ( sc->schedule_time() );
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

  event_time_s* ctim = schedule_list();

  if (ctim){
    trace3("DEV_IVL_BASE::tr_review", sc->schedule_time(), ctim->delay, _sim->_time0 );
    double delay = sc->event_absolute(ctim);
    trace1("DEV_LOGIC_VVP::tr_review", delay );
    _time_by.min_event(delay + _sim->_time0);
  } else {
    trace0("no event");
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
      incount(0),
      _eval_ivl(0)
{
  trace1("COMMON_IVL::COMMON_IVL" , c);
  trace1("COMMON_IVL::COMMON_IVL" + modelname(), c);
  ++_count;
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
COMMON_IVL::COMMON_IVL(const COMMON_IVL& p)
      :COMMON_COMPONENT(p),
      incount(p.incount),
      _eval_ivl(p._eval_ivl)
{
  trace0("COMMON_IVL::COMMON_IVL( COMMON_IVL )" + modelname());
  ++_count;
}
/*--------------------------------------------------------------------------*/
//sort of accept?
void EVAL_IVL::contsim_set(double time) const
{
  trace2("EVAL contsim_set", time, CKT_BASE::_sim->_time0);
  contsim("",time);

  SimTimeA  = time;
  SimDelayD = -1;

  return;
}
/*--------------------------------------------------------------------------
enum sim_mode {SIM_ALL,
               SIM_INIT,
               SIM_CONT0,
               SIM_CONT1,
               SIM_PREM,
               SIM_DONE};
--------------------------------------------------------------------------*/
void EVAL_IVL::catch_up(double time) const
{
  vvp_time64_t discrete_time = discrete_floor(time);
  vvp_time64_t delta = discrete_time - schedule_time();


  struct event_time_s* ctim = schedule_list();
  if (ctim) {
    //assert(ctim->delay >= delta);
    ctim->delay -= delta;
  }
  schedule_time(discrete_time);

}
/*-------------------------------------------------------------*/
vvp_time64_t EVAL_IVL::contsim(vvp_time64_t until_rel) const
{
  trace1("EVAL_IVL::contsim", until_rel);
  vvp_time64_t time0 = schedule_time();
  vvp_time64_t until_abs = until_rel + schedule_time();
  // ivl events before 'until_abs' might be changed by analogue transitions.
  // so execute only until then.
  //
  struct event_time_s* ctim;
  while( ( ctim = schedule_list() ) ) {

    /* If the time is advancing, then first run the
       postponed sync events. Run them all. */
    if (ctim->delay > 0) {

      if (ctim->delay + schedule_time() > until_abs){
        return time0 - schedule_time()+ctim->delay;
      }

      schedule_time( schedule_time() + ctim->delay );
      /* When the design is being traced (we are emitting
       * file/line information) also print any time changes. */
      ctim->delay = 0;

      vpiNextSimTime();
      // Process the cbAtStartOfSimTime callbacks.
      assert (!ctim->start);
    }

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
          run_rosync(ctim);
          schedule_list( ctim->next );
          delete ctim;
          continue;
        }
      }
    }

    /* Pull the first item off the list. If this is the last
       cell in the list, then clear the list. Execute that
       event type, and delete it. */
    struct event_s*cur = ctim->active->next;
    if (cur->next == cur) {
      ctim->active = 0;
    } else {
      ctim->active->next = cur->next;
    }

    cur->single_step_display();
    cur->run_run();

    delete (cur);
  }
  return -1;
} 
/*--------------------------------------------------------------------------*/
double EVAL_IVL::contsim(const char *,double time) const
{
  trace2("EVAL_IVL::contsim", time, SimTimeDlast);
  SimTimeA  = time;
  SimDelayD = -1;

  SimState = SIM_CONT0;
  while (SimTimeDlast < time) {
    trace1("IVL_EVAL contsim loop", time);
    SimState = this->schedule_simulate_m(SimState);
    SimTimeDlast = SimTimeD;
    if (SIM_PREM <= SimState) break;
  }

  return SimDelayD;
} 
/*--------------------------------------------------------------------------*/
double EVAL_IVL::tr_begin()const
{
  trace0("EVAL_IVL::tr_begin");
  static int done;
  if(done) return 0;
  done=1;
  _comp->cleanup();

  SimDelayD  = -1;
  // SimState = schedule_simulate_m(SIM_INIT);
  some_tr_begin_stuff();
  SimState = schedule_cont0();
  SimTimeDlast = SimTimeD;

  return SimDelayD;
} 
/*--------------------------------------------------------------------------*/
void EVAL_IVL::schedule_transition(vpiHandle H,
    vvp_time64_t  dly, vvp_vector4_t val, int a, int b)const
{
  trace3("EVAL_IVL::schedule_assign_plucked_vector ", dly, a,b);
  __vpiSignal* HS = (__vpiSignal*) H;
  vvp_sub_pointer_t<vvp_net_t> ptr(HS->node,0);

  ::schedule_assign_plucked_vector(ptr,dly,val,a,b);
}
/*--------------------------------------------------------------------------*/
void EVAL_IVL::some_tr_begin_stuff() const {
  // FIXME: move where it belongs...
  assert(schedule_time()==0);

  trace0("Execute end of compile callbacks"); // move away
  vpiEndOfCompile();
  trace0("Done EOC");

  // Execute initialization events.
  trace0("EVAL_IVL:: init events..."); //  + long_label());
  exec_init_list();

  trace0("calling vpiStartOfSim");
  vpiStartOfSim();
  assert(schedule_runnable());
  assert(! schedule_stopped());
}
/*--------------------------------------------------------------------------*/
void run_fifo_run( event_s* q ){
  while (q) {
    struct event_s*cur = q->next;
    if (q == cur) {
      q = 0;
    } else {
      q->next = cur->next;
    }
    cur->run_run();
    delete (cur);
  }
}
/*--------------------------------------------------------------------------*/
sim_mode EVAL_IVL::schedule_simulate_m(sim_mode mode) const 
{
  trace1("EVAL_IVL schedule_simulate_m", mode);
  trace_queue( schedule_list());
  struct event_s      *cur  = 0;
  struct event_time_s *ctim = 0;
  double               d_dly;

  switch (mode) {
    case SIM_CONT0: if ((ctim = schedule_list())) goto sim_cont0;
                      goto done;
    case SIM_CONT1: goto sim_cont1;
    default:
                    break;
  }

  unreachable(); //PREM? here?
  assert(false); 
  some_tr_begin_stuff();

    while ( ( ctim = schedule_list())) {
      trace0("EVAL_IVL schedule_list");

      /* If the time is advancing, then first run the
         postponed sync events. Run them all. */
      if (ctim->delay > 0) {
        switch (mode) {
          case SIM_CONT0:
          case SIM_CONT1:
          case SIM_INIT:

            trace3("EVAL_IVL SIM_SOME", SimTimeD, SimTimeA, CKT_BASE::_sim->_time0);
            d_dly = getdtime(ctim);
            assert(d_dly>0);
            if (d_dly > 0) {
              trace5("EVAL_IVL ", d_dly, CKT_BASE::_sim->_time0, ctim->delay, hp(this), hp(ctim));
              //doExtPwl(sched_list->nbassign,ctim);
              SimDelayD = d_dly; return SIM_CONT0; 
sim_cont0:
              double dly = getdtime(ctim),
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

        trace2("EVAL_IVL advance", ctim->delay, schedule_time()  );
        schedule_time(schedule_time() + ctim->delay);
        ctim->delay = 0;
        trace3("EVAL_IVL calling vpiNextSimTime", ctim->delay, schedule_time(), CKT_BASE::_sim->_time0  );

        vpiNextSimTime(); // execute queued callbacks 

        // Process the cbAtStartOfSimTime callbacks.
        trace0("EVAL_IVL cbAtStartOfSimTime callback");
        run_fifo_run( ctim->start );

      } else  {
        trace0("EVAL_IVL delay == 0" );
      }

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
            trace1("EVAL_IVL rosync", CKT_BASE::_sim->_time0);
            run_rosync( ctim );
            trace1("EVAL_IVL pop  schedule_list", mode);
            set_schedule_list( get_schedule_list()->next );
            trace_queue( schedule_list() );
            trace0("EVAL_IVL done pop");
            switch (mode) {
              case SIM_CONT0:
              case SIM_CONT1:
              case SIM_INIT: 

                d_dly = getdtime(ctim);
                if (d_dly > 0) {
                  trace0("EVAL_IVL noextPWL again");
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

            trace0("EVAL_IVL jumping");
            goto cycle_done;
          } else {
            trace0("EVAL_IVL inactive 3");
          }
        } else {
          trace0("EVAL_IVL inactive 2");
        }
      } else {
        trace0("EVAL_IVL 3 inactive 1");
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

      cur->single_step_display();
      cur->run_run();
      trace_queue(ctim);

      delete (cur);

cycle_done:;
    }

done:
  return SIM_DONE;
}
/*--------------------------------------------------------------------------*/
sim_mode EVAL_IVL::schedule_cont0() const 
{
  trace1("EVAL_IVL schedule_cont0", 17);
  trace_queue( schedule_list());
  struct event_s      *cur  = 0;
  struct event_time_s *ctim = 0;
  double               d_dly;
  sim_mode mode = SIM_CONT0;

  if ((ctim = schedule_list())) goto sim_cont0;
                      goto done;


    while ((ctim = schedule_list())) {
      trace0("EVAL_IVL schedule_list");

      /* If the time is advancing, then first run the
         postponed sync events. Run them all. */
      if (ctim->delay > 0) {
        switch (mode) {
          case SIM_CONT0:
          case SIM_CONT1:
          case SIM_INIT:

            trace3("EVAL_IVL SIM_SOME", SimTimeD, SimTimeA, CKT_BASE::_sim->_time0);
            d_dly = getdtime(ctim);
            if (d_dly > 0) {
              trace5("EVAL_IVL ", d_dly, CKT_BASE::_sim->_time0, ctim->delay, hp(this), hp(ctim));
              SimDelayD = d_dly;
              return SIM_CONT0; 
sim_cont0:
              double dly = getdtime(ctim),
                     te  = SimTimeDlast + dly;
              if (te > CKT_BASE::_sim->_time0 ) {
                SimDelayD = te - SimTimeA;
                trace0("EVAL_IVL PREM");
                return SIM_PREM; 
              }
              SimTimeD = SimTimeDlast + dly;
            }
            break;
          default:
            fprintf(stderr,"deffault?\n");
        }

        trace2("EVAL_IVL advance", ctim->delay, schedule_time()  );
        assert (schedule_runnable());
        schedule_time(schedule_time() + ctim->delay);
        ctim->delay = 0;
        trace3("EVAL_IVL calling vpiNextSimTime", ctim->delay, schedule_time(), CKT_BASE::_sim->_time0  );

        vpiNextSimTime(); // execute queued callbacks 

        // Process the cbAtStartOfSimTime callbacks.
        assert (!ctim->start);

      } else  {
        trace0("EVAL_IVL delay <= 0" );
      }

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
            trace1("EVAL_IVL rosync", CKT_BASE::_sim->_time0);
            run_rosync( ctim );
            trace1("EVAL_IVL pop  schedule_list", mode);
            set_schedule_list( get_schedule_list()->next );
            trace_queue( schedule_list() );
            trace0("EVAL_IVL done pop");
            switch (mode) {
              case SIM_CONT0:
              case SIM_CONT1:
              case SIM_INIT: 

                d_dly = getdtime(ctim);
                if (d_dly > 0) {
                  trace0("EVAL_IVL noextPWL again");
                  // doExtPwl(sched_list->nbassign,ctim);
                  SimDelayD = d_dly;
                  delete ctim;
                  return SIM_CONT1;
                  // SimTimeD += ???;
                  goto cycle_done;
                }
              default:
                fprintf(stderr,"default 2\n");
            }
            delete ctim;

            trace0("EVAL_IVL jumping");
            goto cycle_done;
          } else {
            trace0("EVAL_IVL inactive 3");
          }

        } else {
          trace0("EVAL_IVL inactive 2");
        }
      } else {
        // ctim->active != 0
        trace0("EVAL_IVL ... inactive 1");
        trace_queue(schedule_list());
      }

      /* Pull the first item off the list. If this is the last
         cell in the list, then clear the list. Execute that
         event type, and delete it. */
      trace0("EVAL_IVL ... pull");
      cur = ctim->active->next;
      if (cur->next == cur) {
        ctim->active = 0;
      } else {
        ctim->active->next = cur->next;
      }
      assert(cur);
      trace0("EVAL_IVL ssd");

      cur->single_step_display();
      trace0("EVAL_IVL runrun");
      cur->run_run();
      trace_queue(ctim);
      trace0("EVAL_IVL done run_run");

      delete (cur);

cycle_done:;
    }

done:

  trace0("EVAL_IVL done");
  trace_queue(schedule_list());
  return SIM_DONE;
}
/*--------------------------------------------------------------------------*/
sim_mode EVAL_IVL::schedule_cont1() const 
{
  trace1("EVAL_IVL schedule_cont1", 17);
  trace_queue( schedule_list());
  struct event_s      *cur  = 0;
  struct event_time_s *ctim = 0;
  double               d_dly;

  sim_mode mode = SIM_CONT1;

  while ((ctim = schedule_list())) {
    trace0("EVAL_IVL schedule_list");

    /* If the time is advancing, then first run the
       postponed sync events. Run them all. */
    if (ctim->delay > 0) {
      switch (mode) {
        case SIM_CONT0:
        case SIM_CONT1:
        case SIM_INIT:

          trace3("EVAL_IVL SIM_SOME", SimTimeD, SimTimeA, CKT_BASE::_sim->_time0);
          d_dly = getdtime(ctim);
          if (d_dly > 0) {
            trace5("EVAL_IVL ", d_dly, CKT_BASE::_sim->_time0, ctim->delay, hp(this), hp(ctim));
            SimDelayD = d_dly; return SIM_CONT0; 
            double dly = getdtime(ctim),
                   te  = SimTimeDlast + dly;
            if (te > SimTimeA) {
              SimDelayD = te - SimTimeA;
              return SIM_PREM; 
            }
            SimTimeD = SimTimeDlast + dly;
          }
          break;
        default:
          fprintf(stderr,"deffault?\n");
      }

      trace2("EVAL_IVL advance", ctim->delay, schedule_time()  );
      assert (schedule_runnable());
      schedule_time(schedule_time() + ctim->delay);
      ctim->delay = 0;
      trace3("EVAL_IVL calling vpiNextSimTime", ctim->delay, schedule_time(), CKT_BASE::_sim->_time0  );

      vpiNextSimTime(); // execute queued callbacks 

      // Process the cbAtStartOfSimTime callbacks.
      assert (!ctim->start);

    } else  {
      trace0("EVAL_IVL delay <= 0" );
    }

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
          trace1("EVAL_IVL rosync", CKT_BASE::_sim->_time0);
          run_rosync( ctim );
          trace1("EVAL_IVL pop  schedule_list", mode);
          set_schedule_list( get_schedule_list()->next );
          trace_queue( schedule_list() );
          trace0("EVAL_IVL done pop");
          switch (mode) {
            case SIM_CONT0:
            case SIM_CONT1:
            case SIM_INIT: 

              d_dly = getdtime(ctim);
              if (d_dly > 0) {
                trace0("EVAL_IVL noextPWL again");
                // doExtPwl(sched_list->nbassign,ctim);
                SimDelayD = d_dly;
                delete ctim;
                return SIM_CONT1;
                // SimTimeD += ???;
                goto cycle_done;
              }
            default:
              fprintf(stderr,"default 2\n");
          }
          delete ctim;

          trace0("EVAL_IVL jumping");
          goto cycle_done;
        } else {
          trace0("EVAL_IVL inactive 3");
        }
      } else {
        trace0("EVAL_IVL inactive 2");
      }
    } else {
      trace0("EVAL_IVL 2 inactive 1");
      trace_queue(schedule_list());
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

    trace0("EVAL_IVL active run_run");
    cur->single_step_display();
    cur->run_run();
    trace_queue(ctim);
    trace0("EVAL_IVL done run_run");

    delete (cur);

cycle_done:;
  }

  return SIM_DONE;
}
/*--------------------------------------------------------------------------*/
EVAL_IVL::EVAL_IVL(int c)
      :COMMON_COMPONENT(c), 
      incount(0)
{
  trace1("EVAL_IVL::EVAL_IVL ", c);
  ++_count;
}
/*--------------------------------------------------------------------------*/
EVAL_IVL::EVAL_IVL(const EVAL_IVL& p)
      :COMMON_COMPONENT(p),
      incount(p.incount)
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
  trace2("EVAL_IVL::deflate", _commons.size(), hp(this));
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
  trace1("EVAL_IVL::expand", hp(dev));
  COMMON_COMPONENT::expand(dev);
  attach_model(dev);

  assert(_comp);
  vvpinit(_comp);

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

  bool ret =  cr;
  if (ret){
    trace2("EVAL_IVL these are equal ", hp(this), hp(&x) );
  } else {
    trace2("EVAL_IVL these are unequal ", hp(this), hp(&x) );
  }

  return(ret);

}
/*--------------------------------------------------------------------------*/
int EVAL_IVL::vvpinit(COMPILE_WRAP* ) {
  static int foo;
  if (foo) return 1;
  foo = 1;
  SimTimeD = 0;
  SimTimeA = 0;
  SimTimeDlast = 0;
  SimDelayD = -1;
  
  _time_prec = vpip_get_time_precision();

  if(_time_prec>-1 || _time_prec<-12){
    error(bDANGER,"no time precision %i?\n", _time_prec);
    vpip_set_time_precision(-12); // FIXME
    _time_prec = vpip_get_time_precision();
  }
  trace2("vvp init", _time_prec, hp(this));

  assert( -12<=_time_prec && _time_prec<0 ); 
  timescale = pow(10.,_time_prec);

        trace2("set timescale", timescale, _time_prec);

//  compile->flush(); //??

  return 0;
  //return( (*so_main)(args) );
}
/*--------------------------------------------------------------------------*/
void COMMON_IVL::precalc_first(const CARD_LIST* par_scope)
{
  assert(par_scope);

  COMMON_COMPONENT::precalc_first(par_scope);
  //vvpfile.e_val("UNSET", par_scope);
  //module.e_val("UNSET", par_scope);

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

  COMMON_COMPONENT* eval = (COMMON_COMPONENT*) new EVAL_IVL();
  trace1("COMMON_IVL::expand model "+ modelname(), hp(eval));

  eval->set_modelname(modelname());

  //eval->attach(model());
  //
  // this is STUPID.
  // use the logic model attached to the parent device...
  // all EVAL_IVL's need to be identical (and have the same logic model)
  eval->attach( 
      (MODEL_CARD*) (
        ((const MODEL_IVL_BASE*)model())->logic_model()
        )
      );

  trace1("COMMON_IVL attaching original eval common", hp(eval));
  attach_common(eval, &_eval_ivl);
  trace2("COMMON_IVL attached original eval common", hp(eval), hp(_eval_ivl));
}
/*--------------------------------------------------------------------------*/
void COMMON_IVL::precalc_last(const CARD_LIST* par_scope)
{
  COMMON_COMPONENT::precalc_last(par_scope);
}
/*--------------------------------------------------------------------------*/
int COMMON_IVL::compile_design(COMPILE_WRAP*c, COMPONENT* p, COMPONENT** da)const{
  const MODEL_IVL_BASE* m = dynamic_cast<const MODEL_IVL_BASE*>(model());
  trace0("COMMON_IVL::compile_design");

  return m->compile_design(c, p->short_label(), da, this);
}
/*--------------------------------------------------------------------------*/
void DEV_IVL_BASE::init_vvp() // const CARD_LIST* par_scope)
{
  assert(false);
//  FIXME COMMON has to do the compiler...
  if(!_comp){
    _comp = new COMPILE_WRAP();

  } else {
    trace0("COMMON_IVL::precalc_last already done init_vvp");
  }
  trace0("COMMON_IVL::precalc_last done");
}
/*--------------------------------------------------------------------------*/
COMMON_IVL::~COMMON_IVL()	{
  --_count;

  trace1("COMMON_IVL::~COMMON_IVL ", hp(_eval_ivl));
  detach_common(&_eval_ivl);

//  for( vector<COMMON_COMPONENT*>::iterator i = _subcommons.begin();
//      i!=_subcommons.end(); ++i){
//    trace1("COMMON_IVL::~ deleting ", hp((*i)));
//    delete (&(*i));
//  }
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
    default: return COMMON_COMPONENT::param_value(i);
  }
}
/*--------------------------------------------------------------------------*/
void COMMON_IVL::set_param_by_index(int i, std::string& value, int offset)
{
  switch (COMMON_COMPONENT::param_count() - 1 - i) {
    default: COMMON_COMPONENT::set_param_by_index(i, value, offset); break;
  }
}
/*--------------------------------------------------------------------------*/
COMMON_COMPONENT* COMMON_IVL::clone()const
{
  return new COMMON_IVL(*this);
}
/*--------------------------------------------------------------------------
bool COMMON_BUILT_IN_MOS::operator==(const COMMON_COMPONENT& x)const
{
  const COMMON_BUILT_IN_MOS* p = dynamic_cast<const COMMON_BUILT_IN_MOS*>(&x);
  return (p
    && l_in == p->l_in
    && w_in == p->w_in
    && ad_in == p->ad_in
    && as_in == p->as_in
    && pd == p->pd
    && ps == p->ps
    && nrd == p->nrd
    && nrs == p->nrs
    && _sdp == p->_sdp
    && COMMON_COMPONENT::operator==(x));
--------------------------------------------------------------------------*/
bool COMMON_IVL::operator==(const COMMON_COMPONENT& x )const
{
  const COMMON_IVL* p = dynamic_cast<const COMMON_IVL*>(&x);
  //trace2("COMMON_IVL::operator==", hp(this), hp(&x) );

  return(p
      && COMMON_COMPONENT::operator==(x)
      );

 //   && (module==p->module); // bad idea...?

}
/*--------------------------------------------------------------------------*/
COMMON_COMPONENT* COMMON_IVL::no_deflate()
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
const double* COMMON_IVL::timescale()const{ 
  return &EVAL_IVL::timescale; // for now, lets timescale be const.
}
/*--------------------------------------------------------------------------*/
/*--MODEL-------------------------------------------------------------------*/
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
    _logic_model=p._logic_model;
    trace0("MODEL_IVL_BASE::MODEL_IVL_BASE");
  }
/*--------------------------------------------------------------------------*/
//const double* MODEL_IVL_BASE::timescale()const{ }
/*--------------------------------------------------------------------------*/

void yyerror(const char*msg){
        error(bDANGER, "%s\n",msg);
}

ofstream debug_file;

