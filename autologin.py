from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver.common.keys import Keys
from selenium.webdriver.edge.service import Service
from selenium.webdriver.edge.options import Options
import time

# Chemin vers le msedgedriver.exe
edge_service = Service('C:/Users/elmouahids/OneDrive - Mubea/Dokumente/Porsche project/KPI scripts/edgedriver_win64/msedgedriver.exe')

# Configurer les options
options_KPI = Options()
options_KPI.add_argument("--user-data-dir=C:/SAPDashProfiles/DashboardUser")

# Lancer Edge avec ce profil
driver_KPI = webdriver.Edge(service=edge_service, options=options_KPI)

# Positionner la fenêtre sur l'écran secondaire en fenêtre maximisée
driver_KPI.set_window_position(-1920, 0)
driver_KPI.maximize_window()

# Ouvrir la page SAP
driver_KPI.get('https://www.youtube.com')

# Pause pour laisser charger la page complètement
time.sleep(10)

# # Infos de connexion SAP
sap_user = 'ton_utilisateur'
sap_pass = 'ton_mot_de_passe'

# Remplacer par les vrais identifiants HTML de SAP
user_input = driver_KPI.find_element(By.NAME, 'search_query')
user_input.send_keys(sap_user)

# password_input = driver_KPI.find_element(By.ID, 'id_champ_mdp')
# password_input.send_keys(sap_pass)

# # Connecter (souvent touche entrée suffit)
user_input.send_keys(Keys.RETURN)

# Pause supplémentaire pour valider visuellement
time.sleep(20)

# Chemin vers le profil utilisateur Edge (ajuste en fonction du nom du profil)
user_data_dir = 'C:/Users/elmouahids/AppData/Local/Microsoft/Edge/User Data'

# Configurer les options
options_POD = Options()
options_POD.add_argument(f'--user-data-dir={user_data_dir}')

# Lancer Edge avec ce profil
driver_POD = webdriver.Edge(service=edge_service, options=options_POD)

# Positionner la fenêtre sur l'écran principal en fenêtre maximisée
driver_POD.set_window_position(0, 0)
driver_POD.maximize_window()

# Ouvrir la page SAP
driver_POD.get('https://www.youtube.com')

while True:
    time.sleep(86400)  # Garde le navigateur ouvert indéfiniment