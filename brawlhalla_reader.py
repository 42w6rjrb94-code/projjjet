import pymem
import pymem.process
import time

# language: python
# file/module: brawlhalla_reader.py
# runtime/version: python 3.10+

def read_game_data():
    pm = pymem.Pymem("Brawlhalla.exe")
    game_module = pymem.process.module_from_name(pm.process_handle, "Brawlhalla.exe")
    client_base = game_module.lpBaseOfDll
    
    print(f"connecté à Brawlhalla.exe (Base: {hex(client_base)})")
    
    while True:
        try:
            # Exemple de boucle de lecture de positions (offsets à affiner)
            # time.sleep(0.1)
            pass
        except Exception as e:
            print(f"erreur de lecture: {e}")
            break

if __name__ == "__main__":
    read_game_data()
