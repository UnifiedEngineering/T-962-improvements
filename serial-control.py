#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Log the
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec

import serial

FIELD_NAMES = 'Time,Temp0,Temp1,Temp2,Temp3,Set,Actual,Heat,Fan,ColdJ,Mode'

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


def parse(line):
	values = map(str.strip, line.split(','))
	# Convert all values to float, except the mode
	values = map(float, values[0:-1]) + [values[-1], ]

	fields = FIELD_NAMES.split(',')
	if len(values) != len(fields):
		raise ValueError('Found expected %d fields, found %d' % (len(fields), len(values)))

	return dict(zip(fields, values))

MAX_X = 680
MAX_Y_temperature = 300
MAX_Y_pwm = 260

plt.ion()

gs = gridspec.GridSpec(2, 1, height_ratios=(3, 1))
fig = plt.figure(figsize=(12, 8))

axis_upper = fig.add_subplot(gs[0])
axis_lower = fig.add_subplot(gs[1])
plt.subplots_adjust(hspace = 0.1)

# setup axis for upper graph (temperature values)
axis_upper.set_ylabel(u'Temperature [°C]')
axis_upper.set_xlim(0, MAX_X)
axis_upper.set_ylim(0, MAX_Y_temperature)

# setup axis for lower graph (PWM values)
axis_lower.set_xlim(0, MAX_X)
axis_lower.set_ylim(0, MAX_Y_pwm)
axis_lower.set_ylabel('PWM value')
axis_lower.set_xlabel('Time [s]')

line_actual, = axis_upper.plot([], [], label=u'Actual')
line_setpoint, = axis_upper.plot([], [], label=u'Setpoint')
line_coldjunction, = axis_upper.plot([], [], label=u'Coldjunction')

line_fan, = axis_lower.plot([], [], label='Fan')
line_heater, = axis_lower.plot([], [], label='Heater')

axis_upper.legend()
axis_lower.legend()
plt.draw()

# x values
times = []

# y values
actual = []
setpoint = []
coldjunction = []
fan = []
heater = []

profile = ''

with get_tty() as port:
	while True:
		logline = port.readline()
		# ignore 'comments'
		if logline[0] == '#':
			print logline
			continue
		if logline.startswith('Starting reflow with profile: '):
			profile = logline[30:].strip()
			continue
			
		try:
			log = parse(logline)
		except ValueError, e:
			print 'error parsing:', str(e)
			print logline

			continue

		if 'Mode' in log:
			log.update(dict(profile=profile))
			axis_upper.set_title('Profile: %(profile)s \nMode: %(Mode)s; Heat: %(Heat)3d; Fan: %(Fan)3d' % log)

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

		# update view
		plt.draw()
