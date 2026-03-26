import createModule from '../wasm-build/libgit2_wasm.js';

let LibGit2Module;

self.onmessage = async (e) => {
    const { repoPath, iterations } = e.data;

    const bench = { setup: {}, runs: [] };

    // initialize module inside worker
    const t0 = performance.now();
    LibGit2Module = await createModule();
    bench.setup.wasmLoad = performance.now() - t0;

    const t1 = performance.now();
    LibGit2Module._init();
    bench.setup.init = performance.now() - t1;

    const t2 = performance.now();
    LibGit2Module._mount_opfs_in_thread();
    bench.setup.mount = await waitForMount() - t2;

    bench.iterations = iterations;

    for (let i = 0; i < iterations; i++) {
        const run = {};

        const t0 = performance.now();
        const openRes = LibGit2Module.ccall("open_repo", "int", ["string"], [repoPath]);
        run.openRepo = performance.now() - t0;
        if (openRes !== 0) break;

        const t1 = performance.now();
        const walkRes = LibGit2Module.ccall("walk_commits", "int", [], []);
        run.walkCommits = performance.now() - t1;
        if (walkRes !== 0) break;

        const t2 = performance.now();
        const infoPtr = LibGit2Module.ccall("get_commit_info", "number", [], []);
        LibGit2Module._free(infoPtr);
        run.getCommitInfo = performance.now() - t2;

        const commitCount = LibGit2Module.ccall("commit_count", "int", [], []);
        const td = performance.now();
        for (let c = 0; c < commitCount; c++) {
            const diffPtr = LibGit2Module.ccall("get_commit_diff", "number", ["int"], [c]);
            if (diffPtr !== 0) LibGit2Module._free(diffPtr);
        }
        run.diff = performance.now() - td;
        bench.runs.push(run);
    }

    // post results back to main thread
    self.postMessage({ type: 'complete', bench });
};

/**
 * Async function that waits for mounting to finish by periodically polling the state.
 *
 * @param timeoutMs sets the time after which the mounting is considered erronous
 * @return {Promise<number>} returns a promise that when successfully resolved returns the time at which mounting was done and otherwise an error.
 */
async function waitForMount(timeoutMs = 5000) {
    const start = performance.now();
    return new Promise((resolve, reject) => {
        function poll() {
            if (LibGit2Module.ccall("is_opfs_ready", "int", [], [])) {
                resolve(performance.now());
            } else if (performance.now() - start > timeoutMs) {
                reject(new Error("OPFS mount timed out"));
            } else {
                setTimeout(poll, 10);
            }
        }
        poll();
    });
}