/**
 * opfsStorage.js
 *
 * Handles interaction with the Origin Private File System (OPFS).
 * This module is responsible for:
 *  - uploading files and directory structures into OPFS
 *  - listing already uploaded repositories
 *
 * This file does NOT interact with WebAssembly or libgit2 directly.
 */

/**
 * Name of the directory inside OPFS where repositories are stored.
 */
import { DIRECTORY } from "../config.js";

/**
 * Uploads one or more files (or an entire directory) selected by the user
 * into the OPFS repository directory.
 *
 * The directory structure of the uploaded files is preserved using
 * `webkitRelativePath`.
 */
export async function uploadFile() {
    const input = document.getElementById("upload");
    const files = Array.from(input.files);

    const directoryHandle = await (await navigator.storage.getDirectory()).getDirectoryHandle(DIRECTORY, {create: true});

    for (const file of files) {
        // file.webkitRelativePath gives the path relative to the selected folder
        const relativePath = file.webkitRelativePath;
        const parts = relativePath.split("/").filter(Boolean);

        let currentDir = directoryHandle;
        for (let i = 0; i < parts.length - 1; i++) {
            currentDir = await currentDir.getDirectoryHandle(parts[i], {create: true});
        }

        const fileName = parts[parts.length - 1];
        const fileHandle = await currentDir.getFileHandle(fileName, {create: true});
        const writable = await fileHandle.createWritable();
        await writable.write(await file.arrayBuffer());
        await writable.close();
    }

    console.log("Directory uploaded to OPFS:", DIRECTORY);
    window.location.reload();
}



/**
 * Returns a list of existing repositories stored in OPFS.
 *
 * Each key in the top level directory is added to the list.
 */
export async function listRepositories() {
    const root = await navigator.storage.getDirectory();
    const repoDir = await root.getDirectoryHandle(DIRECTORY, { create: false });

    const repos = [];
    for await (const name of repoDir.keys()) {
        repos.push(name);
    }
    return repos;
}


