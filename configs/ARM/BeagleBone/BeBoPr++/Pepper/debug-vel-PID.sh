#!/bin/sh
# filament speed
# set to "zero"
#halcmd setp lincurve.0.x-val-00 0
#halcmd setp lincurve.0.x-val-01 0
#halcmd setp lincurve.0.x-val-02 0
#halcmd setp lincurve.0.x-val-03 0
#halcmd setp lincurve.0.x-val-04 0
#halcmd setp lincurve.0.x-val-05 0

#halcmd setp lincurve.0.y-val-00 0
#halcmd setp lincurve.0.y-val-01 0
#halcmd setp lincurve.0.y-val-02 0
#halcmd setp lincurve.0.y-val-03 0
#halcmd setp lincurve.0.y-val-04 0
#halcmd setp lincurve.0.y-val-05 0

#set to default from halconfig
#halcmd setp lincurve.0.x-val-00 0.0
#halcmd setp lincurve.0.x-val-01 0.0005
#halcmd setp lincurve.0.x-val-02 1
#halcmd setp lincurve.0.x-val-03 2
#halcmd setp lincurve.0.x-val-04 3
#halcmd setp lincurve.0.x-val-05 4

#halcmd setp lincurve.0.y-val-00 0.00
#halcmd setp lincurve.0.y-val-01 0.01
#halcmd setp lincurve.0.y-val-02 0.07
#halcmd setp lincurve.0.y-val-03 0.18
#halcmd setp lincurve.0.y-val-04 0.24
#halcmd setp lincurve.0.y-val-05 0.3

#experimenting
halcmd setp lincurve.0.x-val-00 0.0
halcmd setp lincurve.0.x-val-01 0.35
halcmd setp lincurve.0.x-val-02 1.2
halcmd setp lincurve.0.x-val-03 2
halcmd setp lincurve.0.x-val-04 30
halcmd setp lincurve.0.x-val-05 40

halcmd setp lincurve.0.y-val-00 -0.07
halcmd setp lincurve.0.y-val-01 0.0
halcmd setp lincurve.0.y-val-02 0.072
halcmd setp lincurve.0.y-val-03 0.18
halcmd setp lincurve.0.y-val-04 0.5
halcmd setp lincurve.0.y-val-05 0.5


#halcmd setp hal_pru_generic.stepgen.00.maxaccel 3200
#halcmd setp hal_pru_generic.stepgen.01.maxaccel 3200
#halcmd setp hal_pru_generic.stepgen.02.maxaccel 3200
#halcmd setp hal_pru_generic.stepgen.00.maxvel 200
#halcmd setp hal_pru_generic.stepgen.01.maxvel 200
#halcmd setp hal_pru_generic.stepgen.02.maxvel 200
halcmd setp hal_pru_generic.stepgen.03.maxaccel 5000
halcmd setp hal_pru_generic.stepgen.03.position-scale -2050
#halcmd sets e0.temp.set 215