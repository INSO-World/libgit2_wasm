import {uploadFile, showExistingFiles} from './opfs/fileAccess.js';
import createModule from '../wasm-build/libgit2_wasm.js'

const LibGit2Module = await createModule();
export const DIRECTORY = "repo";

window.uploadFile = uploadFile;
window.testWrite = testWrite;
window.showExistingFiles = showExistingFiles;
window.openRepo = openRepo;

async function setupOPFS() {
    const opfsRoot = await navigator.storage.getDirectory();
    console.log(opfsRoot);
    // Create a hierarchy of files and folders
    const directoryHandle = await opfsRoot.getDirectoryHandle(DIRECTORY, {
        create: true,
    });
    console.log(await opfsRoot.getDirectoryHandle(DIRECTORY));
}


function testWrite() {
    console.log("trying to write to OPFS");
    LibGit2Module.ccall(
        "test_write",
        "void",
        ["string", "string"],
        ["it works!!!!", "newFile"]
    )
}

//TODO  run in thread and test debugging methods (emscripten_console_log doesn't work)
export function openRepo(name) {
    console.log("opening repository: " + name + " ...");

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
        LibGit2Module._free();

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


setupOPFS().then(r => {
    showExistingFiles().then()
    LibGit2Module._init();
    LibGit2Module._mount_opfs_in_thread();
});









