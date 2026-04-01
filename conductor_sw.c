#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <string.h>

#define BLOCK_SIZE 26
#define SHM_SIZE 27 // 1 Byte Handshake-Flag + 26 Bytes Payload
#define SMASH_POINT 28672 // 0x7000
#define SHM_NAME "Local\\MyCustomProtocol"
#define MUTEX_NAME "Local\\MyProtocolMutex"

int main(int argc, char *argv[]) {
    int is_solo = (argc > 1) ? 1 : 0;

    HANDLE hMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SHM_SIZE, SHM_NAME);
    unsigned char* shm_buffer = (unsigned char*) MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, SHM_SIZE);
    HANDLE hMutex = CreateMutex(NULL, FALSE, MUTEX_NAME);

    // Initialisiere das Handshake-Flag auf 0x00 (Freigegeben)
    shm_buffer[0] = 0x00;

    FILE *fHekate = fopen("hekate_ctcaer.bin", "rb");
    if (!fHekate) {
        printf("[ ERROR ] hekate_ctcaer.bin fehlt!\n");
        return 1;
    }

    unsigned int tick = 0;
    unsigned char local_packet[BLOCK_SIZE];
    printf("[ SYSTEM ] Baue Hybrid-Payload mit Salven-Injektion...\n");

    // Phase 1: Hekate
    while (tick < 1100) {
        WaitForSingleObject(hMutex, INFINITE);

        // Ping-Pong Check (JETZT AUCH IN PHASE 1)
        if (!is_solo && shm_buffer[0] == 0x2A) {
            ReleaseMutex(hMutex);
            Sleep(1); // Kurze Atempause für die CPU
            continue;
        }

        memset(local_packet, 0, BLOCK_SIZE);
        if (fread(local_packet, 1, BLOCK_SIZE, fHekate) < BLOCK_SIZE) {
            ReleaseMutex(hMutex);
            break;
        }

        // Kopiere Payload ab Index 1, Index 0 ist das IPC-Flag
        memcpy(shm_buffer + 1, local_packet, BLOCK_SIZE);
        shm_buffer[0] = 0x2A; 
        ReleaseMutex(hMutex);
        tick++;
    }
    fclose(fHekate);

    // Phase 2: DIE SALVE
    printf("[ ACTION ] Starte Salve (Ticks 1100 - 1115)...\n");
    while (tick <= 1115) {
        WaitForSingleObject(hMutex, INFINITE);
        
        // Ping-Pong Check (JETZT AUCH IN PHASE 2)
        if (!is_solo && shm_buffer[0] == 0x2A) {
            ReleaseMutex(hMutex);
            Sleep(1);
            continue;
        }
        
        memset(local_packet, 0, BLOCK_SIZE);
        
        unsigned int reboot_cmd[] = { 0xd280ed00, 0xf2a70000, 0x52800101, 0xb9000001 };
        memcpy(&local_packet[0], reboot_cmd, sizeof(reboot_cmd));
        local_packet[25] = 0xFF;
        
        memcpy(shm_buffer + 1, local_packet, BLOCK_SIZE);
        shm_buffer[0] = 0x2A;
        
        ReleaseMutex(hMutex);
        tick++;
    }
	// Phase 3: Intelligentes Fluten - Jetzt mit "Safe Zone" Limit
	printf("[ SYSTEM ] Starte Flooding (Limit: 4500 Ticks für IRAM-Kompatibilität)...\n");
	while (tick < 4500) { // Hier von 10000 auf 4500 runter!
		WaitForSingleObject(hMutex, INFINITE);
	
		if (!is_solo && shm_buffer[0] == 0x2A) {
			ReleaseMutex(hMutex);
			continue;
		}
	
		memset(local_packet, 0, BLOCK_SIZE);
		local_packet[0] = 0x2A;
	
		// Erhöhe die Spray-Dichte, da wir weniger Platz haben
		// Jedes 3. Paket ein Reboot-Versuch
		if (tick % 3 == 0) {
			unsigned int reboot_cmd[] = { 0xd280ed00, 0xf2a70000, 0x52800101, 0xb9000001 };
			memcpy(&local_packet[1], reboot_cmd, sizeof(reboot_cmd));
			local_packet[17] = 'S'; 
		} else {
			local_packet[15] = 'Y';
			local_packet[16] = 'B';
		}
	
		local_packet[25] = 0xFF;
		memcpy(shm_buffer + 1, local_packet, BLOCK_SIZE);
		shm_buffer[0] = 0x2A;
		ReleaseMutex(hMutex);
		tick++;
	}

    // Ende
    WaitForSingleObject(hMutex, INFINITE);
    shm_buffer[0] = 0xFF;
    ReleaseMutex(hMutex);

    printf("[ OK ] Salven-Payload fertig erstellt!\n");
    return 0;
}