document.addEventListener('DOMContentLoaded', () => {
    const loginSection = document.getElementById('login-section');
    const dashboardSection = document.getElementById('dashboard-section');
    
    const loginForm = document.getElementById('login-form');
    const loginMessage = document.getElementById('login-message');

    const dashboardMessage = document.getElementById('dashboard-message');
    const currentUserSpan = document.getElementById('current-username');
    const logoutButton = document.getElementById('logout-button');

    const userManagementSection = document.getElementById('user-management');
    
    const addUserForm = document.getElementById('add-user-form');
    const usersTableBody = document.querySelector('#users-table tbody');
    
    const addStationForm = document.getElementById('add-station-form');
    const stationsTableBody = document.querySelector('#stations-table tbody');

    const API_URL = ""; // The UI is served from the same origin as the API
    let currentUser = null; // Store current user profile

    /**
     * UTILITY FUNCTIONS
     */

    async function apiRequest(endpoint, method = 'GET', body = null) {
        const token = localStorage.getItem('access_token');
        // No token needed for the login endpoint itself
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
                // Token is invalid or expired
                console.log('Token expired or invalid. Logging out.');
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

    /**
     * UI TOGGLE FUNCTIONS
     */

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

    /**
     * DATA LOADING AND RENDERING
     */

    async function loadUserProfile() {
        const response = await apiRequest('/api/profile');
        if (!response || !response.ok) {
            showLogin();
            return;
        }
        
        currentUser = await response.json();
        currentUserSpan.textContent = currentUser.username;

        if (currentUser.is_admin) {
            userManagementSection.style.display = 'block';
            await loadAdminData();
        } else {
            userManagementSection.style.display = 'none';
            await loadUserData();
        }
    }

    async function loadAdminData() {
        await loadUsers();
        await loadStations(true);
    }
    
    async function loadUserData() {
        await loadStations(false);
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

    async function loadStations(isAdmin = false) {
        const endpoint = isAdmin ? '/api/admin/stations' : '/api/user/stations';
        const response = await apiRequest(endpoint);
        if (!response || !response.ok) return;
        
        try {
            const stations = await response.json();
            stationsTableBody.innerHTML = '';
            stations.forEach(station => {
                const row = document.createElement('tr');
                // For non-admins, the secret is not sent. Handled this in the backend already.
                const secret = station.secret || 'N/A (Hidden)';
                row.innerHTML = `
                    <td>${station.id}</td>
                    <td>${station.station_id}</td>
                    <td>${secret}</td> 
                    <td>${station.user_id}</td>
                    <td><button class="delete-station" data-id="${station.id}">Delete</button></td>
                `;
                // Hide delete button for non-admins
                if (!isAdmin) {
                    row.querySelector('.delete-station').style.display = 'none';
                }
                stationsTableBody.appendChild(row);
            });
        } catch (error) {
            console.error("Failed to process stations response.", error);
        }
    }
    
    /**
     * EVENT HANDLERS
     */

    loginForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const username = loginForm.username.value;
        const password = loginForm.password.value;

        try {
            const response = await apiRequest('/api/login', 'POST', { username, password });
            
            if (!response) return; // Error already handled in apiRequest
            console.log(response)
            const data = await response.json();
            if (response.ok) {
                localStorage.setItem('access_token', data.access_token);
                await showDashboard();
            } else {
                loginMessage.textContent = data.msg || 'Login failed.';
                loginMessage.style.color = 'red';
            }
        } catch (error) {
            throw(error);
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
            await loadStations(true);
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
        if (e.target.classList.contains('delete-station')) {
            const stationId = e.target.dataset.id;
            if (confirm(`Are you sure you want to delete station ${stationId}?`)) {
                const response = await apiRequest(`/api/admin/stations/${stationId}`, 'DELETE');
                if (response && response.ok) {
                    await loadStations(true);
                } else {
                    const data = response ? await response.json() : { msg: "Request failed" };
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
