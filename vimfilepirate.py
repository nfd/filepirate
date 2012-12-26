"""
Python Vim plugin for Filepirate.<ESC>
:arrr
E492: Not an editor command: arrr
:q!


File Pirate by Nicholas FitzRoy-Dale. BSD license.

Many of the Vim incantations were heavily based upon, and in several cases directly copied from, Wincent Colaiuta's Command-T plugin -- much appreciated. Command-T is an excellent plugin and was the inspiration for this one. It's available here: https://wincent.com/products/command-t
"""
import threading
import string
import vim
import time

import filepirate

POLL_INTERVAL = 100 # milliseconds
DUMMY_FILEPIRATE = True # Debug -- provide bogus results
DUMMY_FILEPIRATE_DELAY = 3 # Seconds
MAX_RESULTS = 10

BUFFER_OPTIONS = [
	'bufhidden=unload',  # unload buf when no longer displayed
	'buftype=nofile',    # buffer is not related to any file
	'nomodifiable',      # prevent manual edits
	'noswapfile',        # don't create a swapfile
	'nowrap',            # don't soft-wrap
	'nonumber',          # don't show line numbers
	'nolist',            # don't use List mode (visible tabs etc)
	'foldcolumn=0',      # don't show a fold column at side
	'foldlevel=99',      # don't fold anything
	'nocursorline',      # don't highlight line cursor is on
	'nospell',           # spell-checking off
	'nobuflisted',       # don't show up in the buffer list
	'textwidth=0'        # don't hard-wrap (break long lines)
]

GLOBAL_OPTIONS = {
	'showcmd': False, # Show what's being typed in the status bar
}

NORMAL_KEYS = string.letters + string.digits + ' '
SPECIAL_KEYS = {13: 'filepirate_accept',
		27: 'filepirate_cancel'}
SPINNER = r'/-\|'

class FilePirateThread(threading.Thread):
	"""
	This runs in the background and searches for things the user types.  When
	the search is complete, "callback" gets called with a list of matching
	names.  While searches are in progress (~= search started and "callback"
	not yet called), new searches can be enqueued. "callback" is only called
	when the final such enqueued search completes.

	The idea behind this is that we are searching as a user types a search term
	interactively.  After each character we perform a search and update the
	display. However, we may fall behind the user. In this case, the user will
	type several characters while we are searching.  These will all be enqueued
	as separate searches, but the user will only care about the results of the
	one corresponding to what she most recently typed, i.e. the last enqueued
	search.

	TODO: An obvious improvement is to add support for cancelling in-progress
	searches, perhaps with a flag that the native code can check once per
	directory.
	"""
	def __init__(self):
		threading.Thread.__init__(self)
		self.daemon = True
		self.search_terms = []
		self.lock = threading.Lock()
		self.cond = threading.Condition(self.lock)
		self.idle = True
		if DUMMY_FILEPIRATE:
			self.do_search = self.do_search_dummy
			self.dummy_counter = 0
		else:
			self.do_search = self.do_search_fp

	def run(self):
		while True:
			self.cond.acquire()

			if not self.search_terms:
				self.idle = True
				self.cond.wait()

			if self.search_terms:
				self.idle = False
				term = self.search_terms[-1]
				self.search_terms = []

			self.cond.release()

			results = self.do_search(term)

			self.cond.acquire()
			if not self.search_terms: # Still good!
				self.idle = True
				self.results = results
			self.cond.release()
	
	def do_search_fp(self, term):
		raise NotImplementedError()

	def do_search_dummy(self, term):
		self.dummy_counter += 1
		time.sleep(DUMMY_FILEPIRATE_DELAY)
		return ['Test file - %d - %s' % (self.dummy_counter, term) for i in range(10)]

	def search(self, term):
		self.cond.acquire()
		self.search_terms.append(term)
		self.cond.notify()
		self.cond.release()

class VimAsync(object):
	# FIXME: This is a massive hack.
	def __init__(self):
		self.running = False
		self.clear()
		self.saved_updatetime = 4000
	
	def clear(self):
		self.callback = None
		self.callback_args = None
	
	def start(self, callback, *args):
		self.callback = callback
		self.callback_args = args
		if not self.running:
			# Set up our CursorHold autocommand callback
			self.saved_updatetime = int(vim.eval('&updatetime'))
			vim.command('set updatetime=%d' % (POLL_INTERVAL))
			vim.command("au CursorHold * python filepirate_callback()")
			# The magic key we remap for KeyHold timer updates
			vim.command('noremap <silent> <buffer> <C-A> <Nop>')
			self.running = True
	
	def stop(self):
		vim.command('set updatetime=%d' % (self.saved_updatetime))
		vim.command("au! CursorHold *")
		self.running = False
		self.clear()

	def from_vim(self):
		assert self.running
		self.callback(*self.callback_args)
		# "request another callback" (see comment at start of this class)
		vim.command(r'call feedkeys("\<C-A>")')

class VimFilePirate(object):
	def __init__(self):
		# The File Pirate buffer
		self.buf = None
		self.async = VimAsync()
		self.fp = FilePirateThread()
		self.fp.start()
		self.searching = False
		self.term = '' # search term
		self.stored_vim_globals = {}

	def search_complete(self, results):
		# TODO: update pirate window with latest results
		pass

	def buffer_create(self):
		vim.command('silent! topleft 1split FilePirate')

		for option in BUFFER_OPTIONS:
			vim.command('setlocal ' + option)

		assert 'FilePirate' in vim.current.buffer.name

		self.buffer_register_keys()
		self.buf = vim.current.buffer

		vim.command('setlocal modifiable')
		for idx in range(MAX_RESULTS):
			if len(self.buf) - 2 < idx:
				self.buf.append('')
		vim.command('setlocal nomodifiable')
		vim.current.window.height = MAX_RESULTS + 1
	
	def buffer_register_keys(self):
		for key in 'q': # NORMAL_KEYS
			ascii = ord(key)
			vim.command('noremap <silent> <buffer> <Char-%d> :python filepirate_key(%d)<CR>' % (ascii, ascii))

		for ascii, cmd in SPECIAL_KEYS.items():
			vim.command('noremap <silent> <buffer> <Char-%d> :python %s()<CR>' % (ascii, cmd))
	
	def search_poll(self):
		if self.searching is True and self.fp.idle is True:
			self.async.stop()
			self.searching = False
			self.show_results(self.fp.results)
	
	def show_results(self, results):
		vim.command('setlocal modifiable')
		for idx, result in enumerate(results):
			self.buf[idx + 1] = result
		vim.command('setlocal nomodifiable')

	def set_global_options(self):
		""" Remember the previous global options settings, and set our ones. """
		for opt in GLOBAL_OPTIONS:
			self.stored_vim_globals[opt] = bool(vim.eval('&' + opt))
			setter = opt if GLOBAL_OPTIONS[opt] else 'no' + opt
			vim.command('set ' + setter)
	
	def reset_global_options(self):
		""" Restore settings saved in set_global_options() """
		for opt in self.stored_vim_globals:
			setter = opt if self.stored_vim_globals[opt] else 'no' + opt
			vim.command('set ' + setter)

	# Public API
	def filepirate_open(self):
		self.buffer_create()
		self.set_global_options()
	
	def filepirate_close(self):
		self.reset_global_options()
		vim.command("close");
		vim.command("silent! bunload! #%d" % (self.buf.number))

	def filepirate_key(self, ascii):
		self.term += chr(ascii)
		self.searching = True
		self.fp.search(self.term)
		self.async.start(self.search_poll)
	
	def filepirate_accept(self):
		self.filepirate_close()

	def filepirate_cancel(self):
		self.filepirate_close()

# Singleton
vim_file_pirate = VimFilePirate()

# Exposed to VIM
filepirate_open     = vim_file_pirate.filepirate_open
filepirate_key      = vim_file_pirate.filepirate_key
filepirate_callback = vim_file_pirate.async.from_vim
filepirate_accept   = vim_file_pirate.filepirate_accept
filepirate_cancel   = vim_file_pirate.filepirate_cancel

