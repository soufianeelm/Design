"""
Automates the login process for the SAP KPI dashboard using Microsoft Edge and opens the POD dashboard in a separate Edge window.
This script performs the following steps:
1. Launches Microsoft Edge with a specified user profile for the KPI dashboard.
2. Navigates to the KPI dashboard URL and automatically enters the provided SAP credentials to log in.
3. Waits for the KPI dashboard to load.
4. Launches a second Microsoft Edge window with a different user profile for the POD dashboard.
5. Navigates to the POD dashboard URL and opens the window for manual login.
Notes:
- The login for the POD dashboard is not automated and must be performed manually by the station's operator.
- Ensure that the paths to the Edge driver and user data directories, as well as the SAP credentials and dashboard URLs, are correctly set before running the script.
- The script keeps both browser windows open indefinitely.

Author: Soufiane El mouahid
"""
from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver.common.keys import Keys
from selenium.webdriver.edge.service import Service
from selenium.webdriver.edge.options import Options

import time

# Path to msedgedriver.exe (adjust as needed)
edge_service = Service('PATH_TO/msedgedriver.exe')

# Configure options for KPI dashboard (adjust the user data directory path as needed, same path as the default Edge user data directory can be used)
options_KPI = Options()
options_KPI.add_argument("--user-data-dir=PATH_TO/SAPDashboardUser")
options_KPI.add_argument("--no-startup-window-restore")

# Lunch Edge with the specified profile for KPI dashboard
driver_KPI = webdriver.Edge(service=edge_service, options=options_KPI)

# Position and maximize the window on the secondary screen relative to the main screen (left side -1920, 0) (right side 1920, 0)
driver_KPI.set_window_position(-1920, 0)
driver_KPI.maximize_window()

# Open KPI dashboard (adjust the URL as needed)
driver_KPI.get('LINK_TO_KPI_DASHBOARD')

# Pause to allow the login page to load
time.sleep(10)

# connexion credentials for SAP (adjust as needed)
sap_user = 'USERNAME'
sap_pass = 'PASSWORD'

# Enter credentials in the login form
user_input = driver_KPI.find_element(By.ID, 'j_username')
user_input.send_keys(sap_user)

password_input = driver_KPI.find_element(By.ID, 'j_password')
password_input.send_keys(sap_pass)

# Connect by pressing Enter
password_input.send_keys(Keys.RETURN)

# Pause to allow the KPI dashboard to load
time.sleep(30)

# Path to the user data directory for POD (adjust as needed) it is recommended here to use the default Edge user data directory
user_data_dir = 'PATH_TO/User Data'

# Configure options for POD dashboard
options_POD = Options()
options_POD.add_argument(f'--user-data-dir={user_data_dir}')

# Lunch Edge with the specified profile for POD dashboard
driver_POD = webdriver.Edge(service=edge_service, options=options_POD)

# Position and maximize the window on the main screen
driver_POD.set_window_position(0, 0)
driver_POD.maximize_window()

# Open POD dashboard (adjust the URL as needed)
driver_POD.get('LINK_TO_POD_DASHBOARD')

while True:
    time.sleep(86400)  # Keep the script running indefinitely