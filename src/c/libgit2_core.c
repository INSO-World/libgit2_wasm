
#include "libgit2_core.h"
#include <string.h>

/**
 * Currently opened Git repository.
 * Only a single repository is supported at a time.
 */
static git_repository* repo = NULL;

/**
 * Circular buffer of commit OIDs collected during revision walking.
 * The indices are used as lightweight commit identifiers in JavaScript.
 */
static git_oid oids[CORE_MAX_COMMITS];

/**
 * Number of commits collected during the last walk.
 */
static size_t commit_count;

/**
 * Initializes the libgit2 library.
 *
 * Must be called once before any other libgit2 operations.
 */
int core_init() {
	return git_libgit2_init();
}

/**
 * Shuts down libgit2 and releases global resources.
 */
int core_shutdown(){
	if(repo){
		git_repository_free(repo);
	}
	git_libgit2_shutdown();
	return 0;
}

/**
 * Opens a Git repository stored inside the file system.
 *
 * @param path of the repository
 *
 * @return value of git_repository_open()
 */
int core_open_repo(const char *path){
	if(repo){
		git_repository_free(repo);
        repo = NULL;
	}
	int error;

	return error = git_repository_open(&repo, path);
}

/**
 * Walks the commit history starting from HEAD.
 *
 * Collects up to a fixed maximum number of commits and stores their
 * object IDs in a global array for later access.
 *
 * @return 0 on success, -1 on failure
 */
int core_walk_commits(){
	git_revwalk *walker;
	int err = git_revwalk_new(&walker, repo);
	if(err != 0){
		return err;
	}

	err = git_revwalk_push_head(walker);
	if(err != 0){
		return err;
	}


	git_oid oid;
	commit_count = 0;
	while(git_revwalk_next(&oids[commit_count], walker) == 0 && ++commit_count < CORE_MAX_COMMITS){
	}

	git_revwalk_free(walker);

	return 0;
}

size_t core_commit_count() {
	return commit_count;
}

/**
 * Saves metadata for commit with index i of the last revision walk in a struct.
 *
 * The struct contains
 *  - author name
 *  - author email
 *  - commit message
 *  - number of parent commits
 *
 * @return 0 on Success, -1 on Failure
 *
 */
int core_get_commit_info(size_t i, core_commit_info_t *out) {
    if (i >= commit_count) return -1;

    git_commit *commit;
    if (git_commit_lookup(&commit, repo, &oids[i]) != 0)
        return -1;

    const git_signature *sig = git_commit_author(commit);
    out->author = sig->name;
    out->email = sig->email;
    out->message = git_commit_message(commit);
    out->parent_count = git_commit_parentcount(commit);

    git_commit_free(commit);
    return 0;
}

/**
 * Computes numeric diff statistics for a commit.
 *
 * @param Index of commit in OID array
 * @param *out array of structs for diff stats
 * @param max_entries maximum amount of entries saved in out
 * @param *out_count number of entries saved.
 */
int core_get_commit_diff_stats(
    size_t index,
    core_diff_stat_t *out,
    size_t max_entries,
    size_t *out_count
) {
    if (index >= commit_count) return -1;

    git_commit *commit = NULL;
    git_commit *parent = NULL;
    git_tree *parent_tree = NULL;
    git_tree *child_tree = NULL;
    git_diff *diff = NULL;

    int err = git_commit_lookup(&commit, repo, &oids[index]);
    if (err) goto cleanup;

    if (git_commit_parentcount(commit) == 0) {
        *out_count = 0;
        goto cleanup;
    }

    err = git_commit_parent(&parent, commit, 0);
    if (err) goto cleanup;

    err = git_commit_tree(&parent_tree, parent);
    if (err) goto cleanup;

    err = git_commit_tree(&child_tree, commit);
    if (err) goto cleanup;

    err = git_diff_tree_to_tree(
        &diff,
        repo,
        parent_tree,
        child_tree,
        NULL
    );
    if (err) goto cleanup;

    size_t count = 0;
    size_t deltas = git_diff_num_deltas(diff);

    for (size_t i = 0; i < deltas && count < max_entries; i++) {
         git_patch *patch = NULL;
            if (git_patch_from_diff(&patch, diff, i) != 0)
                continue;

            size_t  adds, dels;
            git_patch_line_stats(NULL, &adds, &dels, patch);

            const git_diff_delta *delta = git_diff_get_delta(diff, i);

            out[count].additions = (uint32_t)adds;
            out[count].deletions = (uint32_t)dels;
            out[count].file      = delta->new_file.path;  // borrowed
            count++;

            git_patch_free(patch);
    }

    *out_count = count;

cleanup:
    git_diff_free(diff);
    git_tree_free(parent_tree);
    git_tree_free(child_tree);
    git_commit_free(parent);
    git_commit_free(commit);
    return err ? -1 : 0;
}

