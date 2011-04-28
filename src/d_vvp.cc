/*$Id: d_logic.cc,v 1.2 2009-12-13 17:55:01 felix Exp $ -*- C++ -*-
 * vim:ts=8:sw=2:et:
 * Copyright (C) 2001 Albert Davis
 * Author: Albert Davis <aldavis@gnu.org>
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
 *------------------------------------------------------------------
 *
 *
 * FIXME: merge extlib into COMMON
 */
//testing=script,sparse 2006.07.17
#include "d_subckt.h"
#include "u_xprobe.h"
#include "d_vvp.h"
#include "ap.h"
#include "u_lang.h"
#include "e_elemnt.h"
#include "m_interp.h"
#include "bm.h"
#include "l_lib.h"
#include "io_trace.h"
#include "vvp/vvp_net.h"
/*--------------------------------------------------------------------------*/

#include "extlib.h"
#include "d_ivl_ports.h"

//#include "schedule.cc"


/*--------------------------------------------------------------------------*/
const double _default_delta (NOT_INPUT);
const int    _default_smooth(0);
/*--------------------------------------------------------------------------*/
// class MODEL_LOGIC_VVP;
class COMMON_LOGIC_VVP;
/*--------------------------------------------------------------------------*/
int MODEL_LOGIC_VVP::_count = -1;
int COMMON_LOGIC_VVP::_count = -1;
int DEV_LOGIC_VVP::_count = -1;
typeof(COMMON_LOGIC_VVP::_commons) COMMON_LOGIC_VVP::_commons;
static COMMON_LOGIC_VVP Default_Logic_Params(CC_STATIC);
static LOGIC_NONE Default_LOGIC(CC_STATIC);
/*--------------------------------------------------------------------------*/
//from extlib.cc
//???
//static int getVoltage(ExtSig *xsig,void *,double *ret) 
//{
//  ret[0] = xsig->d->tr_outvolts();
//  return 1;
//}
/*--------------------------------------------------------------------------*/
void ExtContSim(ExtLib* ext, const char *analysis,double accpt_time) {
  trace0("ExtContSim");
  ExtLib *lib = ext;

  //list<ExtRef*>::iterator scan=lib->refs.begin();
  //lib->now = accpt_time;
  //for (; scan != lib->refs.end(); scan++) {
  //  ExtRef *ref = *scan;
  //  if ('I' == ref->iv) {
  //    list<ExtSig*>::iterator ss=ref->sigs.begin();
  //    for (; ss != ref->sigs.end(); ss++) {
  //      ExtSig *sig = *ss; 
  //      SpcIvlCB *cbd = sig->cb_data;
  //      (*cbd->eval)(cbd,accpt_time,CB_ACCEPT,'I' == ref->iv ? getVoltage
  //          : 0,
  //          sig,0);
  //    }
  //  }   
  //}
  if (lib->active) {
    if (lib->next_time <= accpt_time) lib->active = 0;    
  }
  trace1("tr_accept calling contsim", accpt_time);
  lib->contsim(analysis,accpt_time);
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
COMMON_LOGIC_VVP::COMMON_LOGIC_VVP(int c)
      :COMMON_COMPONENT(c), 
      _extlib(0),
      incount(0),
      file(""),
      module(""),
      status(0),
      _logic_out(0),
      _logic_in(0),
      _logic_none(0)
{
  trace1("COMMON_LOGIC_VVP::COMMON_LOGIC_VVP", c);
  ++_count;
}
/*--------------------------------------------------------------------------*/
COMMON_LOGIC_VVP::COMMON_LOGIC_VVP(const COMMON_LOGIC_VVP& p)
      :COMMON_COMPONENT(p),
      _extlib(p._extlib),
      incount(p.incount),
      file(p.file),
      module(p.module),
      status(p.status),
      _logic_out(0),
      _logic_in(0),
      _logic_none(0)
{
  trace0("COMMON_LOGIC_VVP::COMMON_LOGIC_VVP( COMMON_LOGIC_VVP )");
  ++_count;
}
/*--------------------------------------------------------------------------*/
bool COMMON_LOGIC_VVP::operator==(const COMMON_COMPONENT& x )const{
  const COMMON_LOGIC_VVP* p = dynamic_cast<const COMMON_LOGIC_VVP*>(&x);
  bool ret = (file==p->file);
     //       && (module==p->module); // bad idea...?
  bool cr = COMMON_COMPONENT::operator==(x);

  return ret && cr;
}
/*--------------------------------------------------------------------------*/
COMMON_COMPONENT* COMMON_LOGIC_VVP::deflate()
{
  trace0("COMMON_LOGIC_VVP::deflate");
  // return this;
  for( list<const COMMON_COMPONENT*>::iterator i = _commons.begin();
      i != _commons.end(); ++i ){
    if (*this == **i){
      trace0("COMMON_LOGIC_VVP::deflate hit");
      return const_cast<COMMON_COMPONENT*>( *i );
    }
    trace0("COMMON_LOGIC_VVP::deflate miss");
  }
  _commons.push_back(this);
  return this;
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
bool Getptr(CS& cmd, const std::string& key, PARAMETER<intptr_t>* val)  // consumes key and optional "="
{  // see for "iu_parameter.h" "Get" functions for example and reference
  std::string str;  
  if (cmd.umatch(key + ' ')) {
    if (cmd.skip1b('=')) {
      untested();
      cmd >> str;
    }else{
      str = "";
    }
    *val=0;
    *val=str;
    return true;
  }else {
    str = "";
    *val=str;
    return false;
    }
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

inline OMSTREAM& operator<<(OMSTREAM& o, direction_t t) {
  const std::string s[] = {"", "input", "output", "bus"};
  return (o << s[t]);
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
TIME_PAIR  DEV_LOGIC_VVP::tr_review(){
  return  BASE_SUBCKT::tr_review();
}
/*--------------------------------------------------------------------------*/
bool DEV_LOGIC_VVP::do_tr(){
  assert(status);
  q_accept();
  return BASE_SUBCKT::do_tr();
}
/*--------------------------------------------------------------------------*/
void DEV_LOGIC_VVP::tr_accept(){
  //trace1("DEV_LOGIC_VVP::tr_accept", _sim->_time0);

  subckt()->tr_accept();

  ExtContSim(extlib(),"TRAN",_sim->_time0);     

  uint_t incount=1;
//  FIXME;

  node_t* n=&_n[2];
  for (uint_t ii=0; ii<incount; ++ii) {itested();
    LOGICVAL x = n[ii]->lv();
    /// vvp_net_ptr_t ptr = * _outnet[ii];
    // schedule_set_vector(ptr,data(x) );
  }

  q_eval();
}
/*--------------------------------------------------------------------------*/
void COMMON_LOGIC_VVP::precalc_first(const CARD_LIST* par_scope)
{
  trace0("COMMON_LOGIC_VVP::precalc_first");
  assert(par_scope);

  COMMON_COMPONENT::precalc_first(par_scope);
  file.e_val_normal("UNSET" , par_scope);
  module.e_val_normal("UNSET" , par_scope);
  outports.e_val(1, par_scope);
}
/*--------------------------------------------------------------------------*/
void COMMON_LOGIC_VVP::expand(const COMPONENT* dev ){
  trace1("COMMON_LOGIC_VVP::expand" + dev->long_label(), (intptr_t) dev % PRIME);

  COMMON_COMPONENT::expand(dev);
  attach_model(dev);

  // 
  // dev->outports=outports;

  // COMMON_LOGIC_VVP* c = this;
  const MODEL_LOGIC* m = dynamic_cast<const MODEL_LOGIC*>(model());
  if (!m) {
    throw Exception_Model_Type_Mismatch(dev->long_label(), modelname(), name());
  }

  COMMON_LOGIC* logic_none = new LOGIC_NONE;
  logic_none->set_modelname(modelname());
  logic_none->attach(model());

  attach_common(logic_none, &_logic_none);
}
/*--------------------------------------------------------------------------*/
void COMMON_LOGIC_VVP::precalc_last(const CARD_LIST* par_scope)
{
  assert(par_scope);
  COMMON_COMPONENT::precalc_last(par_scope);

  file.e_val_normal("UNSET" , par_scope);
  module.e_val_normal("UNSET" , par_scope);
  outports.e_val(1, par_scope);

  trace1("COMMON_LOGIC_VVP::precalc_last " + file.string(), status);
  if(!_extlib){
#ifdef SOMETHING_
    /// dlopen has been overwritten!!1
    void* h = dlopen("libvvpg.so",RTLD_LAZY|RTLD_GLOBAL);
    if(h==NULL) throw Exception("cannot open libvvp: %s: ", dlerror());
    trace0("=========== LOADED libvvpg.so ==========");
    dlerror();
#else
    void* h = NULL;
#endif
    _extlib = new ExtLib("foo",h);
    int ret;
    // ret=_extlib->init(("-M. -mbindsigs2 -v "+string(file)+".vvp").c_str());   
    ret=_extlib->init((string(file)+".vvp").c_str());   
    if(dlerror()) throw Exception("cannot init vvp %s", dlerror());

    if(ret)
      error(bDANGER, "somethings wrong with vvp: %s\n", file.string().c_str() );

  } else {
    trace0("COMMON_LOGIC_VVP::precalc_last already done extlib");
  }

  status++;

}
/*--------------------------------------------------------------------------*/
std::string DEV_LOGIC_VVP::port_name(uint_t i)const{
  stringstream a;
  a << "port" << i;
  return a.str();
}
/*--------------------------------------------------------------------------*/
void MODEL_LOGIC_VVP::precalc_first(){
  unreachable();
}
/*--------------------------------------------------------------------------*/
void MODEL_LOGIC_VVP::precalc_last(){
  unreachable();
}
/*--------------------------------------------------------------------------*/
MODEL_LOGIC_VVP::MODEL_LOGIC_VVP(const DEV_LOGIC_VVP* p)
  :MODEL_LOGIC((const COMPONENT*) p){
    trace0("MODEL_LOGIC_VVP::MODEL_LOGIC_VVP");
  }
/*--------------------------------------------------------------------------*/
MODEL_LOGIC_VVP::MODEL_LOGIC_VVP(const MODEL_LOGIC_VVP& p)
  :MODEL_LOGIC(p){
    file=p.file;
    output=p.output;
    input=p.input;
    trace0("MODEL_LOGIC_VVP::MODEL_LOGIC_VVP");
}
/*--------------------------------------------------------------------------*/
bool COMMON_LOGIC_VVP::param_is_printable(int i) const{
    return (COMMON_COMPONENT::param_count() - 1 - i)  < param_count();
}
/*--------------------------------------------------------------------------*/
DEV_LOGIC_VVP::~DEV_LOGIC_VVP() {
  --_count;

  trace0("~DEV_LOGIC_VVP");

/*  for( vector<COMMON_COMPONENT*>::iterator i = _subcommons.begin();
      i!=_subcommons.end(); ++i){
  //   COMMON_COMPONENT::detach_common(&(*i));
  }
  */
}
/*--------------------------------------------------------------------------*/
COMMON_LOGIC_VVP::~COMMON_LOGIC_VVP()	{
  --_count;

  detach_common(&_logic_none);

  for( vector<COMMON_COMPONENT*>::iterator i = _subcommons.begin();
      i!=_subcommons.end(); ++i){
    //trace0("~DEV_LOGIC_VVP detaching from " + short_label());
    delete (&(*i));
  }
}
/*--------------------------------------------------------------------------*/
std::string COMMON_LOGIC_VVP::param_name(int i) const{
    switch (COMMON_COMPONENT::param_count() - 1 - i) {
      case 0: return "file";
      case 1: return "module";
      case 2: return "outports";
      default: return COMMON_COMPONENT::param_name(i);
    }
}
/*--------------------------------------------------------------------------*/
std::string COMMON_LOGIC_VVP::param_name(int i, int j)const
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
std::string COMMON_LOGIC_VVP::param_value(int i)const
{
  switch (COMMON_COMPONENT::param_count() - 1 - i) {
  case 0: return file.string();
  case 1: return module.string();
  case 2: return outports.string();
  default: return COMMON_COMPONENT::param_value(i);
  }
}
/*--------------------------------------------------------------------------*/
COMMON_COMPONENT* COMMON_LOGIC_VVP::clone()const
{
  return new COMMON_LOGIC_VVP(*this);
}
/*--------------------------------------------------------------------------*/
//   MODEL
/*--------------------------------------------------------------------------*/
bool MODEL_LOGIC_VVP::param_is_printable(int i) const{
    return (MODEL_LOGIC::param_count() - 1 - i)  < param_count();
}
/*--------------------------------------------------------------------------*/
std::string MODEL_LOGIC_VVP::param_name(int i, int j)const
{
  if (j == 0) {
    return param_name(i);
  }else if (i >= MODEL_LOGIC::param_count()) {
    return "";
  }else{
    return MODEL_LOGIC::param_name(i, j);
  }
}
/*--------------------------------------------------------------------------*/
void MODEL_LOGIC_VVP::set_param_by_index(int i, std::string& value, int offset)
{
  switch (MODEL_LOGIC_VVP::param_count() - 1 - i) {
  case 0: file = value; break;
  case 1: output = value; break;
  case 2: input = value; break;
  default: MODEL_LOGIC::set_param_by_index(i, value, offset); break;
  }
}
/*--------------------------------------------------------------------------*/
std::string MODEL_LOGIC_VVP::param_name(int i) const{
    switch (MODEL_LOGIC_VVP::param_count() - 1 - i) {
      case 0: return "file_unused";
      case 1: return "input";
      case 2: return "output";
      default: return MODEL_LOGIC::param_name(i);
    }
}
/*--------------------------------------------------------------------------*/
void COMMON_LOGIC_VVP::set_param_by_index(int i, std::string& value, int offset)
{
  switch (COMMON_COMPONENT::param_count() - 1 - i) {
  case 0: file = value; break;
  case 1: module = value; break;
  case 2: outports = value; break;
  default: COMMON_COMPONENT::set_param_by_index(i, value, offset); break;
  }
}
/*--------------------------------------------------------------------------*/
std::string MODEL_LOGIC_VVP::param_value(int i)const
{
  switch (MODEL_LOGIC::param_count() - 1 - i) {
  case 0: return file.string();
  case 1: return output.string();
  case 2: return input.string();
  default: return MODEL_LOGIC::param_value(i);
  }
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/

namespace {
static DEV_LOGIC_VVP p1;
static DISPATCHER<CARD>::INSTALL x1(&device_dispatcher, "vvp", &p1);
}
/*--------------------------------------------------------------------------
namespace MOD_DISP{
static DEV_LOGIC_VVP p1;
static MODEL_LOGIC_VVP p2(&p1);
static DISPATCHER<MODEL_CARD>::INSTALL
d2(&model_dispatcher, "vvpmodel", &p2);
}
--------------------------------------------------------------------------*/
DEV_LOGIC_VVP::DEV_LOGIC_VVP()
  :BASE_SUBCKT(),
  status(0)
{
  trace0("DEV_LOGIC_VVP::DEV_LOGIC_VVP attaching...");
  attach_common(&Default_Logic_Params); // as in mos.cc
  trace0("DEV_LOGIC_VVP::DEV_LOGIC_VVP nodes");
  _n = _nodes;
  ++_count;
}
/*--------------------------------------------------------------------------*/
DEV_LOGIC_VVP::DEV_LOGIC_VVP(const DEV_LOGIC_VVP& p)
  :BASE_SUBCKT(p)
   ,status(0)
{
  for (uint_t ii = 0;  ii < max_nodes();  ++ii) {
    _nodes[ii] = p._nodes[ii];
  }
  _n = _nodes;
  ++_count;
}
/*--------------------------------------------------------------------------*/
PLI_INT32 callback(t_cb_data*x){
  DEV_LOGIC_IN* c= reinterpret_cast<DEV_LOGIC_IN*>(x->user_data);
  assert(c);

  __vpiSignal* signal = (__vpiSignal*) x->obj;
  vvp_sub_pointer_t<vvp_net_t> ptr(signal->node,0);    

  struct t_vpi_value argval;  
  argval.format = vpiIntVal; 
  vpi_get_value(x->obj, &argval);
  const vvp_vector4_t bit(1,(vvp_bit4_t)argval.value.integer);

  trace4("callback", bit.value(0), x->time->real, x->value->value.integer, c->lvfromivl);

  if (CKT_BASE::_sim->_time0 == 0 )  return 0; // UGLY HACK

  switch (bit.value(0)) {
    case 0: c->lvfromivl = lvFALLING; break;
    case 1: c->lvfromivl = lvRISING;  break;
    default: unreachable();           break;
  }
  c->qe();
  return 0;
}
/*--------------------------------------------------------------------------*/
void DEV_LOGIC_VVP::expand()
{
  trace0("DEV_LOGIC_VVP::expand " + long_label());
  BASE_SUBCKT::expand(); // calls common->expand, attaches model
  trace0("DEV_LOGIC_VVP::expand BASE_SUBCKT done");
  assert(_n);

  assert(common());
  const COMMON_LOGIC_VVP* c = prechecked_cast<const COMMON_LOGIC_VVP*>(common());
  assert(c);

  assert(c->model()); // d_mos.cc has a model here...
  const MODEL_LOGIC* m = prechecked_cast<const MODEL_LOGIC*>(c->model());
  assert(m);

  if (!subckt()) {
    new_subckt();
  }else{
  }
  
  if (_sim->is_first_expand()) {
    trace1 ("first expanding " +long_label(), net_nodes());
    precalc_first();
    precalc_last();
    uint_t n=2;

    ExtLib*el=((const COMMON_LOGIC_VVP*) common())->_extlib;
    assert(el);

    vpi_mode_flag = VPI_MODE_COMPILETF;
    trace0("Looking around");
    vpiHandle item;

    const char* modulename="main"; // FIXME

    vpiHandle module = vpi_handle_by_name(modulename,NULL);
    assert(module);

    vpiHandle vvp_device = vpi_handle_by_name(short_label().c_str(),module);
    assert(vvp_device);

    vpiHandle vvp_device_module = vpi_handle(vpiModule,vvp_device);
    assert(vvp_device_module);

    assert ((_n[0].n_()));
    assert ((_n[1].n_()));
    assert ((_n[2].n_()));
    assert ((_n[3].n_()));
    trace1("DEV_LOGIC_VVP::expand "+ short_label() + " entering loop", net_nodes());
    char src;
    COMMON_COMPONENT* logic_common;

    node_t lnodes[] = {_n[n], _n[0], _n[1], _n[1], _n[0]};
//  vpiParameter  holds fall, rise etc.
    trace0 ("listing " +long_label());
    string name;
    vpiHandle net_iterator = vpi_iterate(vpiScope,vvp_device);
    assert(net_iterator);
    CARD* logicdevice;
    node_t* x;
    while ((item = vpi_scan(net_iterator))) {
      int type= vpi_get(vpiType,item);
      name = vpi_get_str(vpiName,item);
      COMPONENT* P;

      switch(type){
        case vpiNet: // <- ivl
          {
          trace2("==> net loop V  " + string(name), item->vpi_type->type_code, n );
          src='V';
          logic_common = c->_logic_none;
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
          trace2("==> net loop to_ivl " + string(name), item->vpi_type->type_code, n );
          src='I';
          x = new node_t();
          x->new_model_node("i_"+name, this);
          lnodes[0] = *x;
          lnodes[1] = _n[0];
          lnodes[2] = _n[1];
          lnodes[3] = _n[1];
          lnodes[4] = _n[n];
          logic_common = c->_logic_none;
          logicdevice = device_dispatcher["port_to_ivl"];
          ((DEV_LOGIC_OUT*) logicdevice)->H = item;
          P = dynamic_cast<COMPONENT*>(logicdevice->clone());
          break;

        default:
          trace1("ignoring", type);
          continue;
      }
      //trace2("DEV_LOGIC_VVP::expand "+ short_label() + " loop", n, noutports);

      trace2("DEV_LOGIC_VVP::expand "+ name + " " + short_label(), n, (intptr_t)logic_common);
      assert(_n[n].n_());

      assert(logicdevice);

      assert(P);
      for (int i=0; i<5; i++ ){
        stringstream a;
        a << short_label() << "." << name << n << "_" << i;
        string port=a.str();
      }

      trace0("pushing controller for "+name);
      subckt()->push_front(P);

      // _e->attach_sig(sig);
      //
      trace3("setting parameters", intptr_t(logic_common), logic_common->attach_count(), n );
      trace0("modelname: " + logic_common->modelname());

      if (src=='I'){
        // to ivl
        COMMON_LOGIC* L = (COMMON_LOGIC*) logic_common;
        //L->ivlhandle = item;
        P->set_parameters(name, this, L, 0, 0, NULL, 5 , lnodes);
      }else{
        // from ivl.
        COMMON_LOGIC* L = (COMMON_LOGIC*) logic_common;
        P->set_parameters(name, this, L, 0, 0, NULL, 5 , lnodes);
      }

      trace1("loop end for "+name, n);
      n++;
    } 
    vpi_mode_flag = VPI_MODE_NONE;
  }

  std::string subckt_name(c->modelname()+"."+string(c->file));

  assert(!is_constant());
//  subckt()->set_slave();
  subckt()->expand();

  trace0("DEV_LOGIC_VVP::expand done");
}
/*--------------------------------------------------------------------------*/
ExtLib* DEV_LOGIC_VVP::extlib()const{
  return (((const COMMON_LOGIC_VVP*) common())->_extlib);
}
/*--------------------------------------------------------------------------*/
void DEV_LOGIC_VVP::tr_begin()
{
  const COMMON_LOGIC_VVP* c = prechecked_cast<const COMMON_LOGIC_VVP*>(common());
  assert(c);

  trace2("DEV_LOGIC_VVP::tr_begin startsim", status, (intptr_t) extlib());
  vvp::startsim("TRAN");

  status++;
  subckt()->tr_begin();
  q_eval();
}
/*--------------------------------------------------------------------------*/
void DEV_LOGIC_VVP::precalc_first()
{
  COMPONENT::precalc_first();
  //const COMMON_LOGIC* c = prechecked_cast<const COMMON_LOGIC*>(common());
  assert(common());

   // FIXME: get ports from file...
   //
  trace1("DEV_LOGIC_VVP::precalc_first, doing subckt()", (intptr_t)subckt());
  if(subckt()){
    subckt()->precalc_first();
  }
}
/*--------------------------------------------------------------------------*/
void DEV_LOGIC_VVP::precalc_last()
{
  trace0("DEV_LOGIC_VVP::precalc_last");
/////////////////////////////////////////
  COMPONENT::precalc_last();

  if (subckt()) {subckt()->precalc_last();}

  assert(common()->model());
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
void DEV_LOGIC_VVP::register_port( vpiHandle net){ // data from ivl
  string netname = vpi_get_str(vpiName, net);
  // int porttype = vpi_get(vpiNetType,net);
        
  int type = net->vpi_type->type_code;

  switch(type) {
    case vpiReg: // -> ivl
      _outport.push_back(net);

      break;
    case vpiNet: // <- ivl

      _inport.push_back(net);


      break;
    default:
      assert(false);
  }
}
/*--------------------------------------------------------------------------*/