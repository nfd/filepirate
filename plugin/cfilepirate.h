struct filepirate;

struct filepirate *fp_init();
bool fp_init_dir(struct filepirate *fp, char *dirname);
bool fp_deinit(struct filepirate *fp);
void fp_filter_add_positive(struct filepirate *fp, char *positive);
void fp_filter_add_negative(struct filepirate *fp, char *negative);
void fp_filter(struct filepirate *fp, char **positive, char **negative);

struct candidate {
	char *dirname;
	char *filename;
	int goodness;

	struct candidate *better, *worse;
};

struct candidate_list {
	struct candidate *best;
	struct candidate *worst;
	int max_candidates;
};

struct candidate_list *fp_candidate_list_create(int max_candidates);
void fp_candidate_list_destroy(struct candidate_list *list);
bool fp_get_candidates(struct filepirate *fp, char *buffer, int buffer_ptr, struct candidate_list *candidates);

