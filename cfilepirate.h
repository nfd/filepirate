bool fp_init(void);
bool fp_deinit(void);
void fp_filter(char **positive, char **negative);
bool fp_init_dir(char *dirname);
void fp_deinit_dir(void);

struct candidate {
	char *dirname;
	char *filename;
	int goodness;

	struct candidate *better, *worse;
};

struct candidate_list {
	struct candidate *best;
	struct candidate *worst;
};

struct candidate_list *fp_candidate_list_create(void);
void fp_candidate_list_destroy(struct candidate_list *list);
bool fp_get_candidates(char *buffer, int buffer_ptr, struct candidate_list *candidates);

