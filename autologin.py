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
import logging

# Logging configuration
logging.basicConfig(
    filename="autologin.log",
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(message)s"
)

logging.info("=== Autologin script started ===")

# Path to msedgedriver.exe (adjust as needed)
edge_service = Service('C:/Users/selmo/Documents/mubea/edgedriver_win64/msedgedriver.exe')

# Path to the Edge user data directories folder (adjust as needed)
user_data_dir = 'C:/Users/selmo/AppData/local/Microsoft/Edge'

# Configure options for KPI dashboard (adjust the user data directory path and name as needed)
options_KPI = Options()
options_KPI.add_argument(f'--user-data-dir={user_data_dir}/SAPDashboardUser')

try:
    # Lunch Edge with the specified profile for KPI dashboard
    driver_KPI = webdriver.Edge(service=edge_service, options=options_KPI)

    # Position and maximize the window on the secondary screen relative to the main screen (left side -1920, 0) (right side 1920, 0)
    driver_KPI.set_window_position(1920, 0)
    driver_KPI.maximize_window()

    logging.info("KPI dashboard browser launched and positioned.")
except Exception as e:
    logging.error(f"Failed to launch KPI dashboard browser: {e}")
    raise

try:
    # Open KPI dashboard (adjust the URL as needed)
    driver_KPI.get('https://www.netflix.com/browse')

    # Pause to allow the login page to load
    time.sleep(3)

    logging.info("KPI login page loaded.")
except Exception as e:
    logging.error(f"Failed to load KPI login page: {e}")
    raise

# connexion credentials for SAP (adjust as needed)
sap_user = 'Admin'
sap_pass = 'admin123'

try:
    # Enter credentials in the login form
    # user_input = driver_KPI.find_element(By.NAME, 'username')
    # user_input.send_keys(sap_user)

    # password_input = driver_KPI.find_element(By.NAME, 'password')
    # password_input.send_keys(sap_pass)

    # # Connect by pressing Enter
    # password_input.send_keys(Keys.RETURN)

    # Pause to allow the KPI dashboard to load
    time.sleep(3)
    logging.info("SAP credentials entered and login attempted.")
except Exception as e:
    logging.error(f"Failed to enter SAP credentials or login: {e}")
    raise

# Configure options for POD dashboard, using the default Edge user data directory is recommanded to keep the settings configuration
options_POD = Options()
options_POD.add_argument(f'--user-data-dir={user_data_dir}/User Data')

try:
    # Lunch Edge with the specified profile for POD dashboard
    driver_POD = webdriver.Edge(service=edge_service, options=options_POD)

    # Position and maximize the window on the main screen
    driver_POD.set_window_position(0, 0)
    driver_POD.maximize_window()

    logging.info("POD dashboard browser launched and positioned.")
except Exception as e:
    logging.error(f"Failed to launch POD dashboard browser: {e}")
    raise

try:
    # Open POD dashboard (adjust the URL as needed)
    driver_POD.get('https://opensource-demo.orangehrmlive.com/web/index.php/auth/login')
    
    # Pause to allow the login page to load
    time.sleep(3)  

    logging.info("POD login page loaded.")
except Exception as e:
    logging.error(f"Failed to load POD login page: {e}")
    raise

HEARTBEAT_TIME = 0

while True:
    try:
        # Simulate a click on the KPI dashboard browser to keep the SAP session active
        driver_KPI.execute_script("document.body.dispatchEvent(new MouseEvent('click', {bubbles: true}));")
    
        logging.info("Simulated activity click on KPI dashboard.")
    except Exception as e:
        logging.error(f"Error during simulated click: {e}")

    if HEARTBEAT_TIME <= 0:
        try:
            # heartbeat
            with open("heartbeat.txt", "w") as f:
                f.write("up")
            logging.info("Heartbeat signaled.")
            HEARTBEAT_TIME = 120  # Reset heartbeat time to 2 minutes
        except Exception as e:
            logging.error(f"Failed to update heartbeat: {e}")

    HEARTBEAT_TIME -= 5

    time.sleep(5)  # Click every 5 seconds

