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
import pygetwindow as gw
from datetime import datetime, timedelta

SCRIPT_CMD = ["python", "C:/Users/XXX/Autologin/autologin.py"]
HEARTBEAT_PATH = "C:/Users/XXX/Autologin/heartbeat.txt"
TIMEOUT_SECONDS = 300  # 5 minutes without heartbeat = freeze detected

logging.basicConfig(
    filename="C:/Users/XXX/Autologin/watchdog.log",
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
        logging.error(f"Impossible to read the heartbeat : {e}")
        return True
    
def both_windows_open():
    windows = gw.getWindowsWithTitle("")
    titles = [w.title for w in windows if w.title.strip() != ""]

    kpi_found = any("Dashboards" in t for t in titles)
    pod_found = any("POD" in t or "SAP" in t for t in titles)

    return kpi_found and pod_found

def kill_process_tree(pid):
    try:
        parent = psutil.Process(pid)
        children = parent.children(recursive=True)
        for child in children:
            child.kill()
        parent.kill()
        logging.warning(f"Process autologin (PID {pid}) and its child processes have been killed.")
    except psutil.NoSuchProcess:
        logging.error(f"Process {pid} is already done.")

def kill_all_msedge():
    for proc in psutil.process_iter(attrs=["pid", "name"]):
        if proc.info["name"] and proc.info["name"].lower() == "msedge.exe":
            try:
                psutil.Process(proc.info["pid"]).kill()
                logging.warning(f"Process msedge (PID {proc.info['pid']}) killed before autologin start.")
            except (psutil.NoSuchProcess, psutil.AccessDenied) as e:
                logging.error(f"Could not kill msedge (PID {proc.info['pid']}): {e}")

def log_kill(pid, reason):
    logging.error(reason)

    subprocess.Popen(["start", "cmd", "/c", "python", "C:/Users/XXX/Autologin/error_popup.py"], shell=True)

    time.sleep(10)  # Wait for the popup to be displayed

    kill_process_tree(pid)
    
while True:
    logging.info("------------------------------------------------")
    logging.info("=== Initializing autologin.py ===")

    kill_all_msedge()  # Ensure no Edge processes are running before starting the script
    
    try:
        process = subprocess.Popen(SCRIPT_CMD)
        time.sleep(30) # Wait for the autologin script to initialize
        logging.info("autologin.py started successfully.")
    except Exception as e:
        logging.error(f"Failed to start autologin.py: {e}")
        continue
    logging.info("Monitoring autologin.py...")

    loop_count = 0

    while True:
        loop_count += 1

        # Check if the autologin process is still running
        if process.poll() is not None:
            log_kill(process.pid, "autologin.py has unexpectedly stopped running.")
            break
        if loop_count >= 6:
            logging.info("autologin.py is running.")

        # Check if the heartbeat file is stale
        if is_heartbeat_stale():
            log_kill(process.pid, "Freeze detected : no recent heartbeat.")
            break
        if loop_count >= 6:
            logging.info("Heartbeat is fresh.")

        # Check if both Edge windows are still open
        if not both_windows_open():
            log_kill(process.pid, "One of the Edge windows was closed manually.")
            break
        if loop_count >= 6:
            logging.info("Both Edge windows are open.")
            loop_count = 0

        time.sleep(10) # Check its state every 10 seconds

    logging.warning("Autologin script reboot in 5 seconds.")
    time.sleep(5)  # Wait before restarting the script
