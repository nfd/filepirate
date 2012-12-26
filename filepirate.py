"""
:arrr
E492: Not an editor command: arrr
:q!

"""
import os
import sys
import ctypes

SONAME = 'cfilepirate.so'

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
	def __init__(self, root, max_candidates):
		self.root = root
		self.native = ctypes.CDLL(SONAME)

		for export in PROTOTYPES:
			restype, argtypes = PROTOTYPES[export]
			obj = getattr(self.native, export)
			obj.restype = restype
			obj.argtypes = argtypes

		self.handle = self.native.fp_init(self.root)
		if bool(self.handle) == False: # ctypes-speak for handle == NULL
			raise Error("fp_init")
		
		self.max_candidates = max_candidates
		self.candidates = self.native.fp_candidate_list_create(max_candidates)
		if self.candidates == None:
			raise Error("fp_candidate_list_create")
	
	def __del__(self):
		self.native.fp_candidate_list_destroy(self.candidates)
		self.native.fp_deinit(self.handle)

	def get_candidates(self, search_term):
		result = self.native.fp_get_candidates(self.handle, search_term, len(search_term), self.candidates)
		if not result:
			raise Error("fp_get_candidates")

		candidates = []
		candidate = self.candidates.contents.best
		while bool(candidate): # magic: bool(ptr) = false when ptr is null
			candidate = candidate.contents
			candidates.append(os.path.join(candidate.dirname, candidate.filename))
			candidate = candidate.worse

		return candidates
		
if __name__ == '__main__':
	dirname = sys.argv[1]
	searchterm = sys.argv[2]
	fp = FilePirate(dirname, 10)
	print fp.get_candidates(searchterm)

