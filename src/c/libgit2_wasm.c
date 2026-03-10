/**
 * libgit2_wasm.c
 *
 * WebAssembly interface layer for libgit2.
 *
 * This file exposes a minimal C API that can be called from JavaScript
 * via Emscripten's ccall interface.
 *
 * Responsibilities:
 *  - initialize and shut down libgit2
 *  - mount the Origin Private File System (OPFS) inside the WASM runtime
 *  - open Git repositories stored in OPFS
 *  - walk commits and extract commit metadata
 *  - compute lightweight diff statistics
 *
 * Design notes:
 *  - OPFS is mounted using wasmfs and must be initialized before any
 *    filesystem access from libgit2.
 *  - Data returned to JavaScript is serialized as JSON strings.
 *  - Memory ownership of returned buffers is transferred to JavaScript,
 *    which is responsible for freeing them.
 *
 * Limitations:
 *  - Only a fixed number of commits is stored (currently 20).
 *  - Diff information is limited to numeric statistics.
 *  - All operations assume a single active repository.
 */

#include <assert.h>
#include <emscripten/wasmfs.h>
#include <emscripten/console.h>
#include <emscripten/emscripten.h>
#include <emscripten/threading.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "libgit2_core.h"
#include <stdatomic.h>

/**
* flag that describes whether the mounting processed has finished or not.
* is 0 if it hasn't finished and 1 if it has.
**/
static atomic_int opfs_ready = 0;

/**
 * Test function to verify OPFS write access from inside WebAssembly.
 *
 * Creates or overwrites a file inside /opfs/repo and writes the given
 * message into it.
 *
 * This function is primarily used to validate that:
 *  - OPFS is mounted correctly
 *  - file I/O works from within the WASM runtime
 */
EMSCRIPTEN_KEEPALIVE
void test_write(const char *msg, const char *name) {
	char path[256];
	snprintf(path, sizeof(path), "/opfs/repo/%s", name);

	int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0666);

	assert(fd > 0);
	emscripten_console_log("opened directory");

	assert(write(fd, msg, strlen(msg)) != -1);
	emscripten_console_log("write succeeded");

	close(fd);
}

/**
 * Prints the last libgit2 error to the JavaScript console.
 *
 * @param msg Contextual error message
 */
void print_error(char *msg){
	const git_error *e = git_error_last();
	char err[512];

	snprintf(err, sizeof(err), "%s: %s", msg, (e && e->message) ? e->message : "unknown error");
	emscripten_console_error(err); 
}

/**
 * Mounts OPFS into the WASM virtual filesystem.
 *
 * This function runs in a separate thread because wasmfs requires
 * filesystem setup to occur outside of the main execution path.
 *
 */
void* mount_opfs(void* arg){
	emscripten_console_log("mounting opfs..."); 
	backend_t opfs = wasmfs_create_opfs_backend();

	int err = wasmfs_create_directory("/opfs", 0777 , opfs);
	if(err != 0){
	   emscripten_console_error("failed mounting OPFS");
	   return NULL;
	}
	emscripten_console_log("successfully mounted OPFS");

	emscripten_console_log("creating OPFS directory...");
	err = mkdir("/opfs/repo", 0777);
	if (err != 0 && errno != EEXIST) {
        emscripten_console_error("mkdir failed");
        return NULL;
    }
	emscripten_console_log("successfully created OPFS directory");
	atomic_store(&opfs_ready, 1);
	return NULL;
}

/**
* returns opfs_ready
**/
EMSCRIPTEN_KEEPALIVE
int is_opfs_ready() {
    return atomic_load(&opfs_ready);
}

/**
 * Spawns a detached thread that mounts OPFS.
 *
 * This function must be called from JavaScript before any filesystem
 * access through libgit2.
 */
EMSCRIPTEN_KEEPALIVE
void mount_opfs_in_thread() {
    pthread_t t;
    pthread_create(&t, NULL, mount_opfs, NULL);
    pthread_detach(t);
}

/**
 * Escapes a C string for safe inclusion in a JSON string.
 *
 * @param input     Original string
 * @param out       Output buffer
 * @param out_size  Size of output buffer
 *
 * @return Pointer to the output buffer
 */
EMSCRIPTEN_KEEPALIVE
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
EMSCRIPTEN_KEEPALIVE
char* get_commit_diff(int i){
	core_diff_stat_t stats[64];
    size_t count;

    if(core_get_commit_diff_stats(i, stats, 64, &count) < 0){
    	emscripten_console_error("There was an error calculating diff");
    	return NULL;
    }


    size_t bufsize = 65536;
    char* buffer = malloc(bufsize);
    if (!buffer){
            emscripten_console_error("error allocating memory to buffer");
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

EMSCRIPTEN_KEEPALIVE
size_t commit_count(){
	return core_commit_count();
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
EMSCRIPTEN_KEEPALIVE
char *get_commit_info() {
    size_t count = commit_count();
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
 * Opens a Git repository stored inside OPFS.
 *
 * @param name Name of the repository directory inside /opfs/repo
 *
 * @return 0 on success, -1 on failure
 */
int open_repo(const char *name){
	char path[256];
	snprintf(path, sizeof(path), "/opfs/repo/%s", name);

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
 * Walks the commit history starting from HEAD.
 *
 * Collects up to a fixed maximum number of commits and stores their
 * object IDs in a global array for later access.
 *
 * @return 0 on success, -1 on failure
 */
EMSCRIPTEN_KEEPALIVE
int walk_commits(){
	emscripten_console_log("trying to walk through commits...");

	int err = core_walk_commits();
	if(err != 0){
		emscripten_console_error("error when walking through commits");
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
	emscripten_console_log("initializing...");
	int res = core_init();
	char buf[64];
	snprintf(buf, sizeof(buf), "init: %d", res);
	emscripten_console_log(buf);
	return 0;
}

/**
 * Shuts down libgit2 and releases global resources.
 */
int shutdown(){
	return core_shutdown();
}

