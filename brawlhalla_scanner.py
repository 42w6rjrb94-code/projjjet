import pymem
import pymem.pattern

# language: python
# file/module: brawlhalla_scanner.py
# runtime/version: python 3.10+

def find_player_pattern():
    pm = pymem.Pymem("Brawlhalla.exe")
    signature = b"\x48\x8B\x05\x00\x00\x00\x00\x48\x85\xC0\x74\x0A"
    
    # Utilisation correcte pour pymem.pattern.pattern_scan_module
    address = pymem.pattern.pattern_scan_module(
        pm.process_handle, signature, 
        buffer_size=0x1000
    )
    if address:
        print(f"found base pattern at: hex({address})")
    else:
        print("pattern not found, check dump.")

if __name__ == "__main__":
    find_player_pattern()
