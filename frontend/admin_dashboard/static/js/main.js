document.addEventListener('DOMContentLoaded', () => {
    lucide.createIcons();

    const UNHEALTHY_THRESHOLD_MS = 12 * 60 * 1000;
    const REFRESH_INTERVAL = 60000;
    const MAX_FIELDS_VISIBLE = 4;

    let currentUser = null;
    let isAdmin = false;
    let stations = [];
    let currentStationId = null;
    let refreshTimer = null;

    const FIELD_CONFIG = {
        'airTemperature': { name: 'Air Temp', unit: '°C' },
        'airHumidity': { name: 'Humidity', unit: '%' },
        'airPressure': { name: 'Pressure', unit: ' hPa' },
        'batteryV': { name: 'Battery', unit: ' V' },
        'solarV': { name: 'Solar', unit: ' V' },
        'windSpeed': { name: 'Wind', unit: ' m/s' },
        'windDirection': { name: 'Wind Dir', unit: '°' },
        'rainmm': { name: 'Rain', unit: ' mm' },
        'terrainTemperature': { name: 'Terrain Temp', unit: '°C' },
        'terrainHumidity': { name: 'Terrain Humidity', unit: '%' },
        'airCO2ppm': { name: 'CO2', unit: ' ppm' },
        'airNOXppm': { name: 'NOX', unit: ' ppm' },
        'waterTemperature': { name: 'Water Temp', unit: '°C' },
        'waterHumidity': { name: 'Water Humidity', unit: '%' },
        'waterPH': { name: 'Water pH', unit: '' },
        'batteryA': { name: 'Battery A', unit: ' A' },
        'solarA': { name: 'Solar A', unit: ' A' },
        'RSSI': { name: 'RSSI', unit: ' dBm' }
    };

    const tryParseSensorData = (msgText) => {
        if (!msgText) return { isSensorData: false, data: null };
        try {
            const parsed = JSON.parse(msgText);
            if (typeof parsed === 'object' && parsed !== null) {
                return { isSensorData: true, data: parsed };
            }
            return { isSensorData: false, data: msgText };
        } catch {
            return { isSensorData: false, data: msgText };
        }
    };

    // Escape untrusted strings before injecting into innerHTML (prevents stored XSS
    // via station_id / description / message payloads / usernames).
    const escapeHtml = (value) => {
        if (value === null || value === undefined) return '';
        return String(value)
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#39;');
    };

    // Login error box: hidden by default, shown only on failure.
    // Fixes the "empty pink box" glitch (div always rendered with .message-error).
    const showLoginMessage = (message) => {
        const el = document.getElementById('login-message');
        el.textContent = message;
        el.classList.remove('hidden');
    };

    const clearLoginMessage = () => {
        const el = document.getElementById('login-message');
        el.textContent = '';
        el.classList.add('hidden');
    };

    const formatSensorData = (data, maxFields = MAX_FIELDS_VISIBLE) => {
        if (!data || typeof data !== 'object') return '';
        
        const entries = Object.entries(data);
        if (entries.length === 0) return 'No sensor data';

        const parts = [];
        let hiddenParts = [];

        entries.forEach(([key, value]) => {
            const config = FIELD_CONFIG[key];
            if (config && value !== null && value !== undefined) {
                // Escape key-derived and value-derived content; config strings are trusted,
                // values come from station payloads and must not break out of HTML.
                const displayValue = typeof value === 'number' ? value.toFixed(1) : escapeHtml(value);
                const formatted = `${config.name}: ${displayValue}${config.unit}`;
                if (parts.length < maxFields) {
                    parts.push(formatted);
                } else {
                    hiddenParts.push(formatted);
                }
            }
        });

        let result = parts.join(' | ') || escapeHtml(JSON.stringify(data));
        
        if (hiddenParts.length > 0) {
            // No inline onclick: handled via delegated click on .sensor-more (CSP-friendly).
            result += ` <span class="sensor-more" role="button" tabindex="0" data-expandable>[+${hiddenParts.length} more]</span><span class="sensor-hidden" style="display:none;"> | ${hiddenParts.join(' | ')}</span>`;
        }

        return result;
    };

    const getCookie = (name) => {
        const match = document.cookie.match(new RegExp('(?:^|;\\s*)' + name + '=([^;]*)'));
        return match ? decodeURIComponent(match[1]) : null;
    };

    // Cookie-based auth: browser attaches HttpOnly access cookie automatically.
    // No token in localStorage/JS. Writes also send the CSRF token header.
    const apiRequest = async (endpoint, method = 'GET', body = null) => {
        const upperMethod = method.toUpperCase();
        const isWrite = !['GET', 'HEAD', 'OPTIONS'].includes(upperMethod);

        // Only send Content-Type when there is a JSON body (avoids needless preflights).
        const headers = { 'Accept': 'application/json' };
        if (body) headers['Content-Type'] = 'application/json';
        if (isWrite && endpoint !== '/api/login') {
            const csrf = getCookie('csrf_access_token');
            if (csrf) headers['X-CSRF-TOKEN'] = csrf;
        }

        const config = { method: upperMethod, headers, credentials: 'include' };
        if (body) config.body = JSON.stringify(body);

        try {
            const response = await fetch(endpoint, config);
            if (response.status === 401) {
                // Session expired/invalid (or not logged in). Don't loop on auth endpoints.
                if (endpoint !== '/api/login' && endpoint !== '/api/profile' && endpoint !== '/api/logout') {
                    clearSessionLocal();
                }
                return response; // let callers decide (login shows error, profile shows login)
            }
            return response;
        } catch (error) {
            console.error('API Error:', error);
            showToast('Network error occurred', 'error');
            return null;
        }
    };

    const showLogin = () => {
        document.getElementById('login-page').classList.remove('hidden');
        document.getElementById('dashboard-page').classList.add('hidden');
        stopAutoRefresh();
    };

    const showDashboard = () => {
        clearLoginMessage();
        // Clear password field so it doesn't linger in the DOM after login.
        const pwd = document.getElementById('password');
        if (pwd) pwd.value = '';
        document.getElementById('login-page').classList.add('hidden');
        document.getElementById('dashboard-page').classList.remove('hidden');
        loadUserProfile();
    };

    const getStationStatus = (lastSeen) => {
        if (!lastSeen) return { status: 'never-seen', healthy: false };
        const now = Date.now();
        const diff = now - new Date(lastSeen).getTime();
        return { status: diff > UNHEALTHY_THRESHOLD_MS ? 'unhealthy' : 'healthy', healthy: diff <= UNHEALTHY_THRESHOLD_MS };
    };

    const formatRelativeTime = (dateStr) => {
        if (!dateStr) return 'Never';
        const date = new Date(dateStr);
        const now = new Date();
        const diff = now - date;
        const minutes = Math.floor(diff / 60000);
        const hours = Math.floor(minutes / 60);
        const days = Math.floor(hours / 24);

        if (minutes < 1) return 'Just now';
        if (minutes < 60) return `${minutes}m ago`;
        if (hours < 24) return `${hours}h ago`;
        return `${days}d ago`;
    };

    const formatDateTime = (dateStr) => {
        if (!dateStr) return '-';
        return new Date(dateStr).toLocaleString();
    };

    const showToast = (message, type = 'info') => {
        const toast = document.getElementById('toast');
        const toastMsg = document.getElementById('toast-message');
        toastMsg.textContent = message;
        toast.className = `toast ${type}`;
        toast.classList.remove('hidden');
        setTimeout(() => toast.classList.add('hidden'), 3000);
    };

    const showView = (viewName) => {
        document.querySelectorAll('.view').forEach(v => v.classList.remove('active'));
        document.querySelectorAll('.nav-item').forEach(n => n.classList.remove('active'));
        document.getElementById(`view-${viewName}`).classList.add('active');
        document.querySelector(`.nav-item[data-view="${viewName}"]`)?.classList.add('active');
    };

    const startAutoRefresh = () => {
        stopAutoRefresh();
        refreshTimer = setInterval(() => {
            const activeView = document.querySelector('.view.active')?.id;
            if (activeView === 'view-dashboard') loadDashboardFeed();
            else if (activeView === 'view-messages') loadAllMessages();
        }, REFRESH_INTERVAL);
    };

    const stopAutoRefresh = () => {
        if (refreshTimer) {
            clearInterval(refreshTimer);
            refreshTimer = null;
        }
    };

    const loadUserProfile = async () => {
        const response = await apiRequest('/api/profile');
        if (!response || !response.ok) {
            clearSessionLocal();
            return false;
        }

        currentUser = await response.json();
        document.getElementById('current-username').textContent = currentUser.username;
        document.getElementById('current-user-role').textContent = currentUser.is_admin ? 'Administrator' : 'User';
        isAdmin = currentUser.is_admin;

        document.getElementById('nav-users').style.display = isAdmin ? 'flex' : 'none';
        // Hide only the button, not its header container (previous code hid the whole header).
        document.getElementById('add-user-btn').style.display = isAdmin ? '' : 'none';

        await loadStations();
        loadDashboardFeed();
        startAutoRefresh();
        return true;
    };

    const loadStations = async () => {
        const endpoint = isAdmin ? '/api/admin/stations' : '/api/user/stations';
        const response = await apiRequest(endpoint);
        if (!response || !response.ok) return;

        stations = await response.json();
        renderStationsGrid();
        updateStationFilter();
    };

    const updateStationFilter = () => {
        const select = document.getElementById('filter-station-messages');
        select.innerHTML = '<option value="all">All Stations</option>';
        stations.forEach(s => {
            // station.id is numeric (backend int); station_id is user-controlled -> escape.
            const opt = document.createElement('option');
            opt.value = String(Number(s.id));
            opt.textContent = s.station_id;
            select.appendChild(opt);
        });
    };

    const renderStationsGrid = () => {
        const grid = document.getElementById('stations-grid');
        if (stations.length === 0) {
            grid.innerHTML = `
                <div class="empty-state">
                    <i data-lucide="radio"></i>
                    <h3>No Stations</h3>
                    <p>You don't have any stations yet.</p>
                </div>`;
            lucide.createIcons();
            return;
        }

        grid.innerHTML = stations.map(station => {
            const { healthy } = getStationStatus(station.last_seen);
            const safeId = Number(station.id);
            return `
                <div class="station-card">
                    <div class="station-card-header">
                        <span class="station-card-title">${escapeHtml(station.station_id)}</span>
                        <span class="status-badge ${healthy ? 'status-healthy' : 'status-unhealthy'}">
                            <i data-lucide="${healthy ? 'check-circle' : 'alert-circle'}"></i>
                            ${healthy ? 'Healthy' : 'Unhealthy'}
                        </span>
                    </div>
                    <div class="station-card-meta">
                        <span>Last seen: <strong>${escapeHtml(formatRelativeTime(station.last_seen))}</strong></span>
                        <span>Public: <strong>${station.is_public ? 'Yes' : 'No'}</strong></span>
                        ${station.description ? `<span>${escapeHtml(station.description)}</span>` : ''}
                    </div>
                    <div class="station-card-actions">
                        <button class="btn btn-primary btn-sm" data-action="view-station" data-id="${safeId}">
                            <i data-lucide="eye"></i>
                            View
                        </button>
                        ${isAdmin ? `
                            <button class="btn btn-danger btn-sm" data-action="delete-station" data-id="${safeId}" aria-label="Delete station">
                                <i data-lucide="trash-2"></i>
                            </button>
                        ` : ''}
                    </div>
                </div>
            `;
        }).join('');
        lucide.createIcons();
    };

    const loadDashboardFeed = async () => {
        const feed = document.getElementById('dashboard-feed');
        const filterType = document.getElementById('filter-type-dashboard').value;
        const showUnhealthy = document.getElementById('filter-unhealthy-dashboard').checked;

        if (stations.length === 0) {
            feed.innerHTML = `
                <div class="empty-state">
                    <i data-lucide="activity"></i>
                    <h3>No Stations</h3>
                    <p>You don't have any stations yet.</p>
                </div>`;
            lucide.createIcons();
            return;
        }

        const stationPromises = stations.map(async (station) => {
            const response = await apiRequest(`/api/user/stations/${station.id}/messages`);
            if (!response || !response.ok) return null;
            const messages = await response.json();
            return { station, messages };
        });

        const results = await Promise.all(stationPromises);
        let html = '';

        results.forEach((result) => {
            if (!result) return;
            const { station, messages } = result;
            if (!station) return;
            const { healthy } = getStationStatus(station.last_seen);
            let filteredMessages = Array.isArray(messages) ? messages : [];

            if (filterType !== 'all') {
                filteredMessages = filteredMessages.filter(m => m.level === filterType);
            }

            if (!healthy && !showUnhealthy) return;

            html += `
                <div class="feed-station">
                    <div class="feed-station-header">
                        <span class="feed-station-title">
                            ${escapeHtml(station.station_id)}
                        </span>
                        <span class="status-badge ${healthy ? 'status-healthy' : 'status-unhealthy'}">
                            <i data-lucide="${healthy ? 'check-circle' : 'alert-circle'}"></i>
                            ${healthy ? 'Healthy' : 'Unhealthy'}
                        </span>
                    </div>
            `;

            if (!healthy) {
                html += `<div class="feed-unhealthy-msg">
                    <i data-lucide="alert-triangle"></i>
                    No messages in ${escapeHtml(formatRelativeTime(station.last_seen))}
                </div>`;
            } else if (filteredMessages.length === 0) {
                html += `<div class="feed-unhealthy-msg" style="color: var(--text-secondary)">
                    No messages matching filters
                </div>`;
            } else {
                html += `<div class="feed-station-messages">`;
                filteredMessages.slice(0, 5).forEach(msg => {
                    html += renderMessageItem(msg);
                });
                if (filteredMessages.length > 5) {
                    html += `<button class="btn btn-ghost btn-sm" data-action="view-station" data-id="${Number(station.id)}" style="margin-top: 8px">
                        View all ${filteredMessages.length} messages
                    </button>`;
                }
                html += `</div>`;
            }

            html += `</div>`;
        });

        feed.innerHTML = html || '<div class="empty-state"><h3>No data</h3></div>';
        lucide.createIcons();
    };

    const renderMessageItem = (msg, stationLabel = null) => {
        const parsed = tryParseSensorData(msg.message);
        let icon, iconClass, displayText, bgClass = '';
        const allowedLevels = ['info', 'warning', 'error'];
        const safeLevel = allowedLevels.includes(msg.level) ? msg.level : 'info';
        
        if (parsed.isSensorData) {
            icon = 'activity';
            iconClass = 'info';
            // formatSensorData already escapes values; it returns HTML with trusted markup.
            displayText = formatSensorData(parsed.data);
        } else {
            icon = safeLevel === 'warning' ? 'alert-triangle' : safeLevel === 'error' ? 'x-circle' : 'info';
            iconClass = safeLevel;
            bgClass = safeLevel === 'warning' ? 'warning-bg' : safeLevel === 'error' ? 'error-bg' : '';
            // Plain-text message from station/backend: must be escaped (stored-XSS source).
            displayText = escapeHtml(msg.message) || 'Unknown message';
        }

        const safeLabel = parsed.isSensorData ? 'Data' : escapeHtml(safeLevel);
        
        return `
            <div class="message-item ${iconClass} ${bgClass}">
                <div class="message-icon ${iconClass}">
                    <i data-lucide="${icon}"></i>
                </div>
                <div class="message-content">
                    <div class="message-header">
                        <span class="message-level ${iconClass}">${safeLabel}</span>
                        <span class="message-time" title="${escapeHtml(formatDateTime(msg.created_at))}">${escapeHtml(formatRelativeTime(msg.created_at))}</span>
                    </div>
                    <div class="message-text">${displayText}</div>
                    ${stationLabel ? `<div class="message-station">${escapeHtml(stationLabel)}</div>` : ''}
                </div>
            </div>
        `;
    };

    const viewStationById = (stationId) => {
        currentStationId = Number(stationId);
        const station = stations.find(s => s.id === currentStationId);
        if (!station) return;

        const { healthy } = getStationStatus(station.last_seen);

        document.getElementById('station-detail-name').textContent = station.station_id;
        document.getElementById('station-detail-lastseen').textContent = `Last seen: ${formatRelativeTime(station.last_seen)}`;
        document.getElementById('station-detail-secret').value = station.secret || 'N/A';
        document.getElementById('station-detail-description').value = station.description || '';
        document.getElementById('station-detail-public').checked = station.is_public;
        
        const statusEl = document.getElementById('station-detail-status');
        statusEl.className = `status-badge ${healthy ? 'status-healthy' : 'status-unhealthy'}`;
        statusEl.innerHTML = `<i data-lucide="${healthy ? 'check-circle' : 'alert-circle'}"></i>${healthy ? 'Healthy' : 'Unhealthy'}`;

        showView('station-detail');
        loadStationMessages();
        lucide.createIcons();
    };

    // Kept for backwards-compat (e.g. console use); UI now uses data-action delegation.
    window.viewStation = viewStationById;

    const copySecret = async () => {
        const secret = document.getElementById('station-detail-secret').value;
        if (secret && secret !== 'N/A') {
            try {
                await navigator.clipboard.writeText(secret);
                showToast('Secret copied to clipboard', 'success');
            } catch {
                // Fallback for non-secure contexts / denied permissions.
                const input = document.getElementById('station-detail-secret');
                input.select();
                try {
                    document.execCommand('copy');
                    showToast('Secret copied to clipboard', 'success');
                } catch {
                    showToast('Copy failed: select and copy manually', 'error');
                }
            }
        }
    };
    window.copySecret = copySecret;

    const loadStationMessages = async () => {
        const container = document.getElementById('station-messages');
        const limit = parseInt(document.getElementById('filter-messages-limit').value) || 100;

        if (!currentStationId) return;

        const response = await apiRequest(`/api/user/stations/${currentStationId}/messages`);
        if (!response || !response.ok) {
            container.innerHTML = '<div class="empty-state"><p>Failed to load messages</p></div>';
            return;
        }

        const messages = await response.json();
        const limitedMessages = messages.slice(0, limit);

        if (limitedMessages.length === 0) {
            container.innerHTML = `
                <div class="empty-state">
                    <i data-lucide="message-circle"></i>
                    <h3>No Messages</h3>
                    <p>This station has no messages yet.</p>
                </div>`;
            lucide.createIcons();
            return;
        }

        container.innerHTML = limitedMessages.map(msg => renderMessageItem(msg)).join('');
        lucide.createIcons();
    };

    const saveStation = async () => {
        if (!currentStationId) return;

        const description = document.getElementById('station-detail-description').value;
        const isPublic = document.getElementById('station-detail-public').checked;

        const response = await apiRequest(`/api/user/stations/${currentStationId}`, 'PUT', {
            description,
            is_public: isPublic
        });

        if (response && response.ok) {
            showToast('Station updated successfully', 'success');
            await loadStations();
        } else {
            const data = response ? await response.json() : {};
            showToast(data.msg || 'Failed to update station', 'error');
        }
    };

    const deleteStationById = async (stationId) => {
        if (!isAdmin) return;
        if (!confirm('Are you sure you want to delete this station?')) return;

        const response = await apiRequest(`/api/admin/stations/${Number(stationId)}`, 'DELETE');
        if (response && response.ok) {
            showToast('Station deleted successfully', 'success');
            await loadStations();
            loadDashboardFeed();
        } else {
            showToast('Failed to delete station', 'error');
        }
    };
    window.deleteStation = deleteStationById;

    const loadAllMessages = async () => {
        const container = document.getElementById('all-messages');
        const filterStation = document.getElementById('filter-station-messages').value;
        const filterType = document.getElementById('filter-type-messages').value;
        const unhealthyOnly = document.getElementById('filter-unhealthy-messages').checked;

        const stationPromises = stations.map(async (station) => {
            const response = await apiRequest(`/api/user/stations/${station.id}/messages`);
            if (!response || !response.ok) return [];
            const messages = await response.json();
            return messages.map(m => ({ ...m, station_id: station.station_id, station_db_id: station.id }));
        });

        let allMessages = (await Promise.all(stationPromises)).flat();

        if (filterStation !== 'all') {
            allMessages = allMessages.filter(m => m.station_db_id === parseInt(filterStation));
        }
        if (filterType !== 'all') {
            allMessages = allMessages.filter(m => m.level === filterType);
        }
        allMessages.sort((a, b) => new Date(b.created_at) - new Date(a.created_at));

        const stationsToShow = unhealthyOnly 
            ? stations.filter(s => !getStationStatus(s.last_seen).healthy)
            : stations;

        let html = '';
        
        stationsToShow.forEach(station => {
            const { healthy } = getStationStatus(station.last_seen);
            if (unhealthyOnly && healthy) return;

            if (unhealthyOnly) {
                html += `
                    <div class="feed-station">
                        <div class="feed-station-header">
                            <span class="feed-station-title">${escapeHtml(station.station_id)}</span>
                            <span class="status-badge status-unhealthy">
                                <i data-lucide="alert-circle"></i>
                                Unhealthy
                            </span>
                        </div>
                        <div class="feed-unhealthy-msg">
                            <i data-lucide="alert-triangle"></i>
                            No messages in ${escapeHtml(formatRelativeTime(station.last_seen))}
                        </div>
                    </div>
                `;
            }
        });

        if (!unhealthyOnly) {
            if (allMessages.length === 0) {
                html += `
                    <div class="empty-state">
                        <i data-lucide="message-circle"></i>
                        <h3>No Messages</h3>
                        <p>No messages match your filters.</p>
                    </div>`;
            } else {
                // Reuse renderMessageItem (escaped) and append the station name safely.
                allMessages.forEach(msg => {
                    html += renderMessageItem(msg, msg.station_id);
                });
            }
        }

        container.innerHTML = html || '<div class="empty-state"><h3>No data</h3></div>';
        lucide.createIcons();
    };

    const loadUsers = async () => {
        if (!isAdmin) return;

        const response = await apiRequest('/api/admin/users');
        if (!response || !response.ok) return;

        const users = await response.json();
        const list = document.getElementById('users-list');

        list.innerHTML = users.map(user => `
            <div class="user-card">
                <div class="user-card-info">
                    <div class="user-card-avatar">
                        <i data-lucide="user"></i>
                    </div>
                    <div class="user-card-details">
                        <h3>${escapeHtml(user.username)}</h3>
                        <span>${user.is_admin ? 'Administrator' : 'Regular User'}</span>
                    </div>
                </div>
                <div class="user-card-actions">
                    <button class="btn btn-secondary btn-sm" data-action="reset-password" data-id="${Number(user.id)}">
                        <i data-lucide="key"></i>
                        Reset
                    </button>
                    ${user.id !== currentUser.id ? `
                        <button class="btn btn-danger btn-sm" data-action="delete-user" data-id="${Number(user.id)}" aria-label="Delete user">
                            <i data-lucide="trash-2"></i>
                        </button>
                    ` : ''}
                </div>
            </div>
        `).join('');
        lucide.createIcons();
    };

    const deleteUserById = async (userId) => {
        if (!isAdmin) return;
        if (!confirm('Are you sure you want to delete this user?')) return;

        const response = await apiRequest(`/api/admin/users/${userId}`, 'DELETE');
        if (response && response.ok) {
            showToast('User deleted successfully', 'success');
            await loadUsers();
        } else {
            showToast('Failed to delete user', 'error');
        }
    };
    window.deleteUser = deleteUserById;

    const openResetPasswordModalById = (userId) => {
        document.getElementById('reset-user-id').value = Number(userId);
        openModal('modal-reset-password');
    };
    window.openResetPasswordModal = openResetPasswordModalById;

    const addUser = async (username, password, isAdminUser) => {
        const response = await apiRequest('/api/admin/users', 'POST', { username, password, is_admin: isAdminUser });
        if (response && response.ok) {
            showToast('User added successfully', 'success');
            await loadUsers();
            closeModal('modal-add-user');
        } else {
            const data = response ? await response.json() : {};
            showToast(data.msg || 'Failed to add user', 'error');
        }
    };

    let allUsers = [];

    const loadUsersForDropdown = async () => {
        const response = await apiRequest('/api/admin/users');
        if (!response || !response.ok) return;
        allUsers = await response.json();
        
        const select = document.getElementById('station-user-select');
        select.innerHTML = '<option value="">-- Select User --</option>';
        allUsers.forEach(user => {
            const opt = document.createElement('option');
            opt.value = String(Number(user.id));
            opt.textContent = user.username;
            select.appendChild(opt);
        });
    };

    const addStation = async (stationId, userId = null) => {
        let response;
        if (isAdmin && userId) {
            response = await apiRequest('/api/admin/stations', 'POST', { station_id: stationId, user_id: parseInt(userId) });
        } else {
            response = await apiRequest('/api/user/stations', 'POST', { station_id: stationId });
        }
        
        if (response && response.ok) {
            const data = await response.json();
            showToast(`Station added! Secret: ${data.secret}`, 'success');
            await loadStations();
            closeModal('modal-add-station');
            document.getElementById('add-station-form').reset();
        } else {
            const data = response ? await response.json() : {};
            showToast(data.msg || 'Failed to add station', 'error');
        }
    };

    const resetUserPassword = async (userId, newPassword) => {
        const response = await apiRequest(`/api/admin/users/${userId}/reset-password`, 'PUT', { new_password: newPassword });
        if (response && response.ok) {
            showToast('Password reset successfully', 'success');
            closeModal('modal-reset-password');
        } else {
            const data = response ? await response.json() : {};
            showToast(data.msg || 'Failed to reset password', 'error');
        }
    };

    const openModal = (modalId) => {
        document.getElementById(modalId).classList.remove('hidden');
    };

    const closeModal = (modalId) => {
        document.getElementById(modalId).classList.add('hidden');
    };

    const clearSessionLocal = () => {
        currentUser = null;
        isAdmin = false;
        stations = [];
        currentStationId = null;
        showLogin();
    };

    // Server-side logout: clears the HttpOnly auth + CSRF cookies.
    // Tolerates 404 (backend not yet migrated) — local state is cleared anyway.
    const logout = async () => {
        try {
            await apiRequest('/api/logout', 'POST');
        } catch {
            // ignore — still clear local state below
        }
        clearSessionLocal();
    };

    document.getElementById('login-form').addEventListener('submit', async (e) => {
        e.preventDefault();
        clearLoginMessage();
        const submitBtn = document.getElementById('login-submit-btn');
        const username = e.target.username.value.trim();
        const password = e.target.password.value;

        if (!username || !password) {
            showLoginMessage('Username and password are required');
            return;
        }

        submitBtn.disabled = true;
        try {
            const response = await apiRequest('/api/login', 'POST', { username, password });
            if (!response) {
                // apiRequest already toasted on network error; keep box hidden unless HTTP error.
                return;
            }

            let data = {};
            try {
                data = await response.json();
            } catch {
                data = {};
            }
            if (response.ok) {
                // Cookie mode: backend set HttpOnly cookies via Set-Cookie.
                // No token in JS. Backwards-compat: if backend still returns
                // access_token (pre-migration), ignore it — do NOT store it.
                showDashboard();
            } else {
                // textContent via helper: backend msg cannot inject HTML.
                showLoginMessage(data.msg || 'Login failed');
            }
        } finally {
            submitBtn.disabled = false;
        }
    });

    // Clear the error as soon as the user starts typing again.
    document.getElementById('login-form').addEventListener('input', clearLoginMessage);

    document.getElementById('logout-button').addEventListener('click', logout);

    document.querySelectorAll('.nav-item').forEach(item => {
        item.addEventListener('click', () => {
            const view = item.dataset.view;
            if (view === 'stations') loadStations();
            if (view === 'messages') loadAllMessages();
            if (view === 'users') loadUsers();
            showView(view);
        });
    });

    document.getElementById('back-to-stations').addEventListener('click', () => {
        currentStationId = null;
        showView('stations');
        loadStations();
    });

    document.getElementById('save-station-detail').addEventListener('click', saveStation);

    document.getElementById('filter-messages-limit').addEventListener('change', loadStationMessages);

    document.getElementById('refresh-dashboard').addEventListener('click', loadDashboardFeed);
    document.getElementById('refresh-messages').addEventListener('click', loadAllMessages);

    document.getElementById('filter-type-dashboard').addEventListener('change', loadDashboardFeed);
    document.getElementById('filter-unhealthy-dashboard').addEventListener('change', loadDashboardFeed);
    document.getElementById('filter-station-messages').addEventListener('change', loadAllMessages);
    document.getElementById('filter-type-messages').addEventListener('change', loadAllMessages);
    document.getElementById('filter-unhealthy-messages').addEventListener('change', loadAllMessages);

    document.getElementById('add-user-btn').addEventListener('click', () => openModal('modal-add-user'));

    document.getElementById('add-user-form').addEventListener('submit', (e) => {
        e.preventDefault();
        const username = document.getElementById('new-username').value;
        const password = document.getElementById('new-password').value;
        const isAdminUser = document.getElementById('new-is-admin').checked;
        addUser(username, password, isAdminUser);
        e.target.reset();
    });

    document.getElementById('add-station-btn').addEventListener('click', async () => {
        if (isAdmin) {
            await loadUsersForDropdown();
            document.getElementById('station-user-select-group').style.display = 'block';
        } else {
            document.getElementById('station-user-select-group').style.display = 'none';
        }
        openModal('modal-add-station');
    });

    document.getElementById('add-station-form').addEventListener('submit', (e) => {
        e.preventDefault();
        const stationId = document.getElementById('new-station-id').value;
        const userId = document.getElementById('station-user-select').value;
        
        if (stationId.length > 32) {
            showToast('Station ID must be max 32 characters', 'error');
            return;
        }
        
        if (isAdmin && userId) {
            addStation(stationId, userId);
        } else {
            addStation(stationId, null);
        }
    });

    document.getElementById('reset-password-form').addEventListener('submit', (e) => {
        e.preventDefault();
        const userId = document.getElementById('reset-user-id').value;
        const newPassword = document.getElementById('reset-new-password').value;
        const confirmPassword = document.getElementById('reset-confirm-password').value;
        
        if (newPassword !== confirmPassword) {
            showToast('Passwords do not match', 'error');
            return;
        }
        
        if (newPassword.length < 1) {
            showToast('Password cannot be empty', 'error');
            return;
        }
        
        resetUserPassword(parseInt(userId), newPassword);
        e.target.reset();
    });

    const changeUserPassword = async (currentPassword, newPassword) => {
        const response = await apiRequest('/api/user/change-password', 'PUT', {
            current_password: currentPassword,
            new_password: newPassword
        });
        if (response && response.ok) {
            showToast('Password changed successfully', 'success');
            closeModal('modal-change-password');
        } else {
            const data = response ? await response.json() : {};
            showToast(data.msg || 'Failed to change password', 'error');
        }
    };

    document.getElementById('change-password-btn').addEventListener('click', () => {
        openModal('modal-change-password');
    });

    document.getElementById('change-password-form').addEventListener('submit', (e) => {
        e.preventDefault();
        const currentPassword = document.getElementById('current-password').value;
        const newPassword = document.getElementById('user-new-password').value;
        const confirmPassword = document.getElementById('user-confirm-password').value;
        
        if (newPassword !== confirmPassword) {
            showToast('Passwords do not match', 'error');
            return;
        }
        
        if (newPassword.length < 1) {
            showToast('Password cannot be empty', 'error');
            return;
        }
        
        changeUserPassword(currentPassword, newPassword);
        e.target.reset();
    });

    document.querySelectorAll('.modal-close').forEach(btn => {
        btn.addEventListener('click', () => {
            btn.closest('.modal').classList.add('hidden');
        });
    });

    document.querySelectorAll('.modal-backdrop').forEach(backdrop => {
        backdrop.addEventListener('click', () => {
            backdrop.closest('.modal').classList.add('hidden');
        });
    });

    // Delegated actions (replaces inline onclick; CSP-friendly, no string injection).
    document.addEventListener('click', (e) => {
        const expandable = e.target.closest('.sensor-more[data-expandable]');
        if (expandable) {
            expandable.style.display = 'none';
            const hidden = expandable.nextElementSibling;
            if (hidden) hidden.style.display = 'inline';
            return;
        }
        const btn = e.target.closest('[data-action]');
        if (!btn) return;
        const id = Number(btn.dataset.id);
        if (!Number.isFinite(id)) return;
        switch (btn.dataset.action) {
            case 'view-station':
                viewStationById(id);
                break;
            case 'delete-station':
                deleteStationById(id);
                break;
            case 'delete-user':
                deleteUserById(id);
                break;
            case 'reset-password':
                openResetPasswordModalById(id);
                break;
        }
    });

    document.getElementById('copy-secret-btn')?.addEventListener('click', copySecret);

    // Close modals with Escape.
    document.addEventListener('keydown', (e) => {
        if (e.key === 'Escape') {
            document.querySelectorAll('.modal:not(.hidden)').forEach(m => m.classList.add('hidden'));
        }
    });

    // Boot: session lives in HttpOnly cookies, so probe the backend.
    // showDashboard() -> loadUserProfile() succeeds only with a valid cookie.
    showLogin(); // default view, replaced if profile probe succeeds
    loadUserProfile().then((ok) => {
        if (ok) {
            document.getElementById('login-page').classList.add('hidden');
            document.getElementById('dashboard-page').classList.remove('hidden');
            clearLoginMessage();
            const pwd = document.getElementById('password');
            if (pwd) pwd.value = '';
        }
    });
});
