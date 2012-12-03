/* File Pirate. Command-Arrr! */
#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>
#include <assert.h>
#include <stdint.h>   /* god these includes are boring */
#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>    /* some of them sound like someone with a bad throat */
#include <unistd.h>   /* and some of them sound like diseases */
#include <stdio.h>    /* stdio: a meet-up for randy programmers */
#include <errno.h>    /* someone should make everythingposix.h */
#include <string.h>   /* probably take 100 years to compile though */
#include <termios.h>

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

static struct {
	char *root_dirname;
	uint8_t *files;               // When initialised, points to the main pool.
	uint8_t *files_end;
	struct memory_pool main_pool;
	struct memory_pool tmp_pool;

	bool term_modified;
	int term_fd;
	struct termios term_settings;
} fp;

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

static inline uintptr_t alloc(size_t size)
{
	return pool_alloc(&fp.main_pool, size);
}

static inline uint8_t *get_ptr(uintptr_t index)
{
	return fp.main_pool.start + index;
}

static inline void write_uint (uintptr_t index, unsigned int val)
{
	*((unsigned int *)(get_ptr(index))) = val;
}

static inline uintptr_t alloc_tmp(size_t size)
{
	return pool_alloc(&fp.tmp_pool, size);
}


static uintptr_t fp_init_dir_recurse(void)
{
	/* Just walk it for now */
	uintptr_t first = 0;
	uintptr_t dirent_start = 0;
	uintptr_t tmp = 0;

	first = fp.main_pool.next;

	char *cwd_only[] = {".", 0};

	FTS *tree = fts_open(cwd_only, FTS_NOCHDIR, 0);
	if (!tree) {
		ERROR("fts_open");
		return 0;
	}

	FTSENT *node;
	while ((node = fts_read(tree))) {
		if (node->fts_level > 0 && node->fts_name[0] == '.')
			fts_set(tree, node, FTS_SKIP);
		else if (node->fts_info & FTS_D) {
			/* Pre-order directory */
			if (dirent_start) {
				/* Finish off the previous directory with an extra nul */
				//printf("Finish %lx\n", fp.main_pool.next);
				tmp = alloc(sizeof(unsigned int) + 1);
				write_uint(tmp, 0);
				tmp += sizeof(unsigned int);
				*((char *)get_ptr(tmp)) = '\0';
			}
			//printf("dirent %lx\n", fp.main_pool.next);
			dirent_start = alloc(sizeof(unsigned int) + node->fts_pathlen + 1);
			write_uint(dirent_start, node->fts_pathlen + 1);
			dirent_start += sizeof(unsigned int);
			strcpy((char *)get_ptr(dirent_start), node->fts_path);

		} else if (node->fts_info & FTS_F) {
			//printf("file %lx\n", fp.main_pool.next);
			tmp = alloc(sizeof(unsigned int) + node->fts_namelen + 1);
			write_uint(tmp, node->fts_namelen + 1);
			tmp += sizeof(unsigned int);
			strcpy((char *)get_ptr(tmp), node->fts_name);
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

	printf("total size 0x%lx, start at %lx\n", fp.main_pool.next, first);

	return first;
}

static bool fp_init_dir(char *dirname)
{
	int cwd;
	uintptr_t files_index;

	assert(fp.root_dirname == NULL);
	fp.root_dirname = malloc(strlen(dirname) + 1);
	strcpy(fp.root_dirname, dirname);

	/* Remember the CWD */
	cwd = open(".", O_RDONLY);
	assert(cwd >= 0);

	/* Do all our work in the target dir's root */
	chdir(dirname);
	files_index = fp_init_dir_recurse();

	/* Restore the previous CWD */
	fchdir(cwd);
	close(cwd);

	// Lock the pointers -- now we can't do more allocation using the main pool (in case we realloc and move the pointer)
	fp.files = fp.main_pool.start + files_index;
	fp.files_end = fp.files + fp.main_pool.next;

	return files_index != 0;
}

static void term_reset(void)
{
	if (fp.term_modified) {
		tcsetattr(fp.term_fd, TCSADRAIN, &fp.term_settings);
	}
}

static void term_init(void)
{
	struct termios new_settings;
	fp.term_fd = 0;

	tcgetattr(fp.term_fd, &fp.term_settings);
	new_settings = fp.term_settings;

	new_settings.c_lflag &= ~ICANON;
	new_settings.c_lflag &= ~ECHO;
	new_settings.c_cc[VMIN] = 1;
	new_settings.c_cc[VTIME] = 0;
	if (tcsetattr(fp.term_fd, TCSANOW, &new_settings)) {
		ERROR("term_init");
	}

	fp.term_modified = true;
}

/* Non-contiguous matching across two strings (directory name and file name) */
static inline bool fp_strstr(char *dirname, char *filename, char *needle, int *num_contiguous)
{
	int idx_hay = 0, idx_needle = 0, contig = 0;
	char *hay = dirname;
	int last_match_idx;

	while (true) {
		if (hay[idx_hay] == '\0') {
			if (hay == dirname) {
				hay = filename;
				idx_hay = 0;
				last_match_idx = 0;
			} else {
				/* End of filename -- we're done */
				break;
			}
		}

		if (needle[idx_needle] == '\0') {
			/* Matched it all? */
			break;
		}

		if (hay[idx_hay] == needle[idx_needle]) {
			idx_needle ++;

			if (idx_hay - 1 == last_match_idx)
				contig ++;

			last_match_idx = idx_hay;
		}

		idx_hay ++;
	}

	if (num_contiguous)
		*num_contiguous = contig;

	return needle[idx_needle] == '\0';
}

static void filepirate_interactive_test(void)
{
	char buffer[80] = {0};
	int buffer_ptr = 0;
	char *files;
	int file_count = 0;
	bool new_directory = true; // Was a new directory entered?
	unsigned int str_len;
	char *dirname = NULL;

	{
		FILE *h;
		h = fopen("mem.txt", "w");
		fwrite(fp.files, 1, 2048, h);
		fclose(h);
	}

	while (true) {
		unsigned int c;
		int previous_best_contig = -1;

		new_directory = true;
		files = (char *)fp.files;
		//printf("files %p (pool %p)\n", files, fp.main_pool.start);

		for(file_count = 0; file_count < 20 && files < (char *)fp.files_end; ) {
			if (new_directory) {
				//printf("read directory %lx\n", files - (char *)fp.main_pool.start);
				str_len = *(unsigned int *)files;
				assert(str_len < 1000);
				files += sizeof(unsigned int);
				//printf("directory name %s\n", files);
				dirname = files;
				files += str_len;
				//printf("files pointer moved to %lx\n", files - (char *)fp.main_pool.start);
				new_directory = false;
			}

			str_len = *(unsigned int *)files;
			files += sizeof(unsigned int);
			//printf("str len for files '%s' %d\n", files, str_len);
			if (str_len == 0) {
				// End of this directory 
				//printf("new directory\n");
				files += 1;
				new_directory = true;
			} else {
				int contig;
				if (fp_strstr(dirname, files, buffer, &contig) == true) {
					if(contig >= previous_best_contig) {
						file_count += 1;
						printf("%s/%s %d\n", dirname, files, contig);
						previous_best_contig = contig;
					}
				}
				files += str_len;
			}

		}
		printf("\n> %s", buffer);
		fflush(stdout);

		c = getchar();
		printf("\n");

		if(c == '\n' || c == '\r')
			break;
		else if (c == 127) {
			if (buffer_ptr > 0) {
				buffer_ptr --;
				buffer[buffer_ptr] = '\0';
			}
		} else {
			buffer[buffer_ptr ++] = c;
			buffer[buffer_ptr] = '\0';
		}
		
	}
}

int main(int argc, char **argv)
{
	assert(argc == 2);

	if (pool_init(&fp.main_pool) == false) {
		ERROR("pool init main");
		return 1;
	}

	if (pool_init(&fp.tmp_pool) == false) {
		ERROR("pool init tmp");
		return 1;
	}

	fp.term_modified = false;
	atexit(term_reset);
	term_init();

	fp_init_dir(argv[1]);
	filepirate_interactive_test();

	pool_free(&fp.main_pool);
	pool_free(&fp.tmp_pool);

}

