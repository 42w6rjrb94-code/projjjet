from camoufox.sync_api import Camoufox

with Camoufox(headless=False, fingerprint_preset=False) as browser:
    page = browser.new_page()
    page.goto("https://google.com")
    input("Connexion reussie ! Pressez Entree...")
