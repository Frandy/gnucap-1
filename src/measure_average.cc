/*$Id: measure_average.cc,v 1.3 2010-09-07 07:46:23 felix Exp $ -*- C++ -*-
 * vim:ts=8:sw=2:et
 * Copyright (C) 2008 Albert Davis
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
 */
#include "u_parameter.h"
#include "m_wave.h"
#include "u_function.h"
/*--------------------------------------------------------------------------*/
namespace {
/*--------------------------------------------------------------------------*/
class MEASURE : public WAVE_FUNCTION {
  PARAMETER<double> before;
  PARAMETER<double> after;
public:
  MEASURE():
    WAVE_FUNCTION(),
    before(BIGBIG),
    after(-BIGBIG)
  {}
  virtual FUNCTION_BASE* clone()const { return new MEASURE(*this);}
  string label()const {return "average";}
  void expand(CS& Cmd, const CARD_LIST* Scope)
  {
    std::string probe_name;
    PARAMETER<double> before(BIGBIG);
    PARAMETER<double> after(-BIGBIG);
    
    unsigned here = Cmd.cursor();
    Cmd >> probe_name;
    w = find_wave(probe_name);

    if (!w) {
      Cmd.reset(here);
    }else{
    }

    here = Cmd.cursor();
    do {
      ONE_OF
	|| Get(Cmd, "probe",  &probe_name)
	|| Get(Cmd, "before", &before)
	|| Get(Cmd, "after",  &after)
	|| Get(Cmd, "end",    &before)
	|| Get(Cmd, "begin",  &after)
	;
    }while (Cmd.more() && !Cmd.stuck(&here));

    if (!w) {
      w = find_wave(probe_name);
    }else{
    }
    before.e_val(BIGBIG, Scope);
    after.e_val(-BIGBIG, Scope);
  }
  fun_t wave_eval()const
  {
    if (w) {

      WAVE::const_iterator begin = lower_bound(w->begin(), w->end(), DPAIR(after, -BIGBIG));
      WAVE::const_iterator end   = upper_bound(w->begin(), w->end(), DPAIR(before, BIGBIG));
      WAVE::const_iterator lower = begin;

      double area = 0;
      WAVE::const_iterator i = begin;
      while (++i < end) {
	area += .5 * (lower->second + i->second) * (i->first - lower->first);
	lower = i;
      }
      return to_fun_t(area/(lower->first - begin->first));
    }else{
      throw Exception_No_Match(probe_name);
    }
  }
} p4;
DISPATCHER<FUNCTION_BASE>::INSTALL d4(&measure_dispatcher, "average|mean|avg", &p4);
/*--------------------------------------------------------------------------*/
}
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
