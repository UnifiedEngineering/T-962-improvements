#!/usr/bin/env python
# -*- coding: utf-8 -*-

import matplotlib.pyplot as plt

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

fig = plt.figure()
plt.title('T-962 reflow log')

ax = plt.axes(xlim=(0, MAX_X), ylim=(0, MAX_Y))
ax.set_ylabel(u'Temperature [°C]')
ax.set_ylabel('Time [s]')

line_actual, = ax.plot([], [], label=u'Actual temp')
line_setpoint, = ax.plot([], [], label=u'Setpoint')
line_coldjunction, = ax.plot([], [], label=u'Coldjunction temp')

plt.legend()
plt.draw()

times = []
actual = []
setpoint = []
coldjunction = []

with get_tty() as port:
	while True:
		logline = port.readline()
		try:
			log = parse(logline)
		except ValueError:
			print 'error parsing:', logline
			continue

		if 'Mode' in log:
			ax.set_title('Mode: %(Mode)s; Heat: %(Heat)3d; Fan: %(Fan)3d' % log)

		if 'Time' in log and log['Time'] != 0.0:
			if 'Actual' not in log:
				continue

			times.append(log['Time'])
			actual.append(log['Actual'])
			line_actual.set_data(times, actual)

			setpoint.append(log['Set'])
			line_setpoint.set_data(times, setpoint)

			if 'ColdJ' in log and 'ColdJ' != 0.0:
				coldjunction.append(log['ColdJ'])
				line_coldjunction.set_data(times, coldjunction)

		plt.draw()
