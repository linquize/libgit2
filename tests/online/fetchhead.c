#include "clar_libgit2.h"

#include "fileops.h"
#include "fetchhead.h"
#include "../fetchhead/fetchhead_data.h"
#include "git2/clone.h"
#include <libssh2.h>

#define LIVE_REPO_URL "git://github.com/libgit2/TestGitRepository"

static git_repository *g_repo;
static git_clone_options g_options;

void test_online_fetchhead__initialize(void)
{
	git_remote_callbacks dummy_callbacks = GIT_REMOTE_CALLBACKS_INIT;
	g_repo = NULL;

	memset(&g_options, 0, sizeof(git_clone_options));
	g_options.version = GIT_CLONE_OPTIONS_VERSION;
	g_options.remote_callbacks = dummy_callbacks;
}

void test_online_fetchhead__cleanup(void)
{
	if (g_repo) {
		git_repository_free(g_repo);
		g_repo = NULL;
	}

	cl_fixture_cleanup("./foo");
}

static void fetchhead_test_clone(void)
{
	cl_git_pass(git_clone(&g_repo, LIVE_REPO_URL, "./foo", &g_options));
}

static void fetchhead_test_fetch(const char *fetchspec, const char *expected_fetchhead)
{
	git_remote *remote;
	git_buf fetchhead_buf = GIT_BUF_INIT;
	int equals = 0;

	cl_git_pass(git_remote_load(&remote, g_repo, "origin"));
	git_remote_set_autotag(remote, GIT_REMOTE_DOWNLOAD_TAGS_AUTO);

	if(fetchspec != NULL) {
		git_remote_clear_refspecs(remote);
		git_remote_add_fetch(remote, fetchspec);
	}

	cl_git_pass(git_remote_connect(remote, GIT_DIRECTION_FETCH));
	cl_git_pass(git_remote_download(remote));
	cl_git_pass(git_remote_update_tips(remote, NULL, NULL));
	git_remote_disconnect(remote);
	git_remote_free(remote);

	cl_git_pass(git_futils_readbuffer(&fetchhead_buf, "./foo/.git/FETCH_HEAD"));

	equals = (strcmp(fetchhead_buf.ptr, expected_fetchhead) == 0);

	git_buf_free(&fetchhead_buf);

	cl_assert(equals);
}

void test_online_fetchhead__wildcard_spec(void)
{
	fetchhead_test_clone();
	fetchhead_test_fetch(NULL, FETCH_HEAD_WILDCARD_DATA2);
	cl_git_pass(git_tag_delete(g_repo, "annotated_tag"));
	cl_git_pass(git_tag_delete(g_repo, "blob"));
	cl_git_pass(git_tag_delete(g_repo, "commit_tree"));
	cl_git_pass(git_tag_delete(g_repo, "nearly-dangling"));
	fetchhead_test_fetch(NULL, FETCH_HEAD_WILDCARD_DATA);
}

void test_online_fetchhead__explicit_spec(void)
{
	fetchhead_test_clone();
	fetchhead_test_fetch("refs/heads/first-merge:refs/remotes/origin/first-merge", FETCH_HEAD_EXPLICIT_DATA);
}

void test_online_fetchhead__no_merges(void)
{
	git_config *config;

	fetchhead_test_clone();

	cl_git_pass(git_repository_config(&config, g_repo));
	cl_git_pass(git_config_delete_entry(config, "branch.master.remote"));
	cl_git_pass(git_config_delete_entry(config, "branch.master.merge"));
	git_config_free(config);

	fetchhead_test_fetch(NULL, FETCH_HEAD_NO_MERGE_DATA2);
	cl_git_pass(git_tag_delete(g_repo, "annotated_tag"));
	cl_git_pass(git_tag_delete(g_repo, "blob"));
	cl_git_pass(git_tag_delete(g_repo, "commit_tree"));
	cl_git_pass(git_tag_delete(g_repo, "nearly-dangling"));
	fetchhead_test_fetch(NULL, FETCH_HEAD_NO_MERGE_DATA);
	cl_git_pass(git_tag_delete(g_repo, "commit_tree"));
	fetchhead_test_fetch(NULL, FETCH_HEAD_NO_MERGE_DATA3);
}

static int read_fetchhead(const char *ref_name, const char *remote_url,
	const git_oid *oid, unsigned int is_merge, void *payload)
{
	const char *expected = (const char *)payload;
	GIT_UNUSED(ref_name);
	GIT_UNUSED(oid);
	GIT_UNUSED(is_merge);
	GIT_UNUSED(payload);

	cl_git_pass(strcmp(remote_url, expected));
	return 0;
}

static void cred_ssh_interactive_cb(const char* name, int name_len, const char* instruction, int instruction_len, int num_prompts, const LIBSSH2_USERAUTH_KBDINT_PROMPT* prompts, LIBSSH2_USERAUTH_KBDINT_RESPONSE* responses, void **abstract)
{
	const char *remote_ssh_passphrase = cl_getenv("GITTEST_REMOTE_SSH_PASSPHRASE");
	responses->text = remote_ssh_passphrase;
	responses->length = strlen(remote_ssh_passphrase);
}

static int cred_acquire_cb(git_cred **cred,
		const char * url,
		const char * username_from_url,
		unsigned int allowed_types,
		void * payload)
{
	const char *remote_user = cl_getenv("GITTEST_REMOTE_USER");
	const char *remote_pass = cl_getenv("GITTEST_REMOTE_PASS");
	const char *remote_ssh_key = cl_getenv("GITTEST_REMOTE_SSH_KEY");
	const char *remote_ssh_pubkey = cl_getenv("GITTEST_REMOTE_SSH_PUBKEY");
	const char *remote_ssh_passphrase = cl_getenv("GITTEST_REMOTE_SSH_PASSPHRASE");
	const char *remote_default = cl_getenv("GITTEST_REMOTE_DEFAULT");

	GIT_UNUSED(url);
	GIT_UNUSED(username_from_url);
	GIT_UNUSED(payload);

	if (GIT_CREDTYPE_USERNAME & allowed_types) {
		if (!remote_user) {
			printf("GITTEST_REMOTE_USER must be set\n");
			return -1;
		}

		return git_cred_username_new(cred, remote_user);
	}

	if (GIT_CREDTYPE_DEFAULT & allowed_types) {
		if (!remote_default) {
			printf("GITTEST_REMOTE_DEFAULT must be set to use NTLM/Negotiate credentials\n");
			return -1;
		}

		return git_cred_default_new(cred);
	}

	if (GIT_CREDTYPE_SSH_INTERACTIVE & allowed_types) {
		if (!remote_user || !remote_ssh_passphrase) {
			printf("GITTEST_REMOTE_USER, and GITTEST_REMOTE_SSH_PASSPHRASE must be set\n");
			return -1;
		}

		printf("git_cred_ssh_interactive_new\n");
		return git_cred_ssh_interactive_new(cred, remote_user, cred_ssh_interactive_cb, NULL);
	}

	if (GIT_CREDTYPE_SSH_KEY & allowed_types) {
		if (!remote_user || !remote_ssh_pubkey || !remote_ssh_key || !remote_ssh_passphrase) {
			printf("GITTEST_REMOTE_USER, GITTEST_REMOTE_SSH_PUBKEY, GITTEST_REMOTE_SSH_KEY and GITTEST_REMOTE_SSH_PASSPHRASE must be set\n");
			return -1;
		}

		printf("%s\n", remote_user);
		printf("%s\n", remote_ssh_pubkey);
		printf("%s\n", remote_ssh_key);
		printf("%s\n", remote_ssh_passphrase);
		return git_cred_ssh_key_new(cred, remote_user, remote_ssh_pubkey, remote_ssh_key, remote_ssh_passphrase);
	}

	if (GIT_CREDTYPE_USERPASS_PLAINTEXT & allowed_types) {
		if (!remote_user || !remote_pass) {
			printf("GITTEST_REMOTE_USER and GITTEST_REMOTE_PASS must be set\n");
			return -1;
		}

		return git_cred_userpass_plaintext_new(cred, remote_user, remote_pass);
	}

	return -1;
}

void test_online_fetchhead__url_userinfo(void)
{
	git_remote *remote;
	git_buf full_url = GIT_BUF_INIT;
	git_buf url_no_user = GIT_BUF_INIT;
	git_remote_callbacks callbacks = GIT_REMOTE_CALLBACKS_INIT;
	const char *remote_url_scheme = cl_getenv("GITTEST_REMOTE_URL_SCHEME");
	const char *remote_url_host = cl_getenv("GITTEST_REMOTE_URL_HOST");
	const char *remote_url_port = cl_getenv("GITTEST_REMOTE_URL_PORT");
	const char *remote_url_path = cl_getenv("GITTEST_REMOTE_URL_PATH");
	const char *remote_user = cl_getenv("GITTEST_REMOTE_USER");
	const char *remote_pass = cl_getenv("GITTEST_REMOTE_PASS");

	if (!remote_url_host || !remote_url_path || !remote_user)
		clar__skip();

	if (remote_url_scheme) {
		git_buf_printf(&full_url, "%s://%s%s%s@%s%s%s%s",
			remote_url_scheme, remote_user,
			remote_pass ? ":" : "", remote_pass ? remote_pass : "",
			remote_url_host,
			remote_url_port ? ":" : "",	remote_url_port ? remote_url_port : "",
			remote_url_path);
		git_buf_printf(&url_no_user, "%s://%s%s%s%s",
			remote_url_scheme, remote_url_host,
			remote_url_port ? ":" : "",	remote_url_port ? remote_url_port : "",
			remote_url_path);
	} else {
		git_buf_printf(&full_url, "%s@%s%s%s:%s",
			remote_user, remote_url_host,
			remote_url_port ? ":" : "", remote_url_port, remote_url_path);
		git_buf_printf(&url_no_user, "%s%s%s:%s",
			remote_url_host, remote_url_port ? ":" : "", remote_url_port, remote_url_path);
	}
	cl_git_pass(git_repository_init(&g_repo, "./fetch", 0));

	cl_git_pass(git_remote_create_anonymous(&remote, g_repo, git_buf_cstr(&full_url), NULL));
	callbacks.credentials = cred_acquire_cb;
	git_remote_set_callbacks(remote, &callbacks);
	cl_git_pass(git_remote_connect(remote, GIT_DIRECTION_FETCH));
	cl_git_pass(git_remote_download(remote));
	cl_git_pass(git_remote_update_tips(remote, NULL, NULL));
	git_remote_disconnect(remote);
	git_remote_free(remote);
	cl_git_pass(git_repository_fetchhead_foreach(g_repo, read_fetchhead, (void *)git_buf_cstr(&url_no_user)));
}
