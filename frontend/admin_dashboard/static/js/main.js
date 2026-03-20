document.addEventListener('DOMContentLoaded', () => {
    lucide.createIcons();

    const UNHEALTHY_THRESHOLD_MS = 12 * 60 * 1000;
    const REFRESH_INTERVAL = 60000;

    let currentUser = null;
    let isAdmin = false;
    let stations = [];
    let currentStationId = null;
    let refreshTimer = null;

    const apiRequest = async (endpoint, method = 'GET', body = null) => {
        const token = localStorage.getItem('access_token');
        if (!token && endpoint !== '/api/login') {
            showLogin();
            return null;
        }

        const headers = { 'Content-Type': 'application/json' };
        if (token) headers['Authorization'] = `Bearer ${token}`;

        const config = { method, headers };
        if (body) config.body = JSON.stringify(body);

        try {
            const response = await fetch(endpoint, config);
            if (response.status === 401) {
                logout();
                return null;
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
            showLogin();
            return;
        }

        currentUser = await response.json();
        document.getElementById('current-username').textContent = currentUser.username;
        document.getElementById('current-user-role').textContent = currentUser.is_admin ? 'Administrator' : 'User';
        isAdmin = currentUser.is_admin;

        document.getElementById('nav-users').style.display = isAdmin ? 'flex' : 'none';
        document.getElementById('add-user-btn').parentElement.style.display = isAdmin ? 'block' : 'none';

        await loadStations();
        loadDashboardFeed();
        startAutoRefresh();
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
            select.innerHTML += `<option value="${s.id}">${s.station_id}</option>`;
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
            return `
                <div class="station-card">
                    <div class="station-card-header">
                        <span class="station-card-title">${station.station_id}</span>
                        <span class="status-badge ${healthy ? 'status-healthy' : 'status-unhealthy'}">
                            <i data-lucide="${healthy ? 'check-circle' : 'alert-circle'}"></i>
                            ${healthy ? 'Healthy' : 'Unhealthy'}
                        </span>
                    </div>
                    <div class="station-card-meta">
                        <span>Last seen: <strong>${formatRelativeTime(station.last_seen)}</strong></span>
                        <span>Public: <strong>${station.is_public ? 'Yes' : 'No'}</strong></span>
                        ${station.description ? `<span>${station.description}</span>` : ''}
                    </div>
                    <div class="station-card-actions">
                        <button class="btn btn-primary btn-sm" onclick="viewStation(${station.id})">
                            <i data-lucide="eye"></i>
                            View
                        </button>
                        ${isAdmin ? `
                            <button class="btn btn-danger btn-sm" onclick="deleteStation(${station.id})">
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

        results.forEach(({ station, messages }) => {
            if (!station) return;
            const { healthy } = getStationStatus(station.last_seen);
            let filteredMessages = messages;

            if (filterType !== 'all') {
                filteredMessages = filteredMessages.filter(m => m.level === filterType);
            }

            if (!healthy && !showUnhealthy) return;

            html += `
                <div class="feed-station">
                    <div class="feed-station-header">
                        <span class="feed-station-title">
                            ${station.station_id}
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
                    No messages in ${formatRelativeTime(station.last_seen)}
                </div>`;
            } else if (filteredMessages.length === 0) {
                html += `<div class="feed-unhealthy-msg" style="color: var(--text-secondary)">
                    No messages matching filters
                </div>`;
            } else {
                html += `<div class="feed-station-messages">`;
                filteredMessages.slice(0, 5).forEach(msg => {
                    html += renderMessageItem(msg, false);
                });
                if (filteredMessages.length > 5) {
                    html += `<button class="btn btn-ghost btn-sm" onclick="viewStation(${station.id})" style="margin-top: 8px">
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

    const renderMessageItem = (msg, showStation = true) => {
        const icons = { info: 'info', warning: 'alert-triangle', error: 'x-circle' };
        return `
            <div class="message-item ${msg.level}">
                <div class="message-icon ${msg.level}">
                    <i data-lucide="${icons[msg.level] || 'info'}"></i>
                </div>
                <div class="message-content">
                    <div class="message-header">
                        <span class="message-level ${msg.level}">${msg.level}</span>
                        <span class="message-time" title="${formatDateTime(msg.created_at)}">${formatRelativeTime(msg.created_at)}</span>
                    </div>
                    <div class="message-text">${msg.message || 'Data submission'}</div>
                    ${showStation ? '' : ''}
                </div>
            </div>
        `;
    };

    window.viewStation = (stationId) => {
        currentStationId = stationId;
        const station = stations.find(s => s.id === stationId);
        if (!station) return;

        const { healthy } = getStationStatus(station.last_seen);

        document.getElementById('station-detail-name').textContent = station.station_id;
        document.getElementById('station-detail-lastseen').textContent = `Last seen: ${formatRelativeTime(station.last_seen)}`;
        document.getElementById('station-detail-description').value = station.description || '';
        document.getElementById('station-detail-public').checked = station.is_public;
        
        const statusEl = document.getElementById('station-detail-status');
        statusEl.className = `status-badge ${healthy ? 'status-healthy' : 'status-unhealthy'}`;
        statusEl.innerHTML = `<i data-lucide="${healthy ? 'check-circle' : 'alert-circle'}"></i>${healthy ? 'Healthy' : 'Unhealthy'}`;

        showView('station-detail');
        loadStationMessages();
        lucide.createIcons();
    };

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

    window.deleteStation = async (stationId) => {
        if (!isAdmin) return;
        if (!confirm('Are you sure you want to delete this station?')) return;

        const response = await apiRequest(`/api/admin/stations/${stationId}`, 'DELETE');
        if (response && response.ok) {
            showToast('Station deleted successfully', 'success');
            await loadStations();
            loadDashboardFeed();
        } else {
            showToast('Failed to delete station', 'error');
        }
    };

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

        const stationsIdsToShow = new Set(stationsToShow.map(s => s.id));

        let html = '';
        
        stationsToShow.forEach(station => {
            const { healthy } = getStationStatus(station.last_seen);
            if (unhealthyOnly && healthy) return;

            if (unhealthyOnly) {
                html += `
                    <div class="feed-station">
                        <div class="feed-station-header">
                            <span class="feed-station-title">${station.station_id}</span>
                            <span class="status-badge status-unhealthy">
                                <i data-lucide="alert-circle"></i>
                                Unhealthy
                            </span>
                        </div>
                        <div class="feed-unhealthy-msg">
                            <i data-lucide="alert-triangle"></i>
                            No messages in ${formatRelativeTime(station.last_seen)}
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
                allMessages.forEach(msg => {
                    html += `
                        <div class="message-item ${msg.level}">
                            <div class="message-icon ${msg.level}">
                                <i data-lucide="${msg.level === 'info' ? 'info' : msg.level === 'warning' ? 'alert-triangle' : 'x-circle'}"></i>
                            </div>
                            <div class="message-content">
                                <div class="message-header">
                                    <span class="message-level ${msg.level}">${msg.level}</span>
                                    <span class="message-time" title="${formatDateTime(msg.created_at)}">${formatRelativeTime(msg.created_at)}</span>
                                </div>
                                <div class="message-text">${msg.message || 'Data submission'}</div>
                                <div class="message-station">${msg.station_id}</div>
                            </div>
                        </div>
                    `;
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
                        <h3>${user.username}</h3>
                        <span>${user.is_admin ? 'Administrator' : 'Regular User'}</span>
                    </div>
                </div>
                <div class="user-card-actions">
                    ${user.id !== currentUser.id ? `
                        <button class="btn btn-danger btn-sm" onclick="deleteUser(${user.id})">
                            <i data-lucide="trash-2"></i>
                        </button>
                    ` : ''}
                </div>
            </div>
        `).join('');
        lucide.createIcons();
    };

    window.deleteUser = async (userId) => {
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

    const addStation = async (stationId, userId) => {
        const response = await apiRequest('/api/admin/stations', 'POST', { station_id: stationId, user_id: parseInt(userId) });
        if (response && response.ok) {
            showToast('Station added successfully', 'success');
            await loadStations();
            closeModal('modal-add-station');
        } else {
            const data = response ? await response.json() : {};
            showToast(data.msg || 'Failed to add station', 'error');
        }
    };

    const openModal = (modalId) => {
        document.getElementById(modalId).classList.remove('hidden');
    };

    const closeModal = (modalId) => {
        document.getElementById(modalId).classList.add('hidden');
    };

    const logout = () => {
        showLogin();
        localStorage.removeItem('access_token');
        currentUser = null;
        stations = [];
    };

    document.getElementById('login-form').addEventListener('submit', async (e) => {
        e.preventDefault();
        const username = e.target.username.value;
        const password = e.target.password.value;

        const response = await apiRequest('/api/login', 'POST', { username, password });
        if (!response) return;

        const data = await response.json();
        if (response.ok) {
            localStorage.setItem('access_token', data.access_token);
            showDashboard();
        } else {
            document.getElementById('login-message').textContent = data.msg || 'Login failed';
        }
    });

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

    if (localStorage.getItem('access_token')) {
        showDashboard();
    } else {
        showLogin();
    }
});
