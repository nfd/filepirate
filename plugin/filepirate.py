"""
Python generic filepirate interface
"""
import os
import sys
import ctypes
import time

SONAME = os.path.abspath(os.path.join(os.path.dirname(__file__), 'cfilepirate.so'))
MAX_PIRATES = 5

class Candidate(ctypes.Structure):
	# Forward declaration because of recursive type
	pass

Candidate._fields_ = [('dirname', ctypes.c_char_p),
			('filename', ctypes.c_char_p),
			('goodness', ctypes.c_int),
			('better', ctypes.POINTER(Candidate)),
			('worse', ctypes.POINTER(Candidate))]

class CandidateList(ctypes.Structure):
	_fields_ = [('best', ctypes.POINTER(Candidate)),
		('worst', ctypes.POINTER(Candidate)),
		('max_candidates', ctypes.c_int)]

PROTOTYPES = {'fp_init': (ctypes.c_void_p, [ctypes.c_char_p]),
		'fp_deinit': (ctypes.c_bool, [ctypes.c_void_p]),
		'fp_candidate_list_create': (ctypes.POINTER(CandidateList), [ctypes.c_int]),
		'fp_candidate_list_destroy': (None, [ctypes.c_void_p]),
		'fp_get_candidates': (ctypes.c_bool, [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_void_p])
}

class Error(Exception):
	pass

class FilePirate(object):
	"""
	Interface to native code
	"""
	# Class static
	native = None

	def __init__(self, root, max_candidates):
		self.root = root

		if self.__class__.native is None:
			self.__class__.native = ctypes.CDLL(SONAME)

			for export in PROTOTYPES:
				restype, argtypes = PROTOTYPES[export]
				obj = getattr(self.__class__.native, export)
				obj.restype = restype
				obj.argtypes = argtypes

		self.max_candidates = max_candidates
		self.create()
	
	def __del__(self):
		if self.native:
			if hasattr(self, 'candidates'):
				self.native.fp_candidate_list_destroy(self.candidates)
			if hasattr(self, 'handle'):
				self.native.fp_deinit(self.handle)
	
	def rescan(self):
		self.__del__()
		self.create()
	
	def create(self):
		self.handle = self.native.fp_init(self.root.encode('utf-8'))
		if bool(self.handle) == False: # ctypes-speak for handle == NULL
			raise Error("fp_init")

		self.candidates = self.native.fp_candidate_list_create(self.max_candidates)
		if self.candidates == None:
			raise Error("fp_candidate_list_create")

	def get_candidates(self, search_term):
		result = self.native.fp_get_candidates(self.handle, search_term.encode('utf-8'), len(search_term), self.candidates)
		if not result:
			raise Error("fp_get_candidates")

		candidates = []
		candidate = self.candidates.contents.best
		while bool(candidate): # magic: bool(ptr) = false when ptr is null
			candidate = candidate.contents
			if candidate.goodness == -1:
				# That's it
				break
			candidates.append(os.path.join(candidate.dirname.decode('utf-8'), candidate.filename.decode('utf-8')))
			candidate = candidate.worse

		return candidates

class FilePirates(object):
	"""
	A set of FilePirate objects. Keeps only MAX_PIRATES in memory. Eviction is LRU.
	"""
	def __init__(self, max_candidates):
		self.pirates = []
		self.max_candidates = max_candidates

	def get(self, root):
		for idx in range(len(self.pirates)):
			if self.pirates[idx].root == root:
				pirate = self.pirates[idx]
				self.pirates.pop(idx)
				break
		else:
			if len(self.pirates) >= MAX_PIRATES:
				self.pirates.pop()
			pirate = FilePirate(root, self.max_candidates)

		self.pirates.insert(0, pirate)
		return pirate

if __name__ == '__main__':
	# test it
	dirname = sys.argv[1]
	searchterm = sys.argv[2]
	fp = FilePirate(dirname, 10)
	print (fp.get_candidates(searchterm))
	t = time.time()
	print (fp.get_candidates(searchterm))
	print (time.time() - t)

