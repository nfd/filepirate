/* File Pirate. Command-Arrr! */
#define _BSD_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>
#include <stdint.h>   /* god these includes are boring */
#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>    /* some of them sound like someone with a bad throat */
#include <unistd.h>   /* and some of them sound like diseases */
#include <stdio.h>    /* stdio: a meet-up for randy programmers */
#include <errno.h>    /* someone should make everythingposix.h */
#include <string.h>   /* probably take 100 years to compile though */
#include <fnmatch.h>
#include <assert.h>

#include "cfilepirate.h"

/* Memory allocation attempts to increase by a power of 2 each time, 
 * stopping at MEM_MAX, which is 64 megabytes by default.
 */
#define MEM_MIN (1 * 1024 * 1024)
#define MEM_MAX (64 * 1024 * 1024)
#define ERROR printf
#define INFO  printf

/* Structure in the memory pool. All indices are relative to the start of the pool.
 * So to convert an index to a pointer: ptr = (uint8_t *)pool_start + index.
 * "index past" means 1 byte past the terminating null byte of the referenced string.
 *
 * Directory entry structure:
 *
 * Offset    Name        Desc
 * 4 bytes   dir_len     Length of directory name including null pointer.
 * 1+ bytes  dirname     Directory name, null terminated
 * 4 bytes   fn_len      Length of file name including null pointer.
 * 1+ bytes  filename    Base file name, null terminated
 * ...       ...         More filenames
 * 1 byte    0           Final extra nul indicates a new directory name
 * 1+ bytes  dirname     Directory name, null terminated
 */

#define DIRENT_HEADER_SIZE (sizeof(unsigned int) + sizeof(unsigned int))

struct memory_pool
{
	uint8_t *start;
	uintptr_t next;  // bump pointer
	uintptr_t size;
};

struct filepirate {
	char *root_dirname;
	uint8_t *files;               // When initialised, points to the main pool.
	uint8_t *files_end;
	struct memory_pool main_pool;
	char **positive_filter;
	char **negative_filter;
};

/* Memory pool functions */
static bool pool_init(struct memory_pool *pool)
{
	pool->start = malloc(MEM_MIN);
	if (!pool->start) {
		ERROR("pool_init malloc]\n");
		return false;
	}
	pool->size = MEM_MIN;
	pool->next = 4;    // some offset from 0 so we can use 0 return for errors.

	return true;
}

static bool pool_free(struct memory_pool *pool)
{
	free(pool->start);

	pool->start = NULL;
	pool->size = 0;
	pool->next = 0;

	return true;
}

static uintptr_t try_pool_alloc(struct memory_pool *pool, size_t size)
{
	uintptr_t pool_next_next = pool->next + size;

	if(pool_next_next > pool->size) {
		return 0;
	} else {
		uintptr_t mem = pool->next;
		pool->next = pool_next_next;
		assert(mem != 0);
		return mem;
	}
}

static void pool_expand(struct memory_pool *pool)
{
	size_t new = pool->size * 2;
	uint8_t *pool_new_start;

	if (new > MEM_MAX)
		return;

	pool_new_start = realloc(pool->start, new);

	if (pool_new_start == NULL) {
		// Original pointer still valid
		return;
	}

	pool->start = pool_new_start;
	pool->size  = new;
}

static uintptr_t pool_alloc(struct memory_pool *pool, size_t size)
{
	uintptr_t addr;

	addr = try_pool_alloc(pool, size);

	if (addr == 0) {
		pool_expand(pool);
		addr = try_pool_alloc(pool, size);
	}

	return addr;
}

static inline uintptr_t alloc(struct filepirate *fp, size_t size)
{
	return pool_alloc(&(fp->main_pool), size);
}

static inline uint8_t *get_ptr(struct filepirate *fp, uintptr_t index)
{
	return fp->main_pool.start + index;
}

static inline void write_uint (struct filepirate *fp, uintptr_t index, unsigned int val)
{
	*((unsigned int *)(get_ptr(fp, index))) = val;
}

static inline bool passes_filter(struct filepirate *fp, char *name)
{
	if (fp->positive_filter) {
		for (char **filter = fp->positive_filter; *filter; filter++) {
			if (fnmatch(*filter, name, 0) == 0) {
				return true;
			}
		}
		return false;
	}
	if (fp->negative_filter) {
		for (char **filter = fp->negative_filter; *filter; filter++) {
			if (fnmatch(*filter, name, 0) == 0) {
				return false;
			}
		}
	}
	return true;
}

/* Joke's on... erm, me -- this does not recurse! */
static uintptr_t fp_init_dir_recurse(struct filepirate *fp)
{
	/* Just walk it for now */
	uintptr_t first = 0;
	uintptr_t dirent_start = 0;
	uintptr_t tmp = 0;
	bool dir_written = false;
	FTSENT *node;

	first = fp->main_pool.next;

	char *cwd_only[] = {".", 0};

	FTS *tree = fts_open(cwd_only, FTS_NOCHDIR, 0);
	if (!tree) {
		ERROR("fts_open");
		return 0;
	}

	while ((node = fts_read(tree))) {
		if (node->fts_level > 0 && node->fts_name[0] == '.')
			fts_set(tree, node, FTS_SKIP);
		else if (node->fts_info & FTS_D) {
			/* Pre-order directory */
			dir_written = false;
		} else if (node->fts_info & FTS_DP) {
			//printf("post-order directory\n");
			dir_written = false;
		} else if ((node->fts_info & FTS_F) && passes_filter(fp, node->fts_name)) {
			if (dir_written == false) {
				FTSENT *parent = node->fts_parent;
				if (dirent_start) {
					/* Finish off the previous directory with an extra nul */
					//printf("end previous directory\n");
					tmp = alloc(fp, sizeof(unsigned int) + 1);
					write_uint(fp, tmp, 0);
					tmp += sizeof(unsigned int);
					*((char *)get_ptr(fp, tmp)) = '\0';
				}
				//printf("write dirent %lx %s (%x) %s\n", fp.main_pool.next, parent->fts_path, parent->fts_pathlen, node->fts_path);
				dirent_start = alloc(fp, sizeof(unsigned int) + parent->fts_pathlen + 1);
				write_uint(fp, dirent_start, parent->fts_pathlen + 1);
				dirent_start += sizeof(unsigned int);
				strncpy((char *)get_ptr(fp, dirent_start), parent->fts_path, parent->fts_pathlen);
				*((char *)(get_ptr(fp, dirent_start) + parent->fts_pathlen)) = '\0';

				dir_written = true;
			}

			//printf("file %lx %s\n", fp.main_pool.next, node->fts_name);
			tmp = alloc(fp, sizeof(unsigned int) + node->fts_namelen + 1);
			write_uint(fp, tmp, node->fts_namelen + 1);
			tmp += sizeof(unsigned int);
			strcpy((char *)get_ptr(fp, tmp), node->fts_name);
		}
	}

	if (errno) {
		ERROR("fts_read");
		return 0;
	}

	if (fts_close(tree)) {
		ERROR("fts_close");
		return 0;
	}

	// printf("total size 0x%lx, start at %lx\n", fp.main_pool.next, first);

	return first;
}

static bool fp_init_dir(struct filepirate *fp, char *dirname)
{
	int cwd;
	uintptr_t files_index;

	assert(fp->root_dirname == NULL);
	fp->root_dirname = malloc(strlen(dirname) + 1);
	strcpy(fp->root_dirname, dirname);

	/* Remember the CWD */
	cwd = open(".", O_RDONLY);
	assert(cwd >= 0);

	/* Do all our work in the target dir's root */
	chdir(dirname);
	files_index = fp_init_dir_recurse(fp);

	/* Restore the previous CWD */
	fchdir(cwd);
	close(cwd);

	// Lock the pointers -- now we can't do more allocation using the main pool (in case we realloc and move the pointer)
	fp->files = fp->main_pool.start + files_index;
	fp->files_end = fp->files + fp->main_pool.next - files_index;

	return files_index != 0;
}

static void fp_deinit_dir(struct filepirate *fp)
{
	if(fp->root_dirname) {
		free(fp->root_dirname);
		fp->root_dirname = NULL;
	}
}

/* Non-contiguous matching across two strings (directory name and file name) */
/* Note the lengths are strlen() lengths, i.e. don't include the terminating
 * null, unlike the lengths stored in the directory structure in memory. */

// We search backwards in order to match as much on filename as possible
// before dirname, because filename is more important
static inline bool fp_strstr(unsigned int dirname_len, char *dirname,
		unsigned int filename_len, char *filename,
		unsigned int needle_len, char *needle,
		int *goodness)
{
	int idx_hay = filename_len, idx_needle = needle_len, contig = 0, contig_filename = 0;
	char *hay = filename;
	int last_match_idx = idx_hay;

	while (true) {
		if (idx_hay == -1) {
			if (hay == filename) {
				hay = dirname;
				idx_hay = dirname_len;
				last_match_idx = -1;

				contig_filename = contig;
				contig = 0;
			} else {
				/* End of filename -- we're done */
				break;
			}
		}

		if (idx_needle == -1) {
			/* Matched it all? */
			break;
		}

		if (hay[idx_hay] == needle[idx_needle]) {
			idx_needle --;

			if (idx_hay + 1 == last_match_idx)
				contig ++;

			last_match_idx = idx_hay;
		}

		idx_hay --;
	}

	if (goodness) {
		*goodness = contig_filename + contig;
	}

	return idx_needle == -1;
}

struct candidate_list *fp_candidate_list_create(int max_candidates)
{
	int i;
	struct candidate_list *list;

	list = malloc(sizeof(*list));
	if (!list)
		return NULL;
	list->best = list->worst = NULL;
	list->max_candidates = max_candidates;

	for (i = 0; i < max_candidates; i++) {
		struct candidate *new_candidate = malloc(sizeof(struct candidate));
		if (!new_candidate)
			return NULL; // FIXME: clean up memory leak
		new_candidate->goodness = -1;

		if (list->worst == NULL) {
			assert(list->best == NULL);
			list->best = list->worst = new_candidate;
			new_candidate->better = new_candidate->worse = NULL;
		} else {
			list->worst->worse = new_candidate;
			new_candidate->better = list->worst;
			new_candidate->worse = NULL;
			list->worst = new_candidate;
		}
	}

	return list;
}

static void candidate_list_reset(struct candidate_list *list)
{
	for (struct candidate *iter = list->best; iter; iter = iter->worse) {
		// Only for debugging
		// iter->dirname = iter->filename = NULL;
		iter->goodness = -1;
	}
}

void fp_candidate_list_destroy(struct candidate_list *list)
{
	while(list->best) {
		struct candidate *tmp = list->best->worse;
		free(list->best);
		list->best = tmp;
	}

	free(list);
}

static void candidate_list_add(struct candidate_list *list, char *dirname, char *filename, int goodness)
{
	if(goodness >= list->worst->goodness) {
		/* FIXME: Binary search better */
		struct candidate *new_candidate = list->worst;
		list->worst->better->worse = NULL;
		list->worst = list->worst->better;
		new_candidate->dirname = dirname;
		new_candidate->filename = filename;
		new_candidate->goodness = goodness;

		for (struct candidate *iter = list->best; iter; iter = iter->worse) {
			if (goodness >= iter->goodness) {
				if (iter == list->best) {
					/* Insert before "iter", at head of list */
					new_candidate->worse = list->best;
					new_candidate->better = NULL;
					list->best->better = new_candidate;
					list->best = new_candidate;
				} else {
					/* Insert before "iter":
					 * A <new> <iter>
					 * New's worse -> A's worse
					 * New's better -> iter's better
					 * A's worse -> new
					 * iter's better -> new*/
					struct candidate *better = iter->better;
					new_candidate->worse = better->worse;
					new_candidate->better = iter->better;
					better->worse = new_candidate;
					iter->better = new_candidate;
				}
				break;
			}
		}
	}
}

bool fp_get_candidates(struct filepirate *fp, char *buffer, int buffer_ptr, struct candidate_list *candidates)
{
	char *files;
	int file_count = 0;
	int previous_best_goodness = -1;
	bool new_directory = true; // Was a new directory entered?
	unsigned int dirname_len, filename_len;
	char *dirname = NULL;

	new_directory = true;
	files = (char *)fp->files;
	candidate_list_reset(candidates);

	for(file_count = 0; file_count < candidates->max_candidates && files < (char *)fp->files_end; ) {
		if (new_directory) {
			dirname_len = *(unsigned int *)files;
			files += sizeof(unsigned int);
			dirname = files;
			files += dirname_len;
			new_directory = false;
		}

		filename_len = *(unsigned int *)files;
		files += sizeof(unsigned int);
		if (filename_len == 0) {
			// End of this directory 
			files += 1;
			new_directory = true;
		} else {
			int goodness;
			if (fp_strstr(dirname_len - 1, dirname, filename_len - 1, files, buffer_ptr - 1, buffer, &goodness) == true) {
				if(goodness >= previous_best_goodness) {
					candidate_list_add(candidates, dirname, files, goodness);
					previous_best_goodness = goodness;
				}
			}
			files += filename_len;
		}
	}
	return true;
}


/* FIXME: Support directory filtering as well. Dodgy option: include both.
 * Less dodgy option: include custom fnmatch to take two-string arg as per
 * strstr. */

struct filepirate *fp_init(char *dirname)
{
	struct filepirate *fp;

	fp = calloc(sizeof *fp, 1);
	if (fp == NULL)
		return NULL;

	fp->positive_filter = fp->negative_filter = NULL;
	if (pool_init(&(fp->main_pool)) == false) {
		free(fp);
		return NULL;
	}

	if (fp_init_dir(fp, dirname) == false) {
		fp_deinit(fp);
		return NULL;
	}

	return fp;
}

bool fp_deinit(struct filepirate *fp)
{
	fp_deinit_dir(fp);

	if (pool_free(&fp->main_pool)) {
		free (fp);
		return true;
	}

	return false;
}

void fp_filter(struct filepirate *fp, char **positive, char **negative)
{
	fp->positive_filter = positive;
	fp->negative_filter = negative;
}

