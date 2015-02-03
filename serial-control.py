#!/usr/bin/env python
# -*- coding: utf-8 -*-

import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec

import serial

def get_tty():
	for devname in ('/dev/ttyUSB0', '/dev/ttyUSB1', '/dev/ttyUSB2'):
		try:
			port = serial.Serial(devname, baudrate=115200)
			print 'Using serial port %s' % port.name
			return port

		except:
			print 'Tried serial port %s, but failed.' % str(devname)
			pass

	return None

fields = 'Time,Temp0,Temp1,Temp2,Temp3,Set,Actual,Heat,Fan,ColdJ,Mode'.split(',')
def parse(line):

	values = map(str.strip, line.split(','))
	values = map(float, values[0:-1]) + [values[-1], ]

	return dict(
		zip(fields, values)
	)


MAX_X = 680   # width of graph
MAX_Y = 300   # Max value for Y axis (degrees celcius)

plt.ion()

gs = gridspec.GridSpec(2, 1, height_ratios=(3, 1))
fig = plt.figure(figsize=(12, 8))

ax1 = fig.add_subplot(gs[0])
ax2 = fig.add_subplot(gs[1])
plt.subplots_adjust(hspace = 0.1)

# ax = plt.axes()
ax1.set_ylabel(u'Temperature [°C]')
ax1.set_xlim(0, MAX_X)
ax1.set_ylim(0, MAX_Y)

ax2.set_xlim(0, MAX_X)
ax2.set_ylim(0, 256)
ax2.set_ylabel('PWM value')
ax2.set_xlabel('Time [s]')

line_actual, = ax1.plot([], [], label=u'Actual temp')
line_setpoint, = ax1.plot([], [], label=u'Setpoint')
line_coldjunction, = ax1.plot([], [], label=u'Coldjunction temp')

line_fan, = ax2.plot([], [], label='Fan')
line_heater, = ax2.plot([], [], label='Heater')

ax1.legend()
ax2.legend()
plt.draw()

times = []
actual = []
setpoint = []
coldjunction = []

fan = []
heater = []

with get_tty() as port:
	while True:
		logline = port.readline()
		try:
			log = parse(logline)
		except ValueError:
			print 'error parsing:', logline
			continue

		if 'Mode' in log:
			ax1.set_title('Mode: %(Mode)s; Heat: %(Heat)3d; Fan: %(Fan)3d' % log)

		if 'Time' in log and log['Time'] != 0.0:
			if 'Actual' not in log:
				continue

			times.append(log['Time'])
			actual.append(log['Actual'])
			line_actual.set_data(times, actual)

			setpoint.append(log['Set'])
			line_setpoint.set_data(times, setpoint)

			fan.append(log['Fan'])
			line_fan.set_data(times, fan)

			heater.append(log['Heat'])
			line_heater.set_data(times, heater)

			if 'ColdJ' in log and 'ColdJ' != 0.0:
				coldjunction.append(log['ColdJ'])
				line_coldjunction.set_data(times, coldjunction)

		plt.draw()
