document.addEventListener('DOMContentLoaded', () => {
    const loginSection = document.getElementById('login-section');
    const dashboardSection = document.getElementById('dashboard-section');
    
    const loginForm = document.getElementById('login-form');
    const loginMessage = document.getElementById('login-message');

    const dashboardMessage = document.getElementById('dashboard-message');
    const currentUserSpan = document.getElementById('current-username');
    const logoutButton = document.getElementById('logout-button');

    const userManagementSection = document.getElementById('user-management');
    const stationManagementSection = document.getElementById('station-management');
    
    const addUserForm = document.getElementById('add-user-form');
    const usersTableBody = document.querySelector('#users-table tbody');
    
    const addStationForm = document.getElementById('add-station-form');
    const stationsTableBody = document.querySelector('#stations-table tbody');

    const API_URL = ''; // The UI is served from the same origin as the API

    /**
     * UTILITY FUNCTIONS
     */

    function parseJwt(token) {
        try {
            return JSON.parse(atob(token.split('.')[1]));
        } catch (e) {
            return null;
        }
    }

    async function apiRequest(endpoint, method = 'GET', body = null) {
        const token = localStorage.getItem('access_token');
        if (!token) {
            showLogin();
            return;
        }

        const headers = {
            'Content-Type': 'application/json',
            'Authorization': `Bearer ${token}`
        };

        const config = {
            method: method,
            headers: headers,
        };

        if (body) {
            config.body = JSON.stringify(body);
        }

        try {
            const response = await fetch(API_URL + endpoint, config);
            if (response.status === 401) {
                // Token is invalid or expired
                logout();
                return;
            }
            return response;
        } catch (error) {
            console.error('API Request Error:', error);
            dashboardMessage.textContent = 'Network error occurred.';
            dashboardMessage.style.color = 'red';
        }
    }

    /**
     * UI TOGGLE FUNCTIONS
     */

    function showLogin() {
        loginSection.style.display = 'block';
        dashboardSection.style.display = 'none';
        localStorage.removeItem('access_token');
    }

    function showDashboard() {
        loginSection.style.display = 'none';
        dashboardSection.style.display = 'block';
        
        const token = localStorage.getItem('access_token');
        if (token) {
            const decodedToken = parseJwt(token);
            const username = decodedToken?.sub?.username || 'User';
            const isAdmin = decodedToken?.sub?.is_admin || false;

            currentUserSpan.textContent = username;

            if (isAdmin) {
                userManagementSection.style.display = 'block';
                loadAdminData();
            } else {
                userManagementSection.style.display = 'none';
                loadUserData();
            }
        }
    }

    /**
     * DATA LOADING AND RENDERING
     */

    async function loadAdminData() {
        await loadUsers();
        await loadStations(true);
    }
    
    async function loadUserData() {
        await loadStations(false);
    }

    async function loadUsers() {
        const response = await apiRequest('/api/admin/users');
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
    }

    async function loadStations(isAdmin = false) {
        const endpoint = isAdmin ? '/api/admin/stations' : '/api/user/stations';
        const response = await apiRequest(endpoint);
        const stations = await response.json();

        stationsTableBody.innerHTML = '';
        stations.forEach(station => {
            const row = document.createElement('tr');
            row.innerHTML = `
                <td>${station.id}</td>
                <td>${station.station_id}</td>
                <td>${station.secret || 'N/A'}</td> 
                <td>${station.user_id}</td>
                <td><button class="delete-station" data-id="${station.id}">Delete</button></td>
            `;
            stationsTableBody.appendChild(row);
        });
    }
    
    /**
     * EVENT HANDLERS
     */

    loginForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const username = loginForm.username.value;
        const password = loginForm.password.value;

        try {
            const response = await fetch(API_URL + '/api/login', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ username, password })
            });

            const data = await response.json();
            if (response.ok) {
                localStorage.setItem('access_token', data.access_token);
                showDashboard();
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
        const data = await response.json();
        
        if (response.ok) {
            addUserForm.reset();
            loadUsers();
        } else {
            alert(`Error: ${data.msg}`);
        }
    });

    addStationForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const station_id = addStationForm.querySelector('#new-station-id').value;
        const user_id = addStationForm.querySelector('#station-user-id').value;

        const response = await apiRequest('/api/admin/stations', 'POST', { station_id, user_id: parseInt(user_id) });
        const data = await response.json();

        if (response.ok) {
            addStationForm.reset();
            loadStations(true);
        } else {
            alert(`Error: ${data.msg}`);
        }
    });

    usersTableBody.addEventListener('click', async (e) => {
        if (e.target.classList.contains('delete-user')) {
            const userId = e.target.dataset.id;
            if (confirm(`Are you sure you want to delete user ${userId}?`)) {
                const response = await apiRequest(`/api/admin/users/${userId}`, 'DELETE');
                if (response.ok) {
                    loadUsers();
                } else {
                    const data = await response.json();
                    alert(`Error: ${data.msg}`);
                }
            }
        }
    });

    stationsTableBody.addEventListener('click', async (e) => {
        if (e.target.classList.contains('delete-station')) {
            const stationId = e.target.dataset.id;
            if (confirm(`Are you sure you want to delete station ${stationId}?`)) {
                const response = await apiRequest(`/api/admin/stations/${stationId}`, 'DELETE');
                if (response.ok) {
                    loadStations(true);
                } else {
                    const data = await response.json();
                    alert(`Error: ${data.msg}`);
                }
            }
        }
    });

    /**
     * INITIALIZATION
     */
    function init() {
        if (localStorage.getItem('access_token')) {
            showDashboard();
        } else {
            showLogin();
        }
    }

    init();
});
