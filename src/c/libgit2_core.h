#pragma once
#include <git2.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define CORE_MAX_COMMITS 20
#define CORE_MAX_DIFFS   128

typedef struct {
    const char *author;
    const char *email;
    const char *message;
    unsigned int parent_count;
} core_commit_info_t;

typedef struct {
    int additions;
    int deletions;
    const char *file;
} core_diff_stat_t;

/* lifecycle */
int core_init(void);
int core_shutdown(void);

/* repository */
int core_open_repo(const char *path);

/* commits */
int core_walk_commits(void);
size_t core_commit_count(void);
int core_get_commit_info(size_t index, core_commit_info_t *out);

/* diffs */
int core_get_commit_diff_stats(
    size_t index,
    core_diff_stat_t *out,
    size_t max_entries,
    size_t *out_count
);
