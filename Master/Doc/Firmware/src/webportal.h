// webportal.h
// Ensure variable name is 'html' so main.cpp can use it
const char *html = R"rawliteral(
<!doctype html>
<html>

<head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width,initial-scale=1" />
    <title>ESP MASTER - Control</title>
    <script src="https://cdn.tailwindcss.com"></script>
    <style>
        body {
            font-family: 'Inter', sans-serif;
            background: #f4f7f9;
            margin: 0;
            padding: 10px;
        }

        .header {
            display: flex;
            align-items: center;
            justify-content: space-between;
            margin-bottom: 16px;
            padding: 0 10px;
        }

        .btn {
            padding: 8px 14px;
            border-radius: 6px;
            cursor: pointer;
            border: none;
            font-weight: 600;
            transition: background-color 0.2s, transform 0.1s;
            box-shadow: 0 1px 3px rgba(0, 0, 0, 0.1);
        }

        .btn:active {
            transform: translateY(1px);
        }

        .btn-main {
            background: #10b981;
            color: #fff;
        }

        .btn-main:hover {
            background: #10b981;
        }

        .btn-toggle {
            background: #e5e7eb;
            color: #4b5563;
        }

        .btn-toggle.on {
            background: #10b981;
            color: #fff;
        }

        .card {
            background: white;
            border-radius: 12px;
            box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1), 0 2px 4px -2px rgba(0, 0, 0, 0.1);
            padding: 20px;
        }

        .node-card {
            border-left: 5px solid;
            transition: border-left-color 0.3s;
        }

        .node-card.online {
            border-left-color: #10b981;
        }

        .node-card.offline {
            border-left-color: #f87171;
        }

        .slider-container {
            display: flex;
            align-items: center;
            gap: 10px;
            margin-top: 10px;
        }

        .slider-container input[type="range"] {
            flex-grow: 1;
            -webkit-appearance: none;
            height: 8px;
            background: #e0e0e0;
            border-radius: 5px;
        }

        .slider-container input[type="range"]::-webkit-slider-thumb {
            -webkit-appearance: none;
            appearance: none;
            width: 16px;
            height: 16px;
            background: #10b981;
            border-radius: 50%;
            cursor: pointer;
        }

        .modal-content {
            background: #fff;
            margin: 10% auto;
            padding: 25px;
            border-radius: 12px;
            width: 90%;
            max-width: 400px;
            box-shadow: 0 8px 16px rgba(0, 0, 0, 0.2);
            animation: fadeIn 0.3s;
        }

        @keyframes fadeIn {
            from {
                opacity: 0;
                transform: translateY(-10px);
            }

            to {
                opacity: 1;
                transform: translateY(0);
            }
        }

        .grid-container {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
            gap: 16px;
            padding: 0 10px;
        }

        .relay-grid {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 12px;
            margin-top: 10px;
        }

        .btn-relay-on {
            background: #10b981;
            color: #fff;
        }

        .btn-relay-off {
            background: #9ca3af;
            color: #fff;
        }
    </style>
</head>

<body class="bg-gray-50 min-h-screen">

    <div class="header">
        <h1 class="text-3xl font-extrabold text-gray-800">ESP Master Gateway</h1>
        <div class="flex flex-col md:flex-row items-end md:items-center gap-3">
            <p id="rtc_time" class="text-gray-500 font-medium text-sm"></p>
            <button onclick="openAddNodeModal()" class="btn btn-main">ADD NODE</button>
            <button onclick="openWifiModal()" class="btn btn-main bg-yellow-600 hover:bg-yellow-700">Wi-Fi
                Setting</button>
        </div>
    </div>

    <div class="grid-container">
        <div class="card">
            <h2 class="text-xl font-semibold text-gray-700 mb-4">Local Relays (RL1-RL4)</h2>
            <div id="local-relays" class="relay-grid">
            </div>
            <div class="flex flex-col gap-3 mt-4">
                <button id="local-toggle-all" class="btn bg-purple-500 text-white hover:bg-purple-600">TOGGLE ALL
                    RL</button>
                <button id="master-toggle" class="btn btn-toggle transition duration-300 w-full">TOGGLE ALL
                    NODE</button>
            </div>
        </div>
    </div>

    <h2 class="text-2xl font-bold text-gray-800 mt-6 mb-4 px-3">Remote Nodes</h2>

    <div id="nodes-container" class="grid-container">
    </div>
    <p id="no-nodes" class="text-gray-500 mt-4 px-3 hidden">No nodes configured yet.</p>

    <div class="footer text-center mt-8 text-gray-500 text-xs">
        <p>ESP Master Gateway Control Panel</p>
        <p>Auto-refresh status every 3s.</p>
    </div>

    <div id="wifiModal"
        style="display:none; position:fixed; top:0; left:0; width:100%; height:100%; background:rgba(0,0,0,0.6); z-index:101; padding:10px;">
        <div class="modal-content">
            <h3 class="text-2xl font-bold mb-4">Wi-Fi Station Settings</h3>
            <p class="text-sm text-gray-600 mb-4">Set up the Gateway to connect to a Home Wi-Fi Network.</p>
            <div class="mb-4">
                <label for="wifi-ssid" class="block text-sm font-medium text-gray-700">SSID:</label>
                <input type="text" id="wifi-ssid" class="mt-1 p-2 w-full border rounded-md"
                    placeholder="Your Wi-Fi Name" required>
            </div>
            <div class="mb-6">
                <label for="wifi-password" class="block text-sm font-medium text-gray-700">Password:</label>
                <input type="password" id="wifi-password" class="mt-1 p-2 w-full border rounded-md"
                    placeholder="Your Wi-Fi Password" required>
            </div>
            <div class="flex justify-end gap-3">
                <button onclick="closeWifiModal()"
                    class="btn bg-gray-300 hover:bg-gray-400 text-gray-800">Cancel</button>
                <button id="wifi-modal-save" onclick="saveWifiConfig()"
                    class="btn btn-main bg-green-500 hover:bg-green-600">Connect & Save</button>
            </div>
        </div>
    </div>

    <div id="nodeModal"
        style="display:none; position:fixed; top:0; left:0; width:100%; height:100%; background:rgba(0,0,0,0.6); z-index:102; padding:10px;">
        <div class="modal-content">
            <h3 id="node-modal-title" class="text-2xl font-bold mb-4">Add New Node</h3>
            <input type="hidden" id="node-modal-id-hidden">
            <div class="mb-4">
                <label for="node-modal-id" class="block text-sm font-medium text-gray-700">Node ID
                    (1-%TOTAL_SLAVE%):</label>
                <input type="number" id="node-modal-id" min="1" max="%TOTAL_SLAVE%"
                    class="mt-1 p-2 w-full border rounded-md" placeholder="Enter unique ID" required>
            </div>
            <div class="mb-6">
                <label for="node-modal-label" class="block text-sm font-medium text-gray-700">Node Label/Name:</label>
                <input type="text" id="node-modal-label" class="mt-1 p-2 w-full border rounded-md"
                    placeholder="e.g., Living Room Light" required>
            </div>
            <div class="flex justify-end gap-3">
                <button onclick="closeNodeModal()"
                    class="btn bg-gray-300 hover:bg-gray-400 text-gray-800">Cancel</button>
                <button id="node-modal-save" onclick="saveNodeConfigFromModal()" class="btn btn-main">Save Node</button>
            </div>
        </div>
    </div>

    <script>
        const TOTAL_SLAVE = % TOTAL_SLAVE %;
        const nodesContainer = document.getElementById('nodes-container');
        const localRelaysContainer = document.getElementById('local-relays');
        const masterToggleBtn = document.getElementById('master-toggle');
        let localRelayStates = [false, false, false, false];

        function sendApiRequest(url, method = 'GET', body = null) {
            const options = {
                method: method,
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: body
            };
            return fetch(url, options)
                .then(response => {
                    if (!response.ok) {
                        return response.json().catch(() => ({ ok: false, err: 'Network error or Invalid JSON' }));
                    }
                    return response.json();
                })
                .catch(error => {
                    console.error('API Error:', error);
                    return { ok: false, err: error.message };
                });
        }

        const localToggleAllBtn = document.getElementById('local-toggle-all');
        localToggleAllBtn.onclick = () => {
            const shouldTurnOn = localRelayStates.some(r => !r);
            sendApiRequest(`/api/local/toggleAll?state=${shouldTurnOn ? 'on' : 'off'}`)
                .then(json => {
                    if (json.ok) refreshStatus();
                    else alert('Failed to toggle all local relays: ' + json.err);
                });
        };

        function toggleLocalRelay(index, state) {
            const url = `/api/local/toggle?relay=${index + 1}&state=${state ? 'on' : 'off'}`;
            sendApiRequest(url)
                .then(json => {
                    if (json.ok) {
                        refreshStatus();
                    } else {
                        alert('Failed to control relay: ' + json.err);
                    }
                });
        }

        function updateLocalRelayDisplay() {
            localRelaysContainer.innerHTML = '';
            for (let i = 0; i < 4; i++) {
                const isCurrentlyOn = localRelayStates[i];
                const btn = document.createElement('button');
                btn.className = `btn ${isCurrentlyOn ? 'btn-relay-on' : 'btn-relay-off'} w-full`;
                btn.textContent = `RL${i + 1} ${isCurrentlyOn ? 'ON' : 'OFF'}`;
                btn.onclick = () => toggleLocalRelay(i, !isCurrentlyOn);
                localRelaysContainer.appendChild(btn);
            }
        }

        function toggleNode(id, state) {
            sendApiRequest(`/api/node/toggle?node=${id}&state=${state ? 'on' : 'off'}`)
                .then(json => {
                    if (!json.ok) {
                        alert('Failed to toggle node: ' + json.err);
                        return;
                    }
                    if (state) {
                        const slider = document.getElementById(`slider-${id}`);
                        const value = slider ? parseInt(slider.value) : 100;
                        sendApiRequest(`/api/node/pwm?node=${id}&value=${value}`)
                            .then(pwmJson => {
                                if (pwmJson.ok) {
                                    refreshStatus();
                                } else {
                                    alert('Failed to set PWM: ' + pwmJson.err);
                                }
                            });
                    } else {
                        refreshStatus();
                    }
                });
        }

        function setNodePwm(id, value) {
            const webValue = parseInt(value);
            if (isNaN(webValue) || webValue < 0 || webValue > 100) {
                alert("PWM must be between 0-100%");
                return;
            }
            sendApiRequest(`/api/node/pwm?node=${id}&value=${webValue}`)
                .then(json => {
                    if (json.ok) refreshStatus();
                    else alert('Failed to set PWM: ' + json.err);
                });
        }

        function removeNode(id) {
            if (!confirm(`Are you sure you want to remove Node ID ${id}?`)) return;
            sendApiRequest(`/api/node/remove?node=${id}`, 'POST')
                .then(json => {
                    if (json.ok) refreshStatus();
                    else alert('Failed to remove node: ' + json.err);
                });
        }

        masterToggleBtn.onclick = () => {
            const currentState = masterToggleBtn.textContent.includes('OFF');
            const newState = currentState ? 'on' : 'off';
            sendApiRequest(`/api/master/toggleAll?state=${newState}`)
                .then(json => {
                    if (json.ok) refreshStatus();
                    else alert('Failed to toggle all: ' + json.err);
                });
        }

        function openNodeModal(node = null) {
            const modal = document.getElementById('nodeModal');
            const title = document.getElementById('node-modal-title');
            const idInput = document.getElementById('node-modal-id');
            const labelInput = document.getElementById('node-modal-label');
            const idHidden = document.getElementById('node-modal-id-hidden');

            idInput.value = node ? node.id : '';
            labelInput.value = node ? node.label : '';
            idHidden.value = node ? node.id : '';

            if (node) {
                title.textContent = `Edit Node ID ${node.id}`;
                idInput.disabled = true;
            } else {
                title.textContent = 'Add New Node';
                idInput.disabled = false;
            }
            modal.style.display = 'block';
        }

        function openAddNodeModal() {
            openNodeModal(null);
        }

        function closeNodeModal() {
            document.getElementById('nodeModal').style.display = 'none';
        }

        function saveNodeConfigFromModal() {
            const id = document.getElementById('node-modal-id').value;
            const label = document.getElementById('node-modal-label').value;
            const oldId = document.getElementById('node-modal-id-hidden').value;

            if (!id || !label) {
                alert("Please fill in both ID and Label.");
                return;
            }
            if (id < 1 || id > TOTAL_SLAVE) {
                alert(`ID must be between 1 and ${TOTAL_SLAVE}`);
                return;
            }

            const body = `id=${id}&label=${encodeURIComponent(label)}`;
            let url = `/api/node/add`;

            if (oldId && oldId == id) {
                url = `/api/node/edit`;
                const bodyEdit = `node=${id}&name=${encodeURIComponent(label)}`;
                sendApiRequest(url, 'POST', bodyEdit).then(json => {
                    if (json.ok) {
                        closeNodeModal();
                        refreshStatus();
                    } else {
                        alert('Failed to edit node: ' + json.err);
                    }
                });
                return;
            }

            sendApiRequest(url, 'POST', body).then(json => {
                if (json.ok) {
                    closeNodeModal();
                    refreshStatus();
                } else {
                    alert('Failed to add node: ' + json.err);
                }
            });
        }

        function openWifiModal() {
            document.getElementById('wifiModal').style.display = 'block';
        }

        function closeWifiModal() {
            document.getElementById('wifiModal').style.display = 'none';
        }

        function saveWifiConfig() {
            const ssid = document.getElementById('wifi-ssid').value;
            const password = document.getElementById('wifi-password').value;

            if (!ssid) {
                alert("SSID cannot be empty.");
                return;
            }

            const body = `ssid=${encodeURIComponent(ssid)}&password=${encodeURIComponent(password)}`;
            sendApiRequest(`/api/wifi/config`, 'POST', body)
                .then(json => {
                    if (json.ok) {
                        alert(json.msg + "\nWhen connected successfully, you can access via the new IP displayed on ESP32 screen.");
                        closeWifiModal();
                    } else {
                        alert('Failed to save Wi-Fi settings: ' + json.err);
                    }
                });
        }

        function renderNodeCard(node) {
            const state = node.relay ? 'ON' : 'OFF';
            const connection = node.online ? 'Online' : 'Offline';
            const statusClass = node.online ? 'online' : 'offline';
            const relayBtnClass = node.relay ? 'btn-relay-on' : 'btn-relay-off';
            const escapedLabel = node.label.replace(/'/g, "\\'");
            let html = `
    <div class="card node-card ${statusClass}">
        <div class="flex justify-between items-center mb-3">
            <h4 class="text-lg font-bold text-gray-800">${node.label} (ID: ${node.id})</h4>
            <span class="text-sm font-semibold p-1 rounded-full ${node.online ? 'bg-green-100 text-green-700' : 'bg-red-100 text-red-700'}">${connection}</span>
        </div>
        
        <p class="text-gray-600 mb-2 text-sm">Temp: <span class="font-medium">${node.temperature}°C</span> | Time: <span class="font-medium">${node.timeString}</span></p>

        <div class="flex items-center justify-between mt-3 mb-2">
            <p class="text-gray-700 font-semibold text-sm">Relay Status:</p>
            <button onclick="toggleNode(${node.id}, ${!node.relay})" 
                    class="btn ${relayBtnClass}">
                ${state}
            </button>
        </div>

        <div class="slider-container">
            <p class="text-gray-700 font-semibold text-sm">PWM (0-100%):</p>
            <input id="slider-${node.id}" type="range" min="0" max="100" value="${node.sliderValue}" 
                    onchange="setNodePwm(${node.id}, this.value)" 
                    oninput="document.getElementById('pwm-value-${node.id}').textContent = this.value">
            <span id="pwm-value-${node.id}" class="font-bold text-lg text-indigo-600 w-10 text-right">${node.sliderValue}</span>%
        </div>

        <div class="flex gap-3 mt-4">
            <button onclick="openNodeModal({id: ${node.id}, label: '${escapedLabel}'})" 
                    class="btn bg-blue-500 text-white hover:bg-blue-600 flex-1">
                Edit Name
            </button>
            <button onclick="removeNode(${node.id})" 
                    class="btn bg-red-500 text-white hover:bg-red-600">
                Remove
            </button>
        </div>
    </div>
    `;
            return html;
        }

        function refreshStatus() {
            sendApiRequest('/api/status')
                .then(data => {
                    if (!data.ok) {
                        console.error("Failed to fetch status:", data.err);
                        return;
                    }
                    if (data.localRelays && data.localRelays.length === 4) {
                        localRelayStates = data.localRelays;
                        updateLocalRelayDisplay();
                    }
                    nodesContainer.innerHTML = '';
                    if (data.nodes.length === 0) {
                        document.getElementById('no-nodes').classList.remove('hidden');
                    } else {
                        document.getElementById('no-nodes').classList.add('hidden');
                        data.nodes.forEach(node => {
                            nodesContainer.innerHTML += renderNodeCard(node);
                        });
                    }

                    const allNodesOff = data.nodes.every(n => !n.relay);
                    masterToggleBtn.textContent = allNodesOff ? 'TURN ALL ON' : 'TURN ALL OFF';

                    if (allNodesOff) {
                        masterToggleBtn.classList.add('btn-toggle');
                        masterToggleBtn.classList.remove('bg-green-500', 'text-white');
                    } else {
                        masterToggleBtn.classList.remove('btn-toggle');
                        masterToggleBtn.classList.add('bg-green-500', 'text-white');
                    }
                });
        }

        function updateClock() {
            fetch(`/getTime`)
                .then(response => response.json())
                .then(data => {
                    let timeString = `RTC Time: ${data.day.toString().padStart(2, '0')}/${data.month.toString().padStart(2, '0')}/${data.year} ${data.hour.toString().padStart(2, '0')}:${data.minute.toString().padStart(2, '0')}:${data.second.toString().padStart(2, '0')}`;
                    document.getElementById("rtc_time").innerText = timeString;
                })
                .catch(err => console.error('Clock update error:', err));
        }

        setInterval(updateClock, 1000);
        setInterval(refreshStatus, 3000);
        updateClock();
        refreshStatus();
        updateLocalRelayDisplay();

    </script>
</body>

</html>
)rawliteral";