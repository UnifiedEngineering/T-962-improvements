#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Log the
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec


import csv
import datetime
import serial

# settings
#
FIELD_NAMES = 'Time,Temp0,Temp1,Temp2,Temp3,Set,Actual,Heat,Fan,ColdJ,Mode'
TTYs = ('/dev/ttyUSB0', '/dev/ttyUSB1', '/dev/ttyUSB2')
BAUD_RATE = 115200

logdir = 'logs/'

MAX_X = 500
MAX_Y_temperature = 300
MAX_Y_pwm = 260

# end of settings


def timestamp(dt=None):
	if dt is None:
		dt = datetime.datetime.now()

	return dt.strftime('%Y-%m-%d-%H%M%S')


def logname(filetype, profile):
	return '%s%s-%s.%s' % (
		logdir,
		timestamp(),
		profile.replace(' ', '_'),
		filetype
	)


def get_tty():
	for devname in TTYs:
		try:
			port = serial.Serial(devname, baudrate=BAUD_RATE)
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

def save_logfiles(profile, raw_log):
	plt.savefig(logname('png', profile))
	import csv
	with open(logname('csv', profile), 'w+') as csvout:
		writer = csv.DictWriter(csvout, FIELD_NAMES.split(','))
		writer.writeheader()

		for l in raw_log:
			writer.writerow(l)

class Line(object):
	def __init__(self, axis, key, label=None):
		self.xvalues = []
		self.yvalues = []

		self._key = key
		self._line, = axis.plot(self.xvalues, self.yvalues, label=label or key)

	def add(self, log):
		self.xvalues.append(log['Time'])
		self.yvalues.append(log[self._key])

		self.update()

	def update(self):
		self._line.set_data(self.xvalues, self.yvalues)

	def clear(self):
		self.xvalues = []
		self.yvalues = []

		self.update()


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

lines = [
	Line(axis_upper, 'Actual'),
	Line(axis_upper, 'Set', u'Setpoint'),
	Line(axis_upper, 'ColdJ', u'Coldjunction'),

	Line(axis_lower, 'Fan'),
	Line(axis_lower, 'Heat', 'Heater')
]

axis_upper.legend()
axis_lower.legend()
plt.draw()

mode = ''
profile = ''
raw_log = []

with get_tty() as port:
	while True:
		logline = port.readline().strip()

		# ignore 'comments'
		if logline.startswith('#'):
			print logline
			continue

		# parse Profile name
		if logline.startswith('Starting reflow with profile: '):
			profile = logline[30:].strip()
			continue

		try:
			log = parse(logline)
		except ValueError, e:
			if len(logline) > 0:
				print '!!', logline
			continue

		if 'Mode' in log:
			# clean up log before starting reflow
			if mode == 'STANDBY' and log['Mode'] in ('BAKE', 'REFLOW'):
				raw_log = []
				map(Line.clear, lines)

			# save png graph an csv file when bake or reflow ends.
			if mode in ('BAKE', 'REFLOW') and log['Mode'] == 'STANDBY':
				save_logfiles(profile, raw_log)

			mode = log['Mode']
			if log['Mode'] == 'BAKE':
				profile = 'bake'

			axis_upper.set_title('Profile: %s Mode: %s ' % (profile, mode))

		if 'Time' in log and log['Time'] != 0.0:
			if 'Actual' not in log:
				continue

			# update all lines
			map(lambda x: x.add(log), lines)
			raw_log.append(log)

		# update view
		plt.draw()
