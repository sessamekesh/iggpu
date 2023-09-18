/**
 * Minimal web server implementation - serve up files from a directory
 * Inspired by: https://developer.mozilla.org/en-US/docs/Learn/Server-side/Node_server_without_framework
 * 
 * Attach headers so that SharedArrayBuffer can be used on Firefox:
 * https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer/Planned_changes
 *
 * To run: From root directory of intended project, run `node <path_to_this_script>`
 */

const http = require('http');
const fs = require('fs/promises');
const path = require('path');

/**
 * @param {http.ServerResponse<http.IncomingMessage>} response
 */
async function respondWith404Error(response) {
    try {
        const e404 = await fs.readFile('./404.html');
        response.writeHead(404, { 'Content-Type': 'text/html' });
        response.end(e404, 'utf-8');
    } catch (e) {
        response.writeHead(404);
        response.end("NOT FOUND", 'utf-8');
    }
}

/**
 * @param {http.ServerResponse<http.IncomingMessage>} response
 * @param {string} dirname
 */
async function respondWithDir(response, dirname) {
    const dir = await fs.readdir(dirname, { withFileTypes: true });

    let html = `<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>Index of ${dirname}</title></head><body><h1>Index of ${dirname}</h1>`;

    html += '<table>';
    dir.forEach((dirent) => {
        html += `<tr><td><a href="http://localhost:8000/${dirname.substring(2) + "/" + encodeURIComponent(dirent.name)}">${dirent.name}</a></td></tr>`;
    });

    html += '</table>'

    if (dirname.length > 2) {
        console.log(dirname.split('/'));
        console.log(dirname.split('/').slice(1, -1));
        console.log(dirname.split('/').slice(1, -1).join('/'));
        html += `<a href="http://localhost:8000/${dirname.split('/').slice(1, -1).join('/')}">..</a>`;
    }

    html += '</body ></html > ';

    response.writeHead(200, {
        'Content-Type': 'text/html',
        // Headers for SharedArrayBuffer
        'Cross-Origin-Opener-Policy': 'same-origin',
        'Cross-Origin-Embedder-Policy': 'require-corp',
    });
    response.end(html, 'utf-8');
}

http.createServer(async (request, response) => {
    console.log('request ', request.url);

    let filePath = '.' + request.url;

    const extname = String(path.extname(filePath)).toLowerCase();
    const mimeTypes = {
        '.txt': 'text/plain',
        '.inl': 'text/plain',
        '.h': 'text/plain',
        '.cc': 'text/plain',
        '.md': 'text/plain',
        '': 'text/plain',
        '.html': 'text/html',
        '.js': 'text/javascript',
        '.css': 'text/css',
        '.json': 'application/json',
        '.png': 'image/png',
        '.jpg': 'image/jpg',
        '.gif': 'image/gif',
        '.svg': 'image/svg+xml',
        '.wav': 'audio/wav',
        '.mp4': 'video/mp4',
        '.woff': 'application/font-woff',
        '.ttf': 'application/font-ttf',
        '.eot': 'application/vnd.ms-fontobject',
        '.otf': 'application/font-otf',
        '.wasm': 'application/wasm',
    };

    try {
        if (filePath.endsWith('/') && (await fs.stat(filePath + 'index.html')).isFile()) {
            filePath = filePath + 'index.html';
        }
    }
    catch (e) { }

    const contentType = mimeTypes[extname] || 'application/octet-stream';

    try {
        const stat = await fs.stat(filePath);
        if (stat.isFile()) {
            const content = await fs.readFile(filePath);
            response.writeHead(200, {
                'Content-Type': contentType,
                // Headers for SharedArrayBuffer
                'Cross-Origin-Opener-Policy': 'same-origin',
                'Cross-Origin-Embedder-Policy': 'require-corp',
            });
            response.end(content, 'utf-8');
        } else if (stat.isDirectory()) {
            return respondWithDir(response, filePath);
        } else {
            return respondWith404Error(response);
        }
    } catch (e) {
        return respondWith404Error(response);
    }
}).listen(8000);
console.log('Server running at http://localhost:8000');