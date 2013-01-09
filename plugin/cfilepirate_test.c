#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <termios.h>
#include <stdlib.h>
#include <string.h>

#include "cfilepirate.h"

#define ERROR printf
#define INFO  printf

/* Interactive mode requires mucking around with the terminal */
bool term_modified;
int term_fd;
struct termios term_settings;

char *positive_filter[] = {
	"*.c",
	"*.h",
	"*.cpp",
	"*.cxx",
	NULL};

char *negative_filter[] = {
	"*.o",
	"*.ppm",
	NULL
};


static void term_reset(void)
{
	if (term_modified) {
		tcsetattr(term_fd, TCSADRAIN, &term_settings);
	}
}

static void term_init(void)
{
	struct termios new_settings;
	term_fd = 0;

	tcgetattr(term_fd, &term_settings);
	new_settings = term_settings;

	new_settings.c_lflag &= ~ICANON;
	new_settings.c_lflag &= ~ECHO;
	new_settings.c_cc[VMIN] = 1;
	new_settings.c_cc[VTIME] = 0;
	if (tcsetattr(term_fd, TCSANOW, &new_settings)) {
		ERROR("term_init");
	}

	term_modified = true;
}

static void candidate_list_dump(struct candidate_list *list)
{
	for (struct candidate *iter = list->best; iter; iter = iter->worse) {
		if (iter->goodness > -1)
			printf("%s/%s (%d)\n", iter->dirname, iter->filename, iter->goodness);
	}
}

static void filepirate_interactive_test(struct filepirate *fp)
{
	char buffer[80] = {0};
	int buffer_ptr = 0;
	unsigned int c;
	struct candidate_list *candidates;

	candidates = fp_candidate_list_create(20);

	while (true) {
		fp_get_candidates(fp, buffer, buffer_ptr, candidates);

		candidate_list_dump(candidates);
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

	fp_candidate_list_destroy(candidates);
}

static void filepirate_search_once(struct filepirate *fp, char *term)
{
	struct candidate_list *candidates;

	candidates = fp_candidate_list_create(10);
	fp_get_candidates(fp, term, strlen(term) + 1, candidates);
	candidate_list_dump(candidates);
	fp_candidate_list_destroy(candidates);
}


int main(int argc, char **argv)
{
	struct filepirate *fp;
	bool interactive = true;

	if (argc == 3)
		interactive = false;
	else
		assert(argc == 2);

	if ((fp = fp_init(argv[1])) == NULL) {
		ERROR("pool init main");
		return 1;
	}

	if (interactive) {
		term_modified = false;
		atexit(term_reset);
		term_init();
	}

	fp_filter(fp, positive_filter, negative_filter);

	if (argc == 2)
		filepirate_interactive_test(fp);
	else
		filepirate_search_once(fp, argv[2]);
	fp_deinit(fp);
}

