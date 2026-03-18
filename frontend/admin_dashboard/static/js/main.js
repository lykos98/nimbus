document.addEventListener('DOMContentLoaded', () => {
    const loginSection = document.getElementById('login-section');
    const dashboardSection = document.getElementById('dashboard-section');
    
    const loginForm = document.getElementById('login-form');
    const loginMessage = document.getElementById('login-message');

    const dashboardMessage = document.getElementById('dashboard-message');
    const currentUserSpan = document.getElementById('current-username');
    const currentUserIdSpan = document.getElementById('current-userid');
    const logoutButton = document.getElementById('logout-button');

    const profileSection = document.getElementById('profile-section');
    const profileUsername = document.getElementById('profile-username');
    const profileIsAdmin = document.getElementById('profile-is-admin');

    const userManagementSection = document.getElementById('user-management');
    const addStationForm = document.getElementById('add-station-form');
    
    const addUserForm = document.getElementById('add-user-form');
    const usersTableBody = document.querySelector('#users-table tbody');
    const stationsTableBody = document.querySelector('#stations-table tbody');

    const messageStationSelect = document.getElementById('message-station-select');
    const messagesContainer = document.getElementById('messages-container');
    const selectedStationIdSpan = document.getElementById('selected-station-id');
    const messagesTableBody = document.querySelector('#messages-table tbody');

    const API_URL = "";
    let currentUser = null;
    let isAdmin = false;

    async function apiRequest(endpoint, method = 'GET', body = null) {
        const token = localStorage.getItem('access_token');
        if (!token && endpoint !== '/api/login') {
            showLogin();
            return null;
        }

        const headers = {
            'Content-Type': 'application/json',
        };

        if (token) {
            headers['Authorization'] = `Bearer ${token}`;
        }

        const config = {
            method: method,
            headers: headers,
        };

        if (body) {
            config.body = JSON.stringify(body);
        }

        try {
            const response = await fetch(API_URL + endpoint, config);
            if (response.status === 401 && endpoint !== '/api/login') {
                logout();
                return null;
            }
            return response;
        } catch (error) {
            console.error('API Request Error:', error);
            dashboardMessage.textContent = 'Network error occurred.';
            dashboardMessage.style.color = 'red';
            return null;
        }
    }

    function showLogin() {
        loginSection.style.display = 'block';
        dashboardSection.style.display = 'none';
        currentUser = null;
        localStorage.removeItem('access_token');
    }

    async function showDashboard() {
        loginSection.style.display = 'none';
        dashboardSection.style.display = 'block';
        await loadUserProfile();
    }

    async function loadUserProfile() {
        const response = await apiRequest('/api/profile');
        if (!response || !response.ok) {
            showLogin();
            return;
        }
        
        currentUser = await response.json();
        currentUserSpan.textContent = currentUser.username;
        currentUserIdSpan.textContent = currentUser.id;
        profileUsername.textContent = currentUser.username;
        profileIsAdmin.textContent = currentUser.is_admin ? 'Yes' : 'No';
        
        isAdmin = currentUser.is_admin;

        if (isAdmin) {
            userManagementSection.style.display = 'block';
            addStationForm.style.display = 'block';
            await loadAdminData();
        } else {
            userManagementSection.style.display = 'none';
            addStationForm.style.display = 'none';
            await loadUserData();
        }
    }

    async function loadAdminData() {
        await loadUsers();
        await loadStations();
    }
    
    async function loadUserData() {
        await loadStations();
    }

    async function loadUsers() {
        const response = await apiRequest('/api/admin/users');
        if (!response || !response.ok) return;

        try {
            const users = await response.json();
            usersTableBody.innerHTML = '';
            users.forEach(user => {
                const row = document.createElement('tr');
                row.innerHTML = `
                    <td>${user.id}</td>
                    <td>${user.username}</td>
                    <td>${user.is_admin ? 'Yes' : 'No'}</td>
                    <td><button class="delete-user" data-id="${user.id}">Delete</button></td>
                `;
                usersTableBody.appendChild(row);
            });
        } catch (error) {
            console.error("Failed to process users response.", error);
        }
    }

    async function loadStations() {
        const endpoint = isAdmin ? '/api/admin/stations' : '/api/user/stations';
        const response = await apiRequest(endpoint);
        if (!response || !response.ok) return;
        
        try {
            const stations = await response.json();
            stationsTableBody.innerHTML = '';
            messageStationSelect.innerHTML = '<option value="">-- Select Station --</option>';
            
            stations.forEach(station => {
                const lastSeen = station.last_seen ? new Date(station.last_seen) : null;
                const now = new Date();
                const tenMinutesAgo = new Date(now.getTime() - 10 * 60 * 1000);
                const isHealthy = lastSeen && lastSeen > tenMinutesAgo;
                const lastSeenStr = lastSeen ? lastSeen.toLocaleString() : 'Never';
                const statusClass = isHealthy ? 'status-healthy' : 'status-error';
                const statusText = isHealthy ? 'Healthy' : 'Error (>10min)';

                const row = document.createElement('tr');
                const secret = station.secret || 'N/A (Hidden)';
                
                row.innerHTML = `
                    <td>${station.id}</td>
                    <td>${station.station_id}</td>
                    <td class="secret-cell">${isAdmin ? secret : '***'}</td>
                    <td>
                        <input type="text" class="station-description" 
                               data-id="${station.id}" 
                               value="${station.description || ''}" 
                               placeholder="Enter description">
                    </td>
                    <td>
                        <input type="checkbox" class="station-public" 
                               data-id="${station.id}" 
                               ${station.is_public ? 'checked' : ''}>
                    </td>
                    <td>${lastSeenStr}</td>
                    <td><span class="status-badge ${statusClass}">${statusText}</span></td>
                    <td>
                        ${isAdmin ? `<button class="delete-station" data-id="${station.id}">Delete</button>` : ''}
                        <button class="save-station" data-id="${station.id}">Save</button>
                    </td>
                `;
                stationsTableBody.appendChild(row);

                const option = document.createElement('option');
                option.value = station.id;
                option.textContent = station.station_id;
                messageStationSelect.appendChild(option);
            });
        } catch (error) {
            console.error("Failed to process stations response.", error);
        }
    }

    async function loadStationMessages(stationId) {
        if (!stationId) {
            messagesContainer.style.display = 'none';
            return;
        }

        const response = await apiRequest(`/api/user/stations/${stationId}/messages`);
        if (!response || !response.ok) {
            messagesTableBody.innerHTML = '<tr><td colspan="4">Failed to load messages</td></tr>';
            return;
        }

        try {
            const messages = await response.json();
            messagesTableBody.innerHTML = '';
            
            if (messages.length === 0) {
                messagesTableBody.innerHTML = '<tr><td colspan="4">No messages</td></tr>';
            } else {
                messages.forEach(msg => {
                    const row = document.createElement('tr');
                    const levelClass = msg.level === 'warning' ? 'level-warning' : 
                                       msg.level === 'error' ? 'level-error' : 'level-info';
                    const createdAt = new Date(msg.created_at).toLocaleString();
                    row.innerHTML = `
                        <td>${msg.id}</td>
                        <td><span class="level-badge ${levelClass}">${msg.level}</span></td>
                        <td>${msg.message || '(no message)'}</td>
                        <td>${createdAt}</td>
                    `;
                    messagesTableBody.appendChild(row);
                });
            }
            
            const stationOption = messageStationSelect.querySelector(`option[value="${stationId}"]`);
            selectedStationIdSpan.textContent = stationOption ? stationOption.textContent : stationId;
            messagesContainer.style.display = 'block';
        } catch (error) {
            console.error("Failed to process messages response.", error);
            messagesTableBody.innerHTML = '<tr><td colspan="4">Failed to load messages</td></tr>';
        }
    }

    loginForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const username = loginForm.username.value;
        const password = loginForm.password.value;

        try {
            const response = await apiRequest('/api/login', 'POST', { username, password });
            
            if (!response) return;
            const data = await response.json();
            if (response.ok) {
                localStorage.setItem('access_token', data.access_token);
                await showDashboard();
            } else {
                loginMessage.textContent = data.msg || 'Login failed.';
                loginMessage.style.color = 'red';
            }
        } catch (error) {
            loginMessage.textContent = 'An error occurred during login.';
            loginMessage.style.color = 'red';
        }
    });

    logoutButton.addEventListener('click', () => {
        showLogin();
    });
    
    addUserForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const username = addUserForm.querySelector('#new-username').value;
        const password = addUserForm.querySelector('#new-password').value;
        const is_admin = addUserForm.querySelector('#new-is-admin').checked;

        const response = await apiRequest('/api/admin/users', 'POST', { username, password, is_admin });
        
        if (response && response.ok) {
            addUserForm.reset();
            await loadUsers();
        } else {
            const data = response ? await response.json() : { msg: "Request failed" };
            alert(`Error: ${data.msg}`);
        }
    });

    addStationForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const station_id = addStationForm.querySelector('#new-station-id').value;
        const user_id = addStationForm.querySelector('#station-user-id').value;

        const response = await apiRequest('/api/admin/stations', 'POST', { station_id, user_id: parseInt(user_id) });
        
        if (response && response.ok) {
            addStationForm.reset();
            await loadStations();
        } else {
            const data = response ? await response.json() : { msg: "Request failed" };
            alert(`Error: ${data.msg}`);
        }
    });

    usersTableBody.addEventListener('click', async (e) => {
        if (e.target.classList.contains('delete-user')) {
            const userId = e.target.dataset.id;
            if (confirm(`Are you sure you want to delete user ${userId}?`)) {
                const response = await apiRequest(`/api/admin/users/${userId}`, 'DELETE');
                if (response && response.ok) {
                    await loadUsers();
                } else {
                    const data = response ? await response.json() : { msg: "Request failed" };
                    alert(`Error: ${data.msg}`);
                }
            }
        }
    });

    stationsTableBody.addEventListener('click', async (e) => {
        const stationId = e.target.dataset.id;
        
        if (e.target.classList.contains('delete-station')) {
            if (confirm(`Are you sure you want to delete station ${stationId}?`)) {
                const response = await apiRequest(`/api/admin/stations/${stationId}`, 'DELETE');
                if (response && response.ok) {
                    await loadStations();
                } else {
                    const data = response ? await response.json() : { msg: "Request failed" };
                    alert(`Error: ${data.msg}`);
                }
            }
        } else if (e.target.classList.contains('save-station')) {
            const row = e.target.closest('tr');
            const description = row.querySelector('.station-description').value;
            const isPublic = row.querySelector('.station-public').checked;

            const response = await apiRequest(`/api/user/stations/${stationId}`, 'PUT', {
                description: description,
                is_public: isPublic
            });
            
            if (response && response.ok) {
                alert('Station updated successfully');
            } else {
                const data = response ? await response.json() : { msg: "Request failed" };
                alert(`Error: ${data.msg}`);
            }
        }
    });

    messageStationSelect.addEventListener('change', async (e) => {
        const stationId = e.target.value;
        await loadStationMessages(stationId);
    });

    function init() {
        if (localStorage.getItem('access_token')) {
            showDashboard();
        } else {
            showLogin();
        }
    }

    init();
});
