#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Log the temperatures reported by the oven in a live plot and
# in a CSV file.
#
# Requires
# python 2.7
# - pyserial (python-serial in ubuntu, pip install pyserial)
# - matplotlib (python-matplotlib in ubuntu, pip install matplotlib)
#

import csv
import datetime
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import serial
import sys
from time import time

# settings
#
FIELD_NAMES = 'Time,Temp0,Temp1,Temp2,Temp3,Set,Actual,Heat,Fan,ColdJ,Mode'
TTYs = ('/dev/ttyUSB0', '/dev/ttyUSB1', '/dev/ttyUSB2')
BAUD_RATE = 115200

logdir = 'logs/'

MAX_X = 470
MAX_Y_temperature = 300
MAX_Y_pwm = 260
#
# end of settings

def timestamp(dt=None):
	if dt is None:
		dt = datetime.datetime.now()

	return dt.strftime('%Y-%m-%d-%H%M%S')


def logname(filetype, profile):
	return '%s%s-%s.%s' % (
		logdir,
		timestamp(),
		profile.replace(' ', '_').replace('/', '_'),
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

class Log(object):
	profile = ''
	last_action = None

	def __init__(self):
		self.init_plot()
		self.clear_logs()

	def clear_logs(self):
		self.raw_log = []
		map(Line.clear, self.lines)
		self.mode = ''

	def init_plot(self):
		plt.ion()

		gs = gridspec.GridSpec(2, 1, height_ratios=(4, 1))
		fig = plt.figure(figsize=(14, 10))

		axis_upper = fig.add_subplot(gs[0])
		axis_lower = fig.add_subplot(gs[1])
		plt.subplots_adjust(hspace=0.05, top=0.95, bottom=0.05, left=0.05, right=0.95)

		# setup axis for upper graph (temperature values)
		axis_upper.set_ylabel(u'Temperature [°C]')
		axis_upper.set_xlim(0, MAX_X)
		axis_upper.set_xticklabels([])
		axis_upper.set_ylim(0, MAX_Y_temperature)

		# setup axis for lower graph (PWM values)
		axis_lower.set_xlim(0, MAX_X)
		axis_lower.set_ylim(0, MAX_Y_pwm)
		axis_lower.set_ylabel('PWM value')
		axis_lower.set_xlabel('Time [s]')

		# select values to be plotted
		self.lines = [
			Line(axis_upper, 'Actual'),
			Line(axis_upper, 'Temp0'),
			Line(axis_upper, 'Temp1'),
			Line(axis_upper, 'Set', u'Setpoint'),
			Line(axis_upper, 'ColdJ', u'Coldjunction'),
		#	Line(axis_upper, 'Temp2'),
		#	Line(axis_upper, 'Temp3'),

			Line(axis_lower, 'Fan'),
			Line(axis_lower, 'Heat', 'Heater')
		]

		axis_upper.legend()
		axis_lower.legend()
		plt.draw()

		self.axis_upper = axis_upper
		self.axis_lower = axis_lower

	def save_logfiles(self):
		print 'Saved log in %s ' % logname('csv', self.profile)
		plt.savefig(logname('png', self.profile))
		plt.savefig(logname('pdf', self.profile))

		with open(logname('csv', self.profile), 'w+') as csvout:
			writer = csv.DictWriter(csvout, FIELD_NAMES.split(','))
			writer.writeheader()

			for l in self.raw_log:
				writer.writerow(l)

	def parse(self, line):
		values = map(str.strip, line.split(','))
		# Convert all values to float, except the mode
		values = map(float, values[0:-1]) + [values[-1], ]

		fields = FIELD_NAMES.split(',')
		if len(values) != len(fields):
			raise ValueError('Expected %d fields, found %d' % (len(fields), len(values)))

		return dict(zip(fields, values))

	def process_log(self, logline):
		# ignore 'comments'
		if logline.startswith('#'):
			print logline
			return

		# parse Profile name
		if logline.startswith('Starting reflow with profile: '):
			self.profile = logline[30:].strip()
			return

		if logline.startswith('Selected profile'):
			self.profile = logline[20:].strip()
			return

		try:
			log = self.parse(logline)
		except ValueError, e:
			if len(logline) > 0:
				print '!!', logline
			return

		if 'Mode' in log:
			# clean up log before starting reflow
			if self.mode == 'STANDBY' and log['Mode'] in ('BAKE', 'REFLOW'):
				self.clear_logs()

			# save png graph an csv file when bake or reflow ends.
			if self.mode in ('BAKE', 'REFLOW') and log['Mode'] == 'STANDBY':
				self.save_logfiles()

			self.mode = log['Mode']
			if log['Mode'] == 'BAKE':
				self.profile = 'bake'

			if log['Mode'] in ('REFLOW', 'BAKE'):
				self.last_action = time()
			self.axis_upper.set_title('Profile: %s Mode: %s ' % (self.profile, self.mode))

		if 'Time' in log and log['Time'] != 0.0:
			if 'Actual' not in log:
				return

			# update all lines
			map(lambda x: x.add(log), self.lines)
			self.raw_log.append(log)

		# update view
		plt.draw()

	def isdone(self):
		return (
			self.last_action is not None and
			time() - self.last_action > 5
		)


def loop_all_profiles(num_profiles=6):
	log = Log()

	with get_tty() as port:
		profile = 0
		def select_profile(profile):
			port.write('stop\n')
			port.write('select profile %d\n' % profile)
			port.write('reflow\n')

		select_profile(profile)

		while True:
			logline = port.readline().strip()

			if log.isdone():
				log.last_action = None
				profile += 1
				if profile > 6:
					print 'Done.'
					sys.exit()
				select_profile(profile)

			log.process_log(logline)

def logging_only():
	log = Log()

	with get_tty() as port:
		while True:
			log.process_log(port.readline().strip())

if __name__ == '__main__':
	action = sys.argv[1] if len(sys.argv) > 1 else 'log'

	if action == 'log':
		print 'Logging reflow sessions...'
		logging_only()

	elif action == 'test':
		print 'Looping over all profiles'
		loop_all_profiles()
	else:
		print 'Unknown action', action
