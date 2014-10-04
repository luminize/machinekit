#!/bin/sh
# filament speed
halcmd setp lincurve.1.x-val-00 0.0
halcmd setp lincurve.1.x-val-01 0.31
halcmd setp lincurve.1.x-val-02 0.62
halcmd setp lincurve.1.x-val-03 0.93
halcmd setp lincurve.1.x-val-04 1.21
halcmd setp lincurve.1.x-val-05 1.55

halcmd setp lincurve.1.y-val-00 0.0
halcmd setp lincurve.1.y-val-01 0.005
halcmd setp lincurve.1.y-val-02 0.015
halcmd setp lincurve.1.y-val-03 0.04
halcmd setp lincurve.1.y-val-04 0.09
halcmd setp lincurve.1.y-val-05 0.13

#halcmd setp hal_pru_generic.stepgen.00.maxaccel 3200
#halcmd setp hal_pru_generic.stepgen.01.maxaccel 3200
#halcmd setp hal_pru_generic.stepgen.02.maxaccel 3200
#halcmd setp hal_pru_generic.stepgen.00.maxvel 200
#halcmd setp hal_pru_generic.stepgen.01.maxvel 200
#halcmd setp hal_pru_generic.stepgen.02.maxvel 200
#halcmd setp hal_pru_generic.stepgen.03.maxaccel 2350
#halcmd setp hal_pru_generic.stepgen.03.position-scale -2050
#halcmd sets e0.temp.set 215