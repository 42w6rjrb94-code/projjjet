import pymem
import pymem.process

# language: python
# file/module: brawlhalla_scanner.py
# runtime/version: python 3.10+

def find_player_pattern():
    pm = pymem.Pymem("Brawlhalla.exe")
    game_module = pymem.process.module_from_name(pm.process_handle, "Brawlhalla.exe")
    
    # Lecture directe de la mémoire via les offsets de base sans l'API pattern instable
    client_base = game_module.lpBaseOfDll
    print(f"processus accroché, base at: hex({client_base})")

if __name__ == "__main__":
    find_player_pattern()
