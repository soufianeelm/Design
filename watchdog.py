"""
The browsers will be closed when the autologin script is terminated.
The script is terminated in these cases:
- The autologin script is not running anymore.
- The heartbeat file is not updated for more than 5 minutes.
- The heartbeat file is not found.
- The heartbeat file is not readable.

These cases can be modified in the watchdog.py script. it's an important matter to consider as
the production is depending on one of the browsers (POD dashboard) to stay open.

Author: Soufiane El mouahid
"""

import psutil
import subprocess
import time
import logging
import os
from datetime import datetime, timedelta

SCRIPT_CMD = ["python", "autologin.py"]
HEARTBEAT_PATH = "heartbeat.txt"
TIMEOUT_SECONDS = 300  # 5 minutes without heartbeat = freeze detected

logging.basicConfig(
    filename="watchdog.log",
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(message)s"
)

def is_heartbeat_stale():
    if not os.path.exists(HEARTBEAT_PATH):
        return True
    try:
        mod_time = datetime.fromtimestamp(os.path.getmtime(HEARTBEAT_PATH))
        return (datetime.now() - mod_time) > timedelta(seconds=TIMEOUT_SECONDS)
    except Exception as e:
        logging.warning(f"Impossible to read the heartbeat : {e}")
        return True

def kill_process_tree(pid):
    try:
        parent = psutil.Process(pid)
        children = parent.children(recursive=True)
        for child in children:
            child.kill()
        parent.kill()
        logging.warning(f"Process {pid} and it's child processes have been killed.")
    except psutil.NoSuchProcess:
        logging.warning(f"Process {pid} already done.")

while True:
    logging.info("Initializing autologin.py")
    process = subprocess.Popen(SCRIPT_CMD)
    time.sleep(30) # Wait for the autologin script to initialize

    while True:
        time.sleep(10) # Check its state every 10 seconds

        # Check if the autologin process is still running
        if process.poll() is not None:
            logging.warning("autologin.py has terminated. Reboot.")
            break
        
        logging.info("autologin.py is running.")

        # Check if the heartbeat file is stale
        if is_heartbeat_stale():
            logging.error("Freeze detected : no recent heartbeat. Reboot.")

            try:
                kill_process_tree(process.pid)
                time.sleep(2)
            except Exception as e:
                logging.error(f"Failed to kill autologin.py process: {e}")
                raise
            break

        logging.info("Heartbeat is fresh.")

        # Maybe add a check to ensure both browsers are open (if a browser is closed but not due to an error, the program won't detect it)

    logging.info("Autologin script reboot in 5 seconds.")
    time.sleep(5)  # Wait before restarting the script
