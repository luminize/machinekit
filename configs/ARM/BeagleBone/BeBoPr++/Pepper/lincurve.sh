#!/bin/sh

halcmd setp lincurve.0.y-val-00 -4.2
halcmd setp lincurve.0.y-val-01 0
halcmd setp lincurve.0.y-val-02 4.2
halcmd setp lincurve.0.x-val-00 -35
halcmd setp lincurve.0.x-val-01 0
halcmd setp lincurve.0.x-val-02 35
halcmd setp hal_pru_generic.stepgen.00.maxaccel 3200
halcmd setp hal_pru_generic.stepgen.01.maxaccel 3200
halcmd setp hal_pru_generic.stepgen.02.maxaccel 3200
#halcmd setp hal_pru_generic.stepgen.00.maxvel 200
#halcmd setp hal_pru_generic.stepgen.01.maxvel 200
#halcmd setp hal_pru_generic.stepgen.02.maxvel 200
#halcmd setp hal_pru_generic.stepgen.03.maxaccel 2350
halcmd setp hal_pru_generic.stepgen.03.position-scale -2050
#halcmd sets e0.temp.set 215