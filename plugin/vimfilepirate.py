"""
Python Vim plugin for Filepirate.<ESC>
:arrr
E492: Not an editor command: arrr
:q!


File Pirate by Nicholas FitzRoy-Dale. BSD license.

The Vim incantations contained herein were heavily based upon, and in several cases directly copied from, Wincent Colaiuta's excellent Command-T plugin, available here: https://wincent.com/products/command-t
"""
import threading
import string
import vim
import time
import os

import filepirate

POLL_INTERVAL = 100 # milliseconds
DUMMY_FILEPIRATE = False # Debug -- provide bogus results
DUMMY_FILEPIRATE_DELAY = 3 # Seconds
MAX_RESULTS = 10
PROMPT = '> '
SPINNER_DELAY = 1 # seconds between starting a search and showing the spinner

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

NORMAL_KEYS = string.letters + string.digits + ' .'
SPECIAL_KEYS = {'<CR>': 'filepirate_accept',
		'<Char-27><Char-27>': 'filepirate_cancel',
		'<Up>': 'filepirate_up',
		'<Down>': 'filepirate_down',
		'<BS>': 'filepirate_bs',
		'<C-R>': 'filepirate_rescan'}
SPINNER = r'/-\|'

class FilePirateThread(threading.Thread):
	"""
	This runs in the background and searches for things the user types.  When
	the search is complete, "results" is set to a list of matching names.
	While searches are in progress (= "idle" is False), new searches can be
	enqueued. "results" is only set when the final such enqueued search
	completes.

	The idea behind this is that we are searching as a user types a search term
	interactively.  After each character we perform a search and update the
	display. However, we may fall behind the user. In this case, the user will
	type several characters while we are searching.  These will all be enqueued
	as separate searches, but the user will only care about the results of the
	one corresponding to what she most recently typed, i.e. the last enqueued
	search.

	The reason this is done with idle flags and so on instead of callbacks is
	because Vim doesn't support asynchronous notification, so the interface
	code below is obliged to poll this object for results. It does this every
	POLL_INTERVAL ms (default 0.1 seconds).

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
		self.rescan_requested = False
		if DUMMY_FILEPIRATE:
			self.do_search = self.do_search_dummy
			self.dummy_counter = 0
		else:
			self.do_search = self.do_search_fp
			self.pirates = filepirate.FilePirates(MAX_RESULTS)

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
		pirate = self.pirates.get(os.getcwd())

		if self.rescan_requested:
			pirate.rescan()
			self.rescan_requested = False

		results = pirate.get_candidates(term)
		# FIXME: Hackish, and not necessary (just pretty)
		results = [result[2:] if result.startswith('./') else result for result in results]
		return results

	def do_search_dummy(self, term):
		self.dummy_counter += 1
		time.sleep(DUMMY_FILEPIRATE_DELAY)
		self.rescan_requested = False
		return ['Test file - %d - %s' % (self.dummy_counter, term) for i in range(10)]

	def search(self, term):
		self.cond.acquire()
		self.search_terms.append(term)
		self.cond.notify()
		self.cond.release()
	
	def rescan(self):
		self.rescan_requested = True

class VimAsync(object):
	"""
	Simulates vim-plugin-initiated communication using polling.

	Unfortunately Vim doesn't really support polling either.  Consequently this
	is a massive hack. For more details, see
	http://vim.wikia.com/wiki/Timer_to_execute_commands_periodically
	"""

	def __init__(self):
		self.running = False
		self.clear()
		self.saved_updatetime = int(vim.eval('&updatetime'))
	
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
			vim.command('noremap <silent> <buffer> <C-A> :python ""<CR>')
			self.running = True
	
	def stop(self):
		vim.command('set updatetime=%d' % (self.saved_updatetime))
		vim.command("au! CursorHold *")
		self.running = False
		self.clear()

	def from_vim(self):
		assert self.running
		self.callback(*self.callback_args)
		vim.command('call feedkeys("\\<C-A>")')

class VimFilePirate(object):
	"""
	Main object. Singleton (one per Vim process).
	"""
	def __init__(self):
		# The File Pirate buffer
		self.buf = None
		self.async = VimAsync()
		self.fp = FilePirateThread()
		self.fp.start()
		self.searching = False
		self.stored_vim_globals = {}
		self.search_start_time = 0
		self.reset()
	
	def reset(self):
		self.term = '' # search term
		self.selected = 0
		self.spinner_character = ' '
		self.spinner_position = 0

	def buffer_create(self):
		# Open the window
		vim.command('silent! topleft 1split FilePirate')

		for option in BUFFER_OPTIONS:
			vim.command('setlocal ' + option)

		assert 'FilePirate' in vim.current.buffer.name

		# Set up the window.
		self.buffer_register_keys()
		self.buf = vim.current.buffer

		self.draw_search_line()
		self.unlock_buffer()
		for idx in range(MAX_RESULTS):
			if len(self.buf) - 2 < idx:
				self.buf.append('')
		self.lock_buffer()
		vim.current.window.height = MAX_RESULTS + 1
		self.cursor_to_selected()
	
	def cursor_to_selected(self):
		vim.current.window.cursor = (2 + self.selected, 0)
	
	def buffer_register_keys(self):
		for key in NORMAL_KEYS:
			ascii = ord(key)
			vim.command('noremap <silent> <buffer> <Char-%d> :python filepirate_key(%d)<CR>' % (ascii, ascii))

		for keyname, cmd in SPECIAL_KEYS.items():
			vim.command('noremap <silent> <buffer> %s :python %s()<CR>' % (keyname, cmd))
	
	def search_poll(self):
		if self.searching is True:
			if self.fp.idle is True:
				self.spinner_character = ' '
				self.async.stop()
				self.searching = False
				self.draw_search_line()
				self.show_results(self.fp.results)
			else:
				self.advance_spinner()
	
	def advance_spinner(self):
		if time.time() - self.search_start_time > SPINNER_DELAY:
			self.spinner_character = SPINNER[self.spinner_position]
			self.spinner_position = (self.spinner_position + 1) % len(SPINNER)
			self.draw_search_line()
	
	def draw_search_line(self):
		self.unlock_buffer()
		self.buf[0] = self.spinner_character + PROMPT + self.term
		self.lock_buffer()

	def lock_buffer(self):
		vim.command('setlocal nomodifiable')
	
	def unlock_buffer(self):
		vim.command('setlocal modifiable')

	def show_results(self, results):
		self.unlock_buffer()
		for idx, result in enumerate(results):
			self.buf[idx + 1] = ' ' + result
		for idx in range(len(results), MAX_RESULTS):
			self.buf[idx + 1] = ''
		self.lock_buffer()

	def set_global_options(self):
		" Remember the previous global options settings, and set our ones. "
		for opt in GLOBAL_OPTIONS:
			self.stored_vim_globals[opt] = bool(vim.eval('&' + opt))
			setter = opt if GLOBAL_OPTIONS[opt] else 'no' + opt
			vim.command('set ' + setter)
	
	def reset_global_options(self):
		" Restore settings saved in set_global_options() "
		for opt in self.stored_vim_globals:
			setter = opt if self.stored_vim_globals[opt] else 'no' + opt
			vim.command('set ' + setter)

	# Public API
	def filepirate_open(self):
		" Open the window "
		self.reset()
		# Set up the buffer and bend vim to our will
		self.buffer_create()
		self.set_global_options()
	
	def filepirate_close(self):
		" Close the window and shut down "
		self.async.stop()
		self.reset_global_options()
		vim.command("close");
		vim.command("silent! bunload! #%d" % (self.buf.number))

	def filepirate_key(self, ascii):
		" User pressed a key in the File Pirate window. "
		self.search(self.term + chr(ascii))
	
	def search(self, term):
		" Start a File Pirate search for 'term' "
		if not self.searching:
			self.spinner_character = ' '
			self.search_start_time = time.time()
		self.term = term
		self.draw_search_line()
		self.searching = True
		self.async.start(self.search_poll)
		self.fp.search(self.term)
	
	def filepirate_accept(self):
		" Close the File Pirate window and switch to the selected file "
		filename = self.buf[self.selected + 1][1:]
		filename = filename.replace(' ', r'\ ')
		self.filepirate_close()
		vim.command('e %s' % (filename))

	def filepirate_cancel(self):
		" Close the File Pirate window without selecting a file "
		self.filepirate_close()
	
	def filepirate_up(self):
		" Move cursor up "
		if self.selected > 0:
			self.selected -= 1
		self.cursor_to_selected()
	
	def filepirate_down(self):
		" Move cursor down "
		if self.selected < MAX_RESULTS - 1:
			self.selected += 1
		self.cursor_to_selected()
	
	def filepirate_bs(self):
		" Backspace "
		if len(self.term) > 0:
			self.search(self.term[:-1])
	
	def filepirate_rescan(self):
		" Rescan the current directory "
		self.fp.rescan()
		if self.term:
			self.search(self.term)

# Singleton
vim_file_pirate = VimFilePirate()

# Exposed to Vim
filepirate_open     = vim_file_pirate.filepirate_open
filepirate_key      = vim_file_pirate.filepirate_key
filepirate_callback = vim_file_pirate.async.from_vim
filepirate_accept   = vim_file_pirate.filepirate_accept
filepirate_cancel   = vim_file_pirate.filepirate_cancel
filepirate_up       = vim_file_pirate.filepirate_up
filepirate_down     = vim_file_pirate.filepirate_down
filepirate_bs       = vim_file_pirate.filepirate_bs
filepirate_rescan   = vim_file_pirate.filepirate_rescan

