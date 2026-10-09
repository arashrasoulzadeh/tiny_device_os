const API_BASE = '/api';
let eventSources = [];
let currentScreen = 'devices';

async function apiGet(path) {
    const res = await fetch(`${API_BASE}${path}`);
    if (!res.ok) throw new Error(`${res.status}: ${await res.text()}`);
    return res.json();
}

async function apiPost(path, body) {
    const res = await fetch(`${API_BASE}${path}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
    });
    if (!res.ok) throw new Error(`${res.status}: ${await res.text()}`);
    return res.json();
}

function showScreen(name) {
    document.querySelectorAll('.screen').forEach(s => {
        s.classList.add('hidden');
        s.classList.remove('active');
    });
    const target = document.getElementById(`screen-${name}`);
    target.classList.remove('hidden');
    target.classList.add('active');
    document.querySelectorAll('.nav-btn').forEach(b => b.classList.remove('active'));
    document.getElementById(`nav-${name}`).classList.add('active');
    currentScreen = name;
    loadScreen(name);
}

async function loadScreen(name) {
    switch (name) {
        case 'devices': await loadDevices(); break;
        case 'device': await loadDeviceInfo(); break;
        case 'image': await loadImage(); break;
        case 'job': await loadJob(); break;
        case 'console': await loadConsole(); break;
    }
}

function el(html) {
    const t = document.createElement('template');
    t.innerHTML = html.trim();
    return t.content.firstChild;
}

async function loadDevices() {
    try {
        const data = await apiGet('/devices');
        const config = data.config || {};
        const device = config.device || {};
        const ports = await apiGet('/ports');
        const container = document.getElementById('devices-list');
        container.innerHTML = '';
        
        if (ports.length === 0) {
            container.appendChild(el(`
                <div class="card warning">
                    <h3>No USB serial ports detected</h3>
                    <p>The CH340 device is not visible to the OS right now.</p>
                    <p>Unplug the NodeMCU, wait 2s, plug into a Mac USB port (not only a hub), then refresh.</p>
                </div>
            `));
        } else {
            ports.forEach(port => {
                const isActive = port === data.port;
                container.appendChild(el(`
                    <div class="card ${isActive ? 'active' : ''}">
                        <div class="port-info">
                            <strong>${port}</strong>
                            ${isActive ? '<span class="badge">active</span>' : ''}
                        </div>
                        <div class="port-actions">
                            ${!isActive ? `<button class="btn" onclick="setPort('${port}')">Use this port</button>` : ''}
                        </div>
                    </div>
                `));
            });
        }
        
        if (data.pin_warnings?.length) {
            const warnDiv = document.createElement('div');
            warnDiv.className = 'card warning';
            warnDiv.innerHTML = '<h4>Pin Conflicts</h4><ul>' + 
                data.pin_warnings.map(w => `<li>${w}</li>`).join('') + '</ul>';
            container.appendChild(warnDiv);
        }
    } catch (e) {
        console.error(e);
        document.getElementById('devices-list').innerHTML = `<div class="card error">Error: ${e.message}</div>`;
    }
}

async function setPort(port) {
    try {
        await apiPost('/device/port', { port });
        loadDevices();
    } catch (e) {
        alert(e.message);
    }
}

async function loadDeviceInfo() {
    try {
        const data = await apiGet('/devices');
        const config = data.config || {};
        const device = config.device || {};
        const lcd = config.lcd || {};
        const inputs = config.inputs || {};
        const container = document.getElementById('device-info');
        
        container.innerHTML = `
            <div class="card">
                <h3>Device Identity</h3>
                <table class="info-table">
                    <tr><th>Name</th><td>${device.name || 'nodemcu'}</td></tr>
                    <tr><th>Arch</th><td>${device.arch || 'esp8266'}</td></tr>
                    <tr><th>Board</th><td>${device.board || 'nodemcu'}</td></tr>
                    <tr><th>PIO Env</th><td>${device.pio_env || 'nodemcu'}</td></tr>
                    <tr><th>Port</th><td>${data.port || '(none)'} <span class="badge">${data.port_status || 'missing'}</span></td></tr>
                    <tr><th>Baud</th><td>${device.baud || 115200}</td></tr>
                    <tr><th>Flash Baud</th><td>${device.flash_baud || 460800}</td></tr>
                    <tr><th>Flash Size</th><td>${device.flash_size || '4MB'}</td></tr>
                    <tr><th>Flash Mode</th><td>${device.flash_mode || 'dio'}</td></tr>
                </table>
            </div>
            <div class="card">
                <h3>LCD</h3>
                <table class="info-table">
                    <tr><th>Type</th><td>${lcd.type || 'ssd1306'}</td></tr>
                    <tr><th>Size</th><td>${lcd.width || 128}x${lcd.height || 32}</td></tr>
                    <tr><th>Bus</th><td>${lcd.bus || 'i2c'}</td></tr>
                    <tr><th>I2C Address</th><td>0x${(lcd.address || 0x3C).toString(16)}</td></tr>
                    <tr><th>SCL Pin</th><td>${lcd.scl || 'D1'} (GPIO${resolvePin(lcd.scl || 'D1')})</td></tr>
                    <tr><th>SDA Pin</th><td>${lcd.sda || 'D2'} (GPIO${resolvePin(lcd.sda || 'D2')})</td></tr>
                </table>
            </div>
            <div class="card">
                <h3>Buttons</h3>
                <table class="info-table">
                    <thead><tr><th>Name</th><th>Pin</th><th>GPIO</th><th>Active Low</th><th>Pull</th></tr></thead>
                    <tbody>
                        ${(inputs.buttons || []).map(btn => `
                            <tr>
                                <td>${btn.name}</td>
                                <td>${btn.pin}</td>
                                <td>GPIO${resolvePin(btn.pin)}</td>
                                <td>${btn.active_low ? 'Yes' : 'No'}</td>
                                <td>${btn.pull || 'up'}</td>
                            </tr>
                        `).join('')}
                    </tbody>
                </table>
            </div>
            <div class="card">
                <h3>Secrets Status</h3>
                <table class="info-table">
                    <tr><th>Secrets File</th><td>${data.secrets_present ? 'Present' : 'Missing'}</td></tr>
                </table>
            </div>
            <div class="card">
                <h3>Toolchain</h3>
                <div id="toolchain-status">Loading...</div>
            </div>
        `;
        
        const health = await apiGet('/health');
        document.getElementById('toolchain-status').innerHTML = `
            <table class="info-table">
                <tr><th>PlatformIO</th><td>${health.platformio_found ? 'Found' : 'Not found'}</td></tr>
                <tr><th>PlatformIO Path</th><td>${health.platformio_path || 'N/A'}</td></tr>
                <tr><th>Apple Silicon</th><td>${health.apple_silicon ? 'Yes' : 'No'}</td></tr>
                <tr><th>Rosetta</th><td>${health.rosetta_available ? 'Available' : 'Not available'}</td></tr>
                <tr><th>XTENSA Toolchain</th><td>${health.xtensa_toolchain_ok ? 'OK' : 'Error'}</td></tr>
                ${health.xtensa_toolchain_error ? `<tr><th>Error</th><td class="error">${health.xtensa_toolchain_error}</td></tr>` : ''}
            </table>
        `;
    } catch (e) {
        console.error(e);
        document.getElementById('device-info').innerHTML = `<div class="card error">Error: ${e.message}</div>`;
    }
}

function resolvePin(label) {
    const map = { 'D0': 16, 'D1': 5, 'D2': 4, 'D3': 0, 'D4': 2, 'D5': 14, 'D6': 12, 'D7': 13, 'D8': 15, 'A0': 17, 'RX': 3, 'TX': 1 };
    if (!label) return '?';
    if (label.startsWith('GPIO')) return label.slice(4);
    if (/^\d+$/.test(label)) return label;
    return map[label.toUpperCase()] || '?';
}

async function loadImage() {
    try {
        const [appsData, featuresData, mainAppData, sizeData] = await Promise.all([
            apiGet('/apps'),
            apiGet('/features'),
            apiGet('/main-app'),
            apiGet('/size')
        ]);
        
        // Apps
        const appsContainer = document.getElementById('apps-list');
        appsContainer.innerHTML = '';
        appsData.apps.forEach(app => {
            const depends = app.depends?.length ? ` (depends: ${app.depends.join(', ')})` : '';
            const minDisp = app.min_display;
            const tooSmall = minDisp && minDisp.width > 0 && minDisp.height > 0 && 
                (minDisp.width > 128 || minDisp.height > 64);
            appsContainer.appendChild(el(`
                <label class="app-row ${app.enabled ? '' : 'disabled'}${tooSmall ? ' too-small' : ''}">
                    <input type="checkbox" ${app.enabled ? 'checked' : ''} 
                           ${tooSmall ? 'disabled' : ''} 
                           onchange="toggleApp('${app.name}', this.checked)">
                    <div class="app-info">
                        <strong>${app.title}</strong> (${app.name})
                        <span class="app-version">v${app.version}</span>
                        ${app.description ? `<span class="app-desc">${app.description}</span>` : ''}
                        ${depends ? `<span class="app-deps">${depends}</span>` : ''}
                        ${tooSmall ? `<span class="badge warning">Display too small (needs ${minDisp.width}x${minDisp.height})</span>` : ''}
                        ${app.exclude_reason ? `<span class="error">${app.exclude_reason}</span>` : ''}
                    </div>
                </label>
            `));
        });
        
        // Main App
        const mainAppSel = document.getElementById('main-app-select');
        mainAppSel.innerHTML = appsData.apps
            .filter(a => a.enabled)
            .map(a => `<option value="${a.name}" ${a.name === mainAppData.main_app ? 'selected' : ''}>${a.title}</option>`)
            .join('');
        
        // Features
        const featContainer = document.getElementById('features-list');
        featContainer.innerHTML = '';
        featuresData.features.forEach(f => {
            if (f.type === 'bool') {
                featContainer.appendChild(el(`
                    <label class="feature-row ${f.wired ? '' : 'unwired'}">
                        <input type="checkbox" ${f.value ? 'checked' : ''} 
                               ${!f.wired ? 'disabled' : ''} 
                               onchange="toggleFeature('${f.symbol}', this.checked)">
                        <div class="feature-info">
                            <strong>${f.symbol}</strong>
                            <span class="feature-prompt">${f.prompt || ''}</span>
                            ${!f.wired ? '<span class="badge warning">Not wired (no compile effect)</span>' : ''}
                        </div>
                    </label>
                `));
            } else {
                featContainer.appendChild(el(`
                    <label class="feature-row ${f.wired ? '' : 'unwired'}">
                        <div class="feature-info">
                            <strong>${f.symbol}</strong>
                            <span class="feature-prompt">${f.prompt || ''}</span>
                            ${f.range ? `<span class="badge">Range: ${f.range}</span>` : ''}
                            ${!f.wired ? '<span class="badge warning">Not wired</span>' : ''}
                        </div>
                        <input type="number" value="${f.value}" 
                               ${!f.wired ? 'disabled' : ''} 
                               onchange="toggleFeature('${f.symbol}', this.value)">
                    </label>
                `));
            }
        });
        
        // Size
        const sizeContainer = document.getElementById('size-preview');
        if (sizeData.measured) {
            sizeContainer.innerHTML = `
                <div class="card">
                    <h4>Last Measured Size</h4>
                    <table class="info-table">
                        <tr><th>Total Flash</th><td>${sizeData.total_flash} bytes</td></tr>
                        <tr><th>Total RAM</th><td>${sizeData.total_ram} bytes</td></tr>
                        <tr><th>Kernel Flash</th><td>${sizeData.categories?.kernel?.flash || 0} bytes</td></tr>
                        <tr><th>Apps Flash</th><td>${Object.values(sizeData.categories?.apps || {}).reduce((s, a) => s + (a.flash || 0), 0)} bytes</td></tr>
                        <tr><th>Runtime Flash</th><td>${sizeData.categories?.runtime?.flash || 0} bytes</td></tr>
                    </table>
                    <p class="note">Data from last successful build</p>
                </div>
            `;
        } else {
            sizeContainer.innerHTML = `
                <div class="card">
                    <h4>Size Budget</h4>
                    <table class="info-table">
                        <tr><th>Flash Budget</th><td>${sizeData.budget_flash}</td></tr>
                    </table>
                    <p class="note">${sizeData.message || 'No measured size yet'}</p>
                </div>
            `;
        }
    } catch (e) {
        console.error(e);
        document.getElementById('apps-list').innerHTML = `<div class="card error">Error: ${e.message}</div>`;
    }
}

async function toggleApp(name, enabled) {
    try {
        const data = await apiGet('/apps');
        const apps = data.apps.filter(a => a.enabled).map(a => a.name);
        if (enabled) apps.push(name);
        else apps.splice(apps.indexOf(name), 1);
        await apiPost('/apps', { apps });
        loadImage();
    } catch (e) {
        alert(e.message);
    }
}

async function toggleFeature(symbol, value) {
    try {
        const data = await apiGet('/features');
        const features = {};
        data.features.forEach(f => { features[f.symbol] = f.value; });
        if (typeof value === 'boolean') features[symbol] = value;
        else features[symbol] = parseInt(value, 10) || 0;
        await apiPost('/features', { features });
        loadImage();
    } catch (e) {
        alert(e.message);
    }
}

async function saveMainApp() {
    const sel = document.getElementById('main-app-select');
    await apiPost('/main-app', { main_app: sel.value });
    loadImage();
}

async function loadJob() {
    try {
        const data = await apiGet('/jobs/current');
        const container = document.getElementById('job-status');
        if (data.state === 'idle') {
            container.innerHTML = '<div class="card"><p>No job running</p></div>';
            return;
        }
        container.innerHTML = `
            <div class="card">
                <h3>Job Status: <span class="badge ${data.state}">${data.state}</span></h3>
                <table class="info-table">
                    <tr><th>Port</th><td>${data.port || '(none)'}</td></tr>
                    <tr><th>Upload</th><td>${data.upload ? 'Yes' : 'No'}</td></tr>
                    <tr><th>Elapsed</th><td>${Math.round(data.elapsed || 0)}s</td></tr>
                    ${data.firmware_path ? `<tr><th>Firmware</th><td>${data.firmware_path}</td></tr>` : ''}
                    ${data.error ? `<tr><th class="error">Error</th><td class="error">${data.error}</td></tr>` : ''}
                </table>
                <div class="actions">
                    ${data.running ? '<button class="btn danger" onclick="cancelJob()">Cancel</button>' : ''}
                    ${data.state === 'flashed' ? '<button class="btn" onclick="showScreen(\'console\')">Open Console</button>' : ''}
                </div>
                <h4>Log</h4>
                <pre id="job-log" class="log"></pre>
            `;
        if (data.running) {
            streamJobLog();
        }
    } catch (e) {
        console.error(e);
    }
}

function streamJobLog() {
    const es = new EventSource('/api/jobs/current/log');
    eventSources.push(es);
    es.onmessage = (e) => {
        const data = JSON.parse(e.data);
        const log = document.getElementById('job-log');
        if (log) log.textContent += data.line + '\n';
    };
    es.onerror = () => es.close();
}

async function cancelJob() {
    try {
        await apiPost('/jobs/current/cancel', {});
        loadJob();
    } catch (e) {
        alert(e.message);
    }
}

async function startJob(upload = true) {
    try {
        await apiPost('/jobs', { upload });
        showScreen('job');
    } catch (e) {
        alert(e.message);
    }
}

async function loadConsole() {
    const container = document.getElementById('console-container');
    container.innerHTML = `
        <div class="card">
            <h3>Console</h3>
            <div class="actions">
                <button class="btn" onclick="openConsole()">Open Console</button>
                <button class="btn danger" id="close-console" disabled onclick="closeConsole()">Close Console</button>
            </div>
            <div id="console-output" class="log"></div>
            <div class="console-input">
                <input type="text" id="console-input" placeholder="Type command..." disabled onkeydown="if(event.key==='Enter')sendConsole()">
                <button class="btn" id="send-btn" disabled onclick="sendConsole()">Send</button>
            </div>
        </div>
    `;
}

async function openConsole() {
    try {
        await apiPost('/console', {});
        const input = document.getElementById('console-input');
        const btn = document.getElementById('send-btn');
        const closeBtn = document.getElementById('close-console');
        input.disabled = false;
        btn.disabled = false;
        closeBtn.disabled = false;
        
        const es = new EventSource('/api/console/output');
        eventSources.push(es);
        es.onmessage = (e) => {
            const data = JSON.parse(e.data);
            const out = document.getElementById('console-output');
            out.textContent += data.data;
            out.scrollTop = out.scrollHeight;
        };
        es.onerror = () => es.close();
    } catch (e) {
        alert(e.message);
    }
}

async function closeConsole() {
    try {
        await apiPost('/console/close', {});
        eventSources.forEach(es => es.close());
        eventSources = [];
        loadConsole();
    } catch (e) {
        alert(e.message);
    }
}

async function sendConsole() {
    const input = document.getElementById('console-input');
    const data = input.value + '\n';
    input.value = '';
    try {
        await apiPost('/console/input', { data });
    } catch (e) {
        alert(e.message);
    }
}

window.addEventListener('beforeunload', () => {
    eventSources.forEach(es => es.close());
});

// Initialize
document.addEventListener('DOMContentLoaded', () => {
    document.querySelectorAll('.nav-btn').forEach(btn => {
        btn.addEventListener('click', () => showScreen(btn.dataset.screen));
    });
    showScreen('devices');
});