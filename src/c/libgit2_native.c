#include "libgit2_core.h"
#include<stdlib.h>
#include <time.h>

void print_error(char *msg);

/**
 * Escapes a C string for safe inclusion in a JSON string.
 *
 * @param input     Original string
 * @param out       Output buffer
 * @param out_size  Size of output buffer
 *
 * @return Pointer to the output buffer
 */
char* escape_json_string(const char* input, char* out, size_t out_size) {
    size_t j = 0;
    for (size_t i = 0; input[i] && j < out_size - 1; i++) {
        char c = input[i];
        if (c == '\"' || c == '\\') {
            if (j + 2 >= out_size) break;
            out[j++] = '\\';
            out[j++] = c;
        } else if (c == '\n') {
            if (j + 2 >= out_size) break;
            out[j++] = '\\';
            out[j++] = 'n';
        } else if (c == '\r') {
            continue; // optional
        } else if (c == '\t') {
            if (j + 2 >= out_size) break;
            out[j++] = '\\';
            out[j++] = 't';
        } else {
            out[j++] = c;
        }
    }
    out[j] = '\0';
    return out;
}

/**
 * Computes numeric diff statistics for a commit.
 *
 * @param i Index into the commit OID array
 *
 * @return JSON array string containing diff statistics
 *
 * The returned buffer is heap-allocated and must be freed by JavaScript.
 */
char* get_commit_diff(int i){
    core_diff_stat_t stats[64];
    size_t count;

    if(core_get_commit_diff_stats(i, stats, 64, &count) < 0){
        printf("There was an error calculating diff\n");
        return NULL;
    }


    size_t bufsize = 65536;
    char* buffer = malloc(bufsize);
    if (!buffer){
        printf("error allocating memory to buffer\n");
        return NULL;
    }
    buffer[0] = '\0';


    strcat(buffer, "[");
    for (int j = 0; j<count; j++){
        char escaped_file[512];
        escape_json_string(stats[j].file, escaped_file, sizeof(escaped_file));
        free((char*)stats[j].file);

        char entry[1024];
        // Convert libgit2 diff stats into a JSON array
        snprintf(entry, sizeof(entry),
                 "{\"additions\":%u,\"deletions\":%u,\"file_name\":\"%s\"}%s",
                 stats[j].additions,
                 stats[j].deletions,
                 escaped_file,
                 (j+1 >= count) ? "" : ",");
        strcat(buffer, entry);
    }
    strcat(buffer, "]");

    return buffer;
}

/**
 * Returns metadata for all commits collected during the last revision walk.
 *
 * Each entry contains:
 *  - author name
 *  - author email
 *  - commit message
 *  - number of parent commits
 *  - index-based commit identifier
 *
 * @return JSON array string
 *
 * The returned buffer must be freed by JavaScript.
 */
char *get_commit_info() {
    size_t count = core_commit_count();
    char *buffer = malloc(8388608);

    if(count > 1000) count = 1000;

    strcpy(buffer, "[");

    for (size_t i = 0; i < count; i++) {
        core_commit_info_t info;
        core_get_commit_info(i, &info);

        char escaped_msg[4096];
        char escaped_author[512];
        char escaped_email[512];

        escape_json_string(info.message, escaped_msg, sizeof(escaped_msg));
        escape_json_string(info.author, escaped_author, sizeof(escaped_author));
        escape_json_string(info.email, escaped_email, sizeof(escaped_email));

        char entry[8192];
        snprintf(entry, sizeof(entry),
                 "{\"author\":\"%s\",\"email\":\"%s\",\"message\":\"%s\","
                 "\"parents\":%u,\"oid\":%zu}%s",
                 escaped_author,
                 escaped_email,
                 escaped_msg,
                 info.parent_count,
                 i,
                 (i + 1 == count) ? "" : ",");

        strcat(buffer, entry);
    }

    strcat(buffer, "]");
    return buffer;
}

/**
 * Walks the commit history starting from HEAD.
 *
 * Collects up to a fixed maximum number of commits and stores their
 * object IDs in a global array for later access.
 *
 * @return 0 on success, -1 on failure
 */
int walk_commits(){
    int err = core_walk_commits();
    if(err != 0){
        char msg[512];
        snprintf(msg,sizeof(msg),"error when walking through commits");
        print_error(msg);
        return -1;
    }

    return 0;
}

/**
 * Prints the last libgit2 error to the JavaScript console.
 *
 * @param msg Contextual error message
 */
void print_error(char *msg){
    const git_error *e = git_error_last();
    printf("%s: %s\n", msg, (e && e->message) ? e->message : "unknown error");
}

/**
 * Opens a Git repository stored inside OPFS.
 *
 * @param name Name of the repository directory inside /opfs/repo
 *
 * @return 0 on success, -1 on failure
 */
int open_repo(const char *name){
    char path[256];
    snprintf(path, sizeof(path), "%s", name);

    int error;

    error = core_open_repo(path);

    if (error < 0) {

        char msg[512];

        snprintf(msg, sizeof(msg), "Failed to open repository at %s", path);
        print_error(msg);
        return -1;
    }
    return 0;
}

/**
 * Initializes the libgit2 library.
 *
 * Must be called once before any other libgit2 operations.
 */
int init() {
    core_init();
    return 0;
}

/**
 * Shuts down libgit2 and releases global resources.
 */
int shutdown(){
    return core_shutdown();
}

static double ms_elapsed(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000.0 +
           (end.tv_nsec - start.tv_nsec) / 1e6;
}

int main(int argc, char* argv[]){
    if(argc < 2 || argc > 3){
        printf("Usage: %s <repo-path> [iterations]", argv[0] );
        return 1;
    }
    int iterations = (argc >= 3) ? atoi(argv[2]) : 1;
    if(iterations <= 0) iterations = 1;

    struct timespec t0,t1;
	clock_gettime(CLOCK_MONOTONIC,&t0);
    init();
    clock_gettime(CLOCK_MONOTONIC,&t1);
    FILE *finit = fopen("bench_init.csv", "w");
    FILE *fruns = fopen("bench_runs.csv", "w");
	double init_ms = ms_elapsed(t0,t1);
	fprintf(finit, "init\n");
	fprintf(finit, "%f",init_ms);
	fprintf(finit, "%f",init_ms);

    fprintf(fruns, "iteration,open_repo_ms,walk_commits_ms,get_commit_info_ms,diff_total_ms\n");
    for (int i = 0; i<iterations; i++) {
    	clock_gettime(CLOCK_MONOTONIC,&t0);
        if(open_repo(argv[1])) break;
        clock_gettime(CLOCK_MONOTONIC,&t1);
        double open_repo_ms = ms_elapsed(t0,t1);

        clock_gettime(CLOCK_MONOTONIC,&t0);
        if(walk_commits()) break;
		clock_gettime(CLOCK_MONOTONIC,&t1);
		double walk_commits_ms = ms_elapsed(t0,t1);

		clock_gettime(CLOCK_MONOTONIC,&t0);
        char *info = get_commit_info();
        free(info);
        clock_gettime(CLOCK_MONOTONIC,&t1);
        double get_commit_info_ms = ms_elapsed(t0,t1);

        clock_gettime(CLOCK_MONOTONIC,&t0);
        int commit_count = core_commit_count();
        for (size_t c = 0; c < commit_count; c++) {
            char *diff = get_commit_diff((int) c);
            free(diff);
        }
        clock_gettime(CLOCK_MONOTONIC,&t1);
        double diff_total_ms = ms_elapsed(t0,t1);

        fprintf(fruns, "%d, %f, %f, %f, %f\n",i, open_repo_ms,walk_commits_ms, get_commit_info_ms, diff_total_ms );

    }
    shutdown();
    return 0;
}