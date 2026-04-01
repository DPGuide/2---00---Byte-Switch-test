#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <string.h>

#define BLOCK_SIZE 28
#define SHM_SIZE 29
#define SHM_NAME "Local\\MyCustomProtocol"
#define MUTEX_NAME "Local\\MyProtocolMutex"

// 120 KB Zielgröße für stabilen IRAM-Exploit
#define TARGET_SIZE_KB 120 
#define MAX_TICKS ((TARGET_SIZE_KB * 1024) / BLOCK_SIZE)

int main(int argc, char *argv[]) {
    int is_solo = (argc > 1) ? 1 : 0;
    HANDLE hMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SHM_SIZE, SHM_NAME);
    unsigned char* shm_buffer = (unsigned char*) MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, SHM_SIZE);
    HANDLE hMutex = CreateMutex(NULL, FALSE, MUTEX_NAME);

    shm_buffer[0] = 0x00;

    FILE *fHekate = fopen("hekate_ctcaer.bin", "rb");
    if (!fHekate) { printf("[ ERROR ] hekate_ctcaer.bin fehlt!\n"); return 1; }

    unsigned int tick = 0;
    unsigned char local_packet[BLOCK_SIZE];
    printf("[ SYSTEM ] Baue 120-KB-Payload (Limit: %u Ticks)...\n", MAX_TICKS);

    // Phase 1: Hekate (Erste ~16 KB / ca. 600 Ticks)
    while (tick < 600) {
        WaitForSingleObject(hMutex, INFINITE);
        if (!is_solo && shm_buffer[0] == 0x2A) { ReleaseMutex(hMutex); Sleep(1); continue; }

        memset(local_packet, 0, BLOCK_SIZE);
        if (fread(local_packet, 1, BLOCK_SIZE, fHekate) < BLOCK_SIZE) { ReleaseMutex(hMutex); break; }

        memcpy(shm_buffer + 1, local_packet, BLOCK_SIZE);
        shm_buffer[0] = 0x2A; 
        ReleaseMutex(hMutex);
        tick++;
    }
    fclose(fHekate);

    // Phase 2: GFX & Reboot Mix (Die Treffer-Zone)
    while (tick <= 800) {
        WaitForSingleObject(hMutex, INFINITE);
        if (!is_solo && shm_buffer[0] == 0x2A) { ReleaseMutex(hMutex); Sleep(1); continue; }
        
        memset(local_packet, 0, BLOCK_SIZE);

        if (tick % 2 == 0) {
            // GFX: Blaues Display (Adressen aus deiner di.c)
            unsigned int gfx_cmd[] = { 0xd2a54200, 0xd28001ff, 0xb9000c21, 0xd2800021, 0xb9000041 };
            memcpy(&local_packet[0], gfx_cmd, sizeof(gfx_cmd));
        } else {
            // Reboot-Befehl (Standard ARM)
            unsigned int reboot_cmd[] = { 0xd280ed00, 0xf2a70000, 0x52800101, 0xb9000001 };
            memcpy(&local_packet[0], reboot_cmd, sizeof(reboot_cmd));
        }
        
        local_packet[27] = 0xFF;
        memcpy(shm_buffer + 1, local_packet, BLOCK_SIZE);
        shm_buffer[0] = 0x2A;
        ReleaseMutex(hMutex);
        tick++;
    }

    // Phase 3: Flooding bis 120 KB
    while (tick < MAX_TICKS) {
        WaitForSingleObject(hMutex, INFINITE);
        if (!is_solo && shm_buffer[0] == 0x2A) { ReleaseMutex(hMutex); continue; }
    
        memset(local_packet, 0, BLOCK_SIZE);
        unsigned int reboot_cmd[] = { 0xd280ed00, 0xf2a70000, 0x52800101, 0xb9000001 };
        memcpy(&local_packet[0], reboot_cmd, sizeof(reboot_cmd));
    
        local_packet[27] = 0xEE;
        memcpy(shm_buffer + 1, local_packet, BLOCK_SIZE);
        shm_buffer[0] = 0x2A;
        ReleaseMutex(hMutex);
        tick++;
    }

    shm_buffer[0] = 0xFF; 
    printf("[ DONE ] Payload bei %u Ticks / ~120 KB fertiggestellt.\n", tick);
    return 0;
}