/**
 * main.js
 *
 * Browser-side entry point of the application.
 * This file coordinates:
 *  - OPFS setup
 *  - interaction with the libgit2 WebAssembly module
 *  - DOM updates for repository and commit information
 *
 * Low-level OPFS logic is delegated to fileAccess.js.
 * Low-level WASM interaction happens through Emscripten's ccall interface.
 */

import {uploadFile, listRepositories} from './opfs/opfsStorage.js';
import createModule from '../wasm-build/libgit2_wasm.js'


/**
 * Name of the directory inside OPFS where repositories are stored.
 */
import { DIRECTORY } from "./config.js";

const bench = {setup: {}, runs: []};
/**
 * Initialize the libgit2 WebAssembly module.
 * This loads the WASM binary, starts worker threads,
 * and prepares exported C functions for use via ccall.
 */
const LibGit2Module = await createModule();

/**
 * Expose selected functions globally for use in HTML event handlers.
 * This avoids inline module imports inside index.html.
 */
window.uploadFile = uploadFile;
window.testWrite = testWrite;
window.listRepositories = listRepositories;
window.openRepo = openRepo;

/**
 * Initializes the Origin Private File System (OPFS).
 * Creates the repository root directory if it does not yet exist.
 */
async function setupOPFS() {
    const opfsRoot = await navigator.storage.getDirectory();
    console.log(opfsRoot);
    // Create a hierarchy of files and folders
    const directoryHandle = await opfsRoot.getDirectoryHandle(DIRECTORY, {
        create: true,
    });
    console.log(await opfsRoot.getDirectoryHandle(DIRECTORY));
}


/**
 * Simple test function to verify OPFS writes from WASM.
 * Calls a C function that writes a file using OPFS-backed APIs.
 */
function testWrite() {
    console.log("trying to write to OPFS");
    LibGit2Module.ccall(
        "test_write",
        "void",
        ["string", "string"],
        ["it works!!!!", "newFile"]
    )
}
/**
 * Opens a Git repository using libgit2 inside WebAssembly.
 *
 * @param {string} name - Path to the repository inside OPFS
 *
 * NOTE:
 * This currently runs on the main thread.
 * A future improvement would be executing this in a worker thread
 * and improving debugging support.
 */
export function openRepo(name) {
    console.log("opening repository: " + name + "...");

    const res = LibGit2Module.ccall(
        "open_repo",
        "int",
        ["string"],
        [name]
    );

    if (res === 0) {
        console.log("successfully opened repository");
    }
    walkCommits(name);
}

/**
 * Displays or toggles the diff for a specific commit.
 *
 * @param {number} oid   - Commit object ID
 * @param {number} count - Commit index used for DOM lookup
 */
export function calcDiff(oid, count) {
    console.log("diff"+String(count));
    const el = document.getElementById("diff" + String(count))
    if (el) {
        window.getComputedStyle(el).display ==='table' ? el.style.display = 'none' : el.style.display = 'table';
    } else {
        const ptr = LibGit2Module.ccall(
            "get_commit_diff",
            "number",
            ["int"],
            [oid]
        )
        const str = LibGit2Module.UTF8ToString(ptr);
        LibGit2Module._free(ptr);

        const diff = JSON.parse(str);
        console.log(diff);
        const commitRow = document.getElementById(String(count));

        const table = document.createElement('table');
        table.className = "diff-table";
        table.id = "diff" + String(count);
        const thead = document.createElement('thead');
        const headerRow = document.createElement('tr');
        ['Additions', 'Deletions', 'File'].forEach(text => {
            const th = document.createElement('th');
            th.textContent = text;
            headerRow.appendChild(th);
        });
        thead.appendChild(headerRow);
        table.appendChild(thead);
        diff.forEach(line => {
                const lineRow = document.createElement('tr');

                const addTd = document.createElement('td');
                addTd.textContent = line.additions;
                lineRow.appendChild(addTd);

                const delTd = document.createElement('td');
                delTd.textContent = line.deletions;
                lineRow.appendChild(delTd);

                const fileTd = document.createElement('td');
                fileTd.textContent = line.file_name;
                lineRow.appendChild(fileTd);

                table.appendChild(lineRow);
            }
        )
        const newRow = document.createElement('tr');
        const newTd = document.createElement('td');
        newTd.colSpan = commitRow.children.length;
        newTd.appendChild(table);
        newRow.appendChild(newTd);
        commitRow.parentNode.insertBefore(newRow, commitRow.nextSibling);
    }
}

/**
 * Walks through repository commits using libgit2
 * and renders them into an HTML table.
 */
function walkCommits() {
    const res = LibGit2Module.ccall(
        "walk_commits",
        "int",
        [],
        []
    )
    if (res === 0) {
        console.log("successfully walked through commits");
    }

    const ptr = LibGit2Module.ccall(
        "get_commit_info",
        "number",
        [],
        []
    )
    const str = LibGit2Module.UTF8ToString(ptr);
    LibGit2Module._free(ptr);

    const commitInfo = JSON.parse(str);

    const commitTable = document.getElementById('commit-table');
    commitTable.innerHTML = "";

    const h2 = document.createElement('h2');
    h2.id = "commit-table-h2"
    h2.className = "heading"
    h2.textContent = "Last commits";
    commitTable.appendChild(h2);

    const table = document.createElement('table');
    table.className = "commit-table";
    table.id = "commit-table";

    const thead = document.createElement('thead');
    const headerRow = document.createElement('tr');
    ['Author', 'Email', 'Message', 'Parent commits'].forEach(text => {
        const th = document.createElement('th');
        th.textContent = text;
        headerRow.appendChild(th);
    });
    thead.appendChild(headerRow);
    table.appendChild(thead);

    const tbody = document.createElement('tbody');
    let count = 0;
    commitInfo.forEach(commit => {
        console.log(count);
        const row = document.createElement('tr');
        row.id = String(count);
        const nameTd = document.createElement('td');
        nameTd.textContent = commit.author;
        row.appendChild(nameTd);

        const emailTd = document.createElement('td');
        emailTd.textContent = commit.email;
        row.appendChild(emailTd);

        const msgTd = document.createElement('td');
        msgTd.textContent = commit.message;
        row.appendChild(msgTd);

        const nopTd = document.createElement('td');
        nopTd.textContent = commit.parents;
        row.appendChild(nopTd);
        tbody.appendChild(row);

        const buttonTd = document.createElement('td');
        const button = document.createElement('button');
        button.textContent = "show diff";
        const val = count.valueOf();
        button.onclick = () => calcDiff(commit.oid, val);
        buttonTd.appendChild(button);
        row.appendChild(buttonTd);
        tbody.appendChild(row);
        count++;
    })
    count = 0;
    table.appendChild(tbody);
    commitTable.appendChild(table);
}

/**
 * Creates HTML elements for the repositories uploaded.
 *
 * Each Repository is one line in the table.
 */
async function showFiles(repos){
    if(repos.length !== 0){

        const fileTable = document.getElementById('file-table');
        const h2 = document.createElement('h2');
        h2.id = "file-table-h2"
        h2.className = "subheading"
        h2.textContent = "Uploaded files";
        fileTable.appendChild(h2);

        repos.forEach(name => {
            const div = document.createElement('div');
            div.className = "file-table-file";
            const p = document.createElement('p');
            p.textContent = name;
            div.appendChild(p);
            const button = document.createElement('button');
            button.textContent = "select";
            button.onclick = () => openRepo(name);
            const benchButton = document.createElement('button');
            benchButton.textContent = "benchmark";
            benchButton.onclick = () => runBenchmark(name,100);

            div.appendChild(button);
            div.appendChild(benchButton);
            fileTable.appendChild(div);
        });
    }
}

/**
 * Application startup sequence:
 *  1. Initialize OPFS
 *  2. Display existing files
 *  3. Initialize libgit2
 *  4. Mount OPFS inside the WASM runtime (worker thread)
 */
(async function startup() {
    const setup = {};
    const t3 = performance.now();
    await setupOPFS();
    LibGit2Module._init();
    setup.init = performance.now() - t3;

    const repos = await listRepositories();
    showFiles(repos);

    const t4 = performance.now();
    LibGit2Module._mount_opfs_in_thread();
    setup.mount = await waitForMount() - t4;
    bench.setup = setup;
    console.table(bench.setup);
})();


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

/**
 * Function allowing the user to download a .csv of the benchmarked values.
 *
 * @param bench benchmarking of libgit2 functionality on the repository
 */

function downloadCSV(bench) {
    const header_bench = "iteration,open_repo_ms,walk_commits_ms,get_commit_info_ms,diff_total_ms";
    const header_setup = "init, mount";
    const rows_bench = bench.runs.map((run, i) =>
        `${i},${run.openRepo.toFixed(3)},${run.walkCommits.toFixed(3)},${run.getCommitInfo.toFixed(3)},${run.diff.toFixed(3)}`
    );

    const rows_setup = `${bench.setup.init.toFixed(3)},${bench.setup.mount.toFixed(3)}`;

    const csv_bench = [header_bench, ...rows_bench].join("\n");
    const blob_bench = new Blob([csv_bench], { type: "text/csv" });
    const url_bench = URL.createObjectURL(blob_bench);

    const a_bench = document.createElement("a");
    a_bench.href = url_bench;
    a_bench.download = "bench_results.csv";
    a_bench.click();
    URL.revokeObjectURL(url_bench);

    const csv_setup = [header_setup, rows_setup].join("\n");
    const blob_setup = new Blob([csv_setup], { type: "text/csv" });
    const url_setup = URL.createObjectURL(blob_setup);

    const a_setup = document.createElement("a");
    a_setup.href = url_setup;
    a_setup.download = "setup_results.csv";
    a_setup.click();
    URL.revokeObjectURL(url_setup);
}

export async function runBenchmark(repoPath, iterations = 100) {
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

        const t3 = performance.now();
        const commitCount = LibGit2Module.ccall("commit_count", "int", [], []);
        for (let c = 0; c < commitCount; c++) {

            const diffPtr = LibGit2Module.ccall("get_commit_diff", "number", ["int"], [c]);
            LibGit2Module._free(diffPtr);

        }
        run.diff = performance.now() - t3;
        bench.runs.push(run);
    }
    downloadCSV(bench);
    return bench;
}







