import {DIRECTORY} from "../main.js";


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

export async function showExistingFiles() {
    const directoryHandle = await (await navigator.storage.getDirectory()).getDirectoryHandle(DIRECTORY);
    const fileTable = document.getElementById('file-table');
    if (directoryHandle.keys().length !== 0) {
        const h2 = document.createElement('h2');
        h2.id = "file-table-h2"
        h2.className = "subheading"
        h2.textContent = "Uploaded files";
        fileTable.appendChild(h2);

        for await (let name of directoryHandle.keys()) {
            const div = document.createElement('div');
            div.className = "file-table-file";
            const p = document.createElement('p');
            p.textContent = name;
            div.appendChild(p);
            const button = document.createElement('button');
            button.textContent = "select";
            button.onclick = () => openRepo(name);

            div.appendChild(button);
            fileTable.appendChild(div);
        }
    }
}


