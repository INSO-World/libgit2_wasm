#include <git2.h>
#include <assert.h>
#include <emscripten/wasmfs.h>
#include <emscripten/console.h>
#include <emscripten/emscripten.h>
#include <emscripten/threading.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

static git_repository* curr_repo = NULL;
static git_oid oids[20]; 
static size_t commit_count;

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

void print_error(char *msg){
	const git_error *e = git_error_last();
	char err[512];

	snprintf(err, sizeof(err), "%s: %s", msg, (e && e->message) ? e->message : "unknown error");
	emscripten_console_error(err); 
}

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
	return NULL;
}

EMSCRIPTEN_KEEPALIVE
void mount_opfs_in_thread() {
    pthread_t t;
    pthread_create(&t, NULL, mount_opfs, NULL);
    pthread_detach(t);
}

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

EMSCRIPTEN_KEEPALIVE
char* get_commit_diff(int i){
    size_t bufsize = 65536;
    char* buffer = malloc(bufsize);
    if (!buffer){
            emscripten_console_error("error allocating memory to buffer");
            return NULL;
    }
    buffer[0] = '\0';

    git_commit *commit;
    int err = git_commit_lookup(&commit, curr_repo, &oids[i]);
    if(err != 0){
        free(buffer);
        print_error("error looking up commit");
        return NULL;
    }
    unsigned int parentcount = git_commit_parentcount(commit);

    strcat(buffer, "[");
    if (parentcount > 0){
        git_diff *diff;
        git_commit *parent;
        git_tree *parent_tree, *child_tree;
        git_diff_stats_format_t format = GIT_DIFF_STATS_NUMBER;
        git_diff_stats *stats;
        int sec_count = 0;
        char *prim_tok, *sec_tok, *prim_save_ptr, *sec_save_ptr;


        err = git_commit_parent(&parent, commit, 0);
        if(err != 0){
            free(buffer);
            print_error("error looking up parent");
            git_commit_free(commit);
            return NULL;
        }


        err = git_commit_tree(&parent_tree, parent);
        git_commit_free(parent);
        if(err != 0){
            free(buffer);
            git_commit_free(commit);
            print_error("error looking up git tree of parent commit");
            return NULL;
        }

        err = git_commit_tree(&child_tree, commit);
        git_commit_free(commit);
        if(err != 0){
            git_tree_free(parent_tree);
            free(buffer);
            print_error("error looking up git tree of child commit");
            return NULL;
        }

        err = git_diff_tree_to_tree(&diff, curr_repo, parent_tree, child_tree, NULL);
        git_tree_free(parent_tree);
        git_tree_free(child_tree);
        if(err != 0){
            free(buffer);
            print_error("error getting diff between this commit and the parent commit");
            return NULL;
        }

        err = git_diff_get_stats(&stats, diff);
        git_diff_free(diff);
        if(err != 0){
            free(buffer);
            print_error("error getting diff stats");
            return NULL;
        }

        git_buf buf = GIT_BUF_INIT;
        err = git_diff_stats_to_buf(&buf, stats, format, 0);
        git_diff_stats_free(stats);
        if(err != 0){
            git_buf_dispose(&buf);
            free(buffer);
            print_error("error writing diff stats to buffer");
            return NULL;
        }

        prim_tok = strtok_r(buf.ptr, "\n", &prim_save_ptr);
        while(prim_tok != NULL){
            char entry[1024];
            char *tokens[3];

            sec_tok = strtok_r(prim_tok, " ", &sec_save_ptr);
            while(sec_tok != NULL){
                tokens[sec_count++] = sec_tok;
                sec_tok = strtok_r(NULL, " ", &sec_save_ptr);
            }
            sec_count = 0;

            prim_tok = strtok_r(NULL, "\n", &prim_save_ptr);
            snprintf(entry, sizeof(entry),
                        "{\"additions\":\"%s\",\"deletions\":\"%s\",\"file_name\":\"%s\"}%s",
                        tokens[0],
                        tokens[1],
                        tokens[2],
                        (prim_tok == NULL) ? "" : ",");
            strcat(buffer, entry);
        }
        git_buf_dispose(&buf);


    }

    strcat(buffer, "]");

    return buffer;
}

EMSCRIPTEN_KEEPALIVE
char* get_commit_info(){
	emscripten_console_log("getting commit info...");
	int count = 0;
	size_t bufsize = 65536;
	char* buffer = malloc(bufsize);
	if (!buffer){
		emscripten_console_error("error allocating memory to buffer");
		return NULL;
	}
	
	strcpy(buffer, "[");
	while(count++ < commit_count){
		char entry[1024];
		git_commit *commit;
		int err = git_commit_lookup( &commit, curr_repo, &oids[count-1]);
		if(err != 0){
		    free(buffer);
			print_error("error looking up commit");
			return NULL;
		}

		const git_signature* sig = git_commit_author(commit);
		unsigned int parentcount = git_commit_parentcount(commit);
		const char* msg = git_commit_message(commit);
		git_commit_free(commit);

		char escaped_msg[1024];
		escape_json_string(msg, escaped_msg, sizeof(escaped_msg));


		snprintf(entry, sizeof(entry),
			"{\"author\":\"%s\",\"email\":\"%s\",\"message\":\"%s\",\"parents\":%u,\"oid\":%d}%s",
			sig->name,
			sig->email,
			escaped_msg,
			parentcount,
			count-1,
			(count == commit_count) ? "" : ",");

		strcat(buffer, entry);


	}

	strcat(buffer, "]");
	emscripten_console_log("successfully got commit info");
	return buffer;
}

int open_repo(const char *name){
	char path[256];
	snprintf(path, sizeof(path), "/opfs/repo/%s", name);

	int error;

	//TODO use assert
	error = git_repository_open(&curr_repo, path);

	if (error < 0) {
        	
		char msg[512];

		snprintf(msg, sizeof(msg), "Failed to open repository at %s", path);
        	print_error(msg);
		return -1;
	}

	return 0;
}

EMSCRIPTEN_KEEPALIVE
int walk_commits(){
	emscripten_console_log("trying to walk through commits...");
	git_revwalk *walker;
	int err = git_revwalk_new(&walker, curr_repo);
	if(err != 0){
		print_error("error creating revision walker");
		return -1;
	}

	err = git_revwalk_push_head(walker);
	if(err != 0){
		print_error("error pushing head of repository");
		return -1;
	}

	
	git_oid oid;
	commit_count = 0;
	while(git_revwalk_next(&oids[commit_count], walker) == 0 && ++commit_count < 20){
	}
	
	git_revwalk_free(walker);
	
	return 0;
}



int init() {
	emscripten_console_log("initializing...");
	int res = git_libgit2_init();
	char buf[64];
	snprintf(buf, sizeof(buf), "init: %d", res);
	emscripten_console_log(buf);
	return 0;
}

int shutdown(){
	git_repository_free(curr_repo);
	git_libgit2_shutdown();
	return 0;
}
