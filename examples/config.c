/*
 * libgit2 "config" example - shows how to walk history and get commit info
 *
 * Written by the libgit2 contributors
 *
 * To the extent possible under law, the author(s) have dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 *
 * You should have received a copy of the CC0 Public Domain Dedication along
 * with this software. If not, see
 * <http://creativecommons.org/publicdomain/zero/1.0/>.
 */

#include "common.h"

/**
 * The following example partially reimplements the `git config` command
 * and some of its options.
 *
 * These commands should work:
 */
 
 typedef enum {
	VAR_TYPE_BOOL = 1,
	VAR_TYPE_INT = 2,
	VAR_TYPE_BOOLORINT = 3,
	VAR_TYPE_PATH = 4,
} var_type_t;
 
/** config_options represents the parsed command line options */
typedef struct {
	git_config_level_t level;
	const char *file;
	var_type_t var_type;
	int null;
	int def;
	int add;
	int replace_all;
	int get;
	int get_all;
	int get_regexp;
	int get_urlmatch;
	int unset;
	int unset_all;
	int rename_section;
	int remove_section;
	int list;
	const char *p1;
	const char *p2;
	const char *p3;
} config_options;

static int config_set(config_options *opt, git_config *config)
{
	char sbuf[32];
	const char *svalue = NULL;
	const char *regex = NULL;

	if (opt->var_type == VAR_TYPE_BOOL) {
		int value;
		check_lg2(git_config_parse_bool(&value, opt->p2), "git_config_parse_bool", opt->p1);
		svalue = value ? "true" : "false";
	} else if (opt->var_type == VAR_TYPE_INT) {
		int64_t value;
		check_lg2(git_config_parse_int64(&value, opt->p2), "git_config_parse_int64", opt->p1);
		sprintf(sbuf, "%" PRId64, value);
		svalue = sbuf;
	} else if (opt->var_type == VAR_TYPE_BOOLORINT) {
		int64_t ivalue;
		int bvalue;
		int iresult, bresult;
		iresult = git_config_parse_int64(&ivalue, opt->p2);
		if (iresult == 0) {
			sprintf(sbuf, "%" PRId64, ivalue);
			svalue = sbuf;
		} else {
			bresult = git_config_parse_bool(&bvalue, opt->p2);
			if (bresult == 0)
				svalue = bvalue ? "true" : "false";
			else
				check_lg2(iresult, "git_config_parse_int64", opt->p1);
		}
	} else {
		svalue = opt->p2;
	}

	if (opt->add)
		regex = "^$";
	else if (opt->replace_all && opt->p3 == NULL)
		regex = "$";
	else
		regex = opt->p3;
	if (regex != NULL)
		check_lg2(git_config_set_multivar(config, opt->p1, regex, svalue), "git_config_set_multivar", opt->p1);
	else
		check_lg2(git_config_set_string(config, opt->p1, svalue), "git_config_set_string", opt->p1);
	return 0;
}

static int config_get(config_options *opt, git_config *config)
{
	if (opt->var_type == VAR_TYPE_BOOL) {
		int value;
		int result = git_config_get_bool(&value, config, opt->p1);
		if (result == GIT_ENOTFOUND)
			return 1;
		if (result == 0) {
			printf("%s\n", value ? "true" : "false");
			return 0;
		}
		check_lg2(result, "git_config_get_bool", opt->p1);
	} else if (opt->var_type == VAR_TYPE_INT) {
		int64_t value;
		int result = git_config_get_int64(&value, config, opt->p1);
		if (result == GIT_ENOTFOUND)
			return 1;
		if (result == 0) {
			printf("%" PRId64 "\n", value);
			return 0;
		}
		check_lg2(result, "git_config_get_int64", opt->p1);
	} else if (opt->var_type == VAR_TYPE_BOOLORINT) {
		int64_t ivalue;
		int bvalue;
		int iresult, bresult;
		iresult = git_config_get_int64(&ivalue, config, opt->p1);
		if (iresult == GIT_ENOTFOUND)
			return 1;
		if (iresult == 0) {
			printf("%" PRId64 "\n", ivalue);
			return 0;
		}
		bresult = git_config_get_bool(&bvalue, config, opt->p1);
		if (bresult == GIT_ENOTFOUND)
			return 1;
		if (bresult == 0) {
			printf("%s\n", bvalue ? "true" : "false");
			return 0;
		}
		check_lg2(iresult, "git_config_get_int64", opt->p1);
		check_lg2(bresult, "git_config_get_bool", opt->p1);
	} else {
		const char *value = NULL;
		int result = git_config_get_string(&value, config, opt->p1);
		if (result == GIT_ENOTFOUND)
			return 1;
		if (result == 0) {
			if (opt->var_type == VAR_TYPE_PATH) {
				git_buf out = { 0 };
				result = git_config_parse_path(&out, value);
				if (result == 0) {
					printf("%s\n", out);
					return 0;
				}
				check_lg2(result, "git_config_parse_path", opt->p1);
			}

			printf("%s\n", value);
			return 0;
		}
		check_lg2(result, "git_config_get_string", opt->p1);
	}

	return 0;
}

static int config_get_all_cb(const git_config_entry *entry, void *payload)
{
	config_options *opt = (config_options *)payload;

	if (opt->var_type == VAR_TYPE_BOOL) {
		int value;
		check_lg2(git_config_parse_bool(&value, entry->value), "git_config_parse_bool", opt->p1);
		printf("%s\n", value ? "true" : "false");
		return 0;
	} else if (opt->var_type == VAR_TYPE_INT) {
		int64_t value;
		check_lg2(git_config_parse_int64(&value, entry->value), "git_config_parse_int64", opt->p1);
		printf("%" PRId64 "\n", value);
		return 0;
	} else if (opt->var_type == VAR_TYPE_BOOLORINT) {
		int64_t ivalue;
		int bvalue;
		int iresult, bresult;
		iresult = git_config_parse_int64(&ivalue, entry->value);
		if (iresult == 0) {
			printf("%" PRId64 "\n", ivalue);
			return 0;
		}
		bresult = git_config_parse_bool(&bvalue, entry->value);
		if (bresult == 0) {
			printf("%s\n", bvalue ? "true" : "false");
			return 0;
		}
		check_lg2(iresult, "git_config_parse_int64", opt->p1);
		check_lg2(bresult, "git_config_parse_bool", opt->p1);
	} else {
		if (opt->var_type == VAR_TYPE_PATH) {
			git_buf out = { 0 };
			check_lg2(git_config_parse_path(&out, entry->value), "git_config_parse_path", opt->p1);
			printf("%s\n", out);
			return 0;
		}

		printf("%s\n", entry->value);
		return 0;
	}

	return 0;
}

static int config_get_all(config_options *opt, git_config *config)
{
	return git_config_get_multivar_foreach(config, opt->p1, opt->p2, config_get_all_cb, opt);
}

static void prepare_config(config_options *opt, git_config **config)
{
	git_config *fconfig = NULL;
	git_repository *repo = NULL;

	if (opt->level == GIT_CONFIG_LEVEL_SYSTEM) {
		git_buf config_file = { 0 };
		check_lg2(git_config_find_system(&config_file), "git_config_find_system", "");
		check_lg2(git_config_new(&fconfig), "git_config_new", "");
		check_lg2(git_config_add_file_ondisk(fconfig, config_file.ptr, GIT_CONFIG_LEVEL_SYSTEM, 1), "git_config_add_file_ondisk", config_file.ptr);
	}
	else if (opt->level == GIT_CONFIG_LEVEL_GLOBAL) {
		git_buf config_file = { 0 };
		int result;
		check_lg2(git_config_find_global(&config_file), "git_config_find_global", "");
		check_lg2(git_config_new(&fconfig), "git_config_new", "");
		check_lg2(git_config_add_file_ondisk(fconfig, config_file.ptr, GIT_CONFIG_LEVEL_GLOBAL, 1), "git_config_add_file_ondisk", config_file.ptr);

		result = git_config_find_xdg(&config_file);
		if (result == 0) {
			check_lg2(git_config_new(&fconfig), "git_config_new", "");
			check_lg2(git_config_add_file_ondisk(fconfig, config_file.ptr, GIT_CONFIG_LEVEL_GLOBAL, 1), "git_config_add_file_ondisk", config_file.ptr);
		}
		else if (result != GIT_ENOTFOUND)
			check_lg2(result, "git_config_find_xdg", "");
	}
	else if (opt->level == GIT_CONFIG_LEVEL_APP) {
		check_lg2(git_config_new(&fconfig), "git_config_new", "");
		check_lg2(git_config_add_file_ondisk(fconfig, opt->file, GIT_CONFIG_LEVEL_GLOBAL, 1), "git_config_add_file_ondisk", opt->file);
	}
	else if (opt->level == GIT_CONFIG_LEVEL_LOCAL) {
		git_buf root = { 0 };
		char config_file[GIT_PATH_MAX];
		check_lg2(git_repository_discover(&root, ".", 1, NULL), "git_repository_discover", "");
		check_lg2(git_repository_open(&repo, root.ptr), "git_repository_open", "");
		sprintf(config_file, "%s%sconfig", root.ptr, (root.ptr[root.size - 1] == '/') ? "" : "/");
		check_lg2(git_config_new(&fconfig), "git_config_new", "");
		check_lg2(git_config_add_file_ondisk(fconfig, config_file, GIT_CONFIG_LEVEL_GLOBAL, 1), "git_config_add_file_ondisk", config_file);
	}
	else {
		git_buf root = { 0 };
		int result = git_repository_discover(&root, ".", 1, NULL);
		if (result == 0) {
			check_lg2(git_repository_open(&repo, root.ptr), "git_repository_open", "");
			check_lg2(git_repository_config(&fconfig, repo), "git_repository_config", "");
		}
		else
			check_lg2(git_config_open_default(&fconfig), "git_config_open_default", "");
	}

	if ((opt->def && opt->p2 == NULL) || opt->get || opt->get_all || opt->get_regexp || opt->get_urlmatch || opt->list)
		check_lg2(git_config_snapshot(config, fconfig), "git_config_snapshot", "");
	else
		*config = fconfig;
}

static int do_config(config_options *opt)
{
	git_config *config = NULL;

	prepare_config(opt, &config);

	if (opt->def && opt->p2 != NULL || opt->add || opt->replace_all)
		return config_set(opt, config);
	if (opt->get || (opt->def && opt->p2 == NULL))
		return config_get(opt, config);
	if (opt->get_all)
		return config_get_all(opt, config);

	return 129;
}

/** Print a usage message for the program. */
static void usage(const char *message, const char *arg)
{
	if (message && arg)
		fprintf(stderr, "%s: %s\n", message, arg);
	else if (message)
		fprintf(stderr, "%s\n", message);
	fprintf(stderr, "usage: config [<options>]\n");
	exit(129);
}

static void wrong_arg_count()
{
	usage("error", "wrong number of arguments");
}

/** Parse some config command line options. */
static int parse_options(config_options *opt, int argc, char **argv)
{
	struct args_info args = ARGS_INFO_INIT;
	int argp = 0;

	memset(opt, 0, sizeof(*opt));

	args.pos = 1;
	// config file location
	if (argc > args.pos) {
		const char *a = argv[args.pos];

		if (!strcmp(a, "--local")) {
			opt->level = GIT_CONFIG_LEVEL_LOCAL;
			args.pos++;
		} else if (!strcmp(a, "--global")) {
			opt->level = GIT_CONFIG_LEVEL_GLOBAL;
			args.pos++;
		} else if (!strcmp(a, "--system")) {
			opt->level = GIT_CONFIG_LEVEL_SYSTEM;
			args.pos++;
		} else if (match_str_arg(&opt->file, &args, "-f") || match_str_arg(&opt->file, &args, "--file")) {
			opt->level = GIT_CONFIG_LEVEL_APP;
			args.pos++;
		}
	}

	// type
	if (argc > args.pos) {
		const char *a = argv[args.pos++];

		if (!strcmp(a, "--bool"))
			opt->var_type = VAR_TYPE_BOOL;
		else if (!strcmp(a, "--int"))
			opt->var_type = VAR_TYPE_INT;
		else if (!strcmp(a, "--bool-or-int"))
			opt->var_type = VAR_TYPE_BOOLORINT;
		else if (!strcmp(a, "--path"))
			opt->var_type = VAR_TYPE_PATH;
		else
			args.pos--;
	}

	// null
	if (argc > args.pos) {
		const char *a = argv[args.pos++];

		if (!strcmp(a, "-z") || !strcmp(a, "--null"))
			opt->null = 1;
		else
			args.pos--;
	}

	if (argc > args.pos) {
		const char *a = argv[args.pos++];

		if (!strcmp(a, "--add"))
			opt->add = 1;
		else if (!strcmp(a, "--replace-all"))
			opt->replace_all = 1;
		else if (!strcmp(a, "--get"))
			opt->get = 1;
		else if (!strcmp(a, "--get-all"))
			opt->get_all = 1;
		else if (!strcmp(a, "--get-regexp"))
			opt->get_regexp = 1;
		else if (!strcmp(a, "--get-urlmatch"))
			opt->get_urlmatch = 1;
		else if (!strcmp(a, "--unset"))
			opt->unset = 1;
		else if (!strcmp(a, "--unset-all"))
			opt->unset_all = 1;
		else if (!strcmp(a, "--rename-section"))
			opt->rename_section = 1;
		else if (!strcmp(a, "--remove-section"))
			opt->remove_section = 1;
		else if (!strcmp(a, "-l") || !strcmp(a, "--list"))
			opt->list = 1;
		else if (a[0] == '-')
			usage("Unsupported argument", a);
		else
		{
			opt->def = 1;
			args.pos--;
		}
	}

	for (; args.pos < argc; ++args.pos) {
		const char *a = argv[args.pos];

		argp++;
		if (argp == 1)
			opt->p1 = a;
		else if (argp == 2)
			opt->p2 = a;
		else if (argp == 3)
			opt->p3 = a;
	}
	if (opt->def && (argp != 1 && argp != 2 && argp != 3))
		wrong_arg_count();
	if (opt->add && argp != 2)
		wrong_arg_count();
	if (opt->replace_all && (argp != 2 && argp != 3))
		wrong_arg_count();
	if (opt->get && (argp != 1 && argp != 2))
		wrong_arg_count();
	if (opt->get_all && (argp != 1 && argp != 2))
		wrong_arg_count();
	if (opt->get_regexp && (argp != 1 && argp != 2))
		wrong_arg_count();
	if (opt->get_urlmatch && argp != 2)
		wrong_arg_count();
	if (opt->unset && (argp != 1 && argp != 2))
		wrong_arg_count();
	if (opt->unset_all && (argp != 1 && argp != 2))
		wrong_arg_count();
	if (opt->rename_section && argp != 2)
		wrong_arg_count();
	if (opt->remove_section && argp != 1)
		wrong_arg_count();
	if (opt->list && argp > 0)
		wrong_arg_count();

	return args.pos;
}

int main(int argc, char *argv[])
{
	config_options opt;
	int result;

	git_libgit2_init();

	parse_options(&opt, argc, argv);
	result = do_config(&opt);

	git_libgit2_shutdown();

	return result;
}
