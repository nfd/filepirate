# It's snake-t!
import os
import re

import sys
import tty
import termios

class _GetchUnix:
	def __init__(self):
		self.is_setup = False

	def __call__(self):
		if self.is_setup:
			try:
				ch = sys.stdin.read(1)
			except:
				termios.tcsetattr(self.fd, termios.TCSADRAIN, self.old_settings)
				raise
		else:
			self.fd = sys.stdin.fileno()
			self.old_settings = termios.tcgetattr(self.fd)
			try:
				tty.setraw(sys.stdin.fileno())
				ch = sys.stdin.read(1)
			except:
				termios.tcsetattr(self.fd, termios.TCSADRAIN, self.old_settings)
				raise
		return ch

	def setup(self):
		self.fd = sys.stdin.fileno()
		self.old_settings = termios.tcgetattr(self.fd)
		try:
			tty.setraw(sys.stdin.fileno())
		except:
			termios.tcsetattr(self.fd, termios.TCSADRAIN, self.old_settings)
			raise
		self.is_setup = True
	
	def finish(self):
		termios.tcsetattr(self.fd, termios.TCSADRAIN, self.old_settings)


class _GetchWindows:
	def __init__(self):
		import msvcrt

	def __call__(self):
		import msvcrt
		return msvcrt.getch()


getch = _GetchUnix()


class SnakeT(object):
	def __init__(self, root):
		self.root = root
		self.files = []
		self.re_args = re.IGNORECASE
	
	def loadem(self):
		self.files = []
		os.path.walk(self.root, self.load_dir, None)
	
	def load_dir(self, arg, dirname, names):
		self.files.extend(['%s/%s' % (dirname, name) for name in names])
	
	def search_interactive(self):
		split_matcher = ['.*']

		while 1:
			print split_matcher, '>', '\r'
			matcher = re.compile(''.join(split_matcher), self.re_args)
			count = 0
			for filename in self.files:
				if matcher.match(filename):
					count += 1
					if count < 10:
						print '%d %s' % (count, filename), '\r'
					else:
						print '  %s' % (filename), '\r'
				if count == 20:
					break

			char = getch()
			if char == '\r':
				break
			elif char == '\x7f':
				if len(split_matcher) > 1:
					split_matcher.pop()
			else:
				split_matcher.append(char)


if __name__ == '__main__':
	st = SnakeT('/Users/wzdd/Music')
	#st = SnakeT('/Users/wzdd/Projects')
	st.loadem()
	getch.setup()
	try:
		st.search_interactive()
	finally:
		getch.finish()


