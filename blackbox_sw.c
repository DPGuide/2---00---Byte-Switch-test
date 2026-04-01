#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <string.h>

#define BLOCK_SIZE 26
#define SHM_SIZE 27 // 1 Byte Handshake-Flag + 26 Bytes Payload
#define BUFFER_CAPACITY 10000 
#define SHM_NAME "Local\\MyCustomProtocol"
#define MUTEX_NAME "Local\\MyProtocolMutex"

int main() {
    FILE *log_file = fopen("blackbox.bin", "wb");
    if (!log_file) return 1;

    printf("--- BLACKBOX: REKORDER AKTIV ---\n");

    HANDLE hMap = NULL;
    HANDLE hMutex = NULL;

    while (!hMap || !hMutex) {
        hMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, SHM_NAME);
        hMutex = OpenMutex(SYNCHRONIZE, FALSE, MUTEX_NAME);
        if (!hMap || !hMutex) Sleep(50);
    }

    printf("[ SYSTEM ] Verbindung steht. Zeichne auf...\n");

    // SHM_SIZE anpassen
    unsigned char* shm_ptr = (unsigned char*) MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, SHM_SIZE);
    unsigned char *disk_buffer = (unsigned char*) malloc(BUFFER_CAPACITY * BLOCK_SIZE);

    int frames_in_buffer = 0;
    int running = 1;

    while (running) {
        WaitForSingleObject(hMutex, INFINITE);

        if (shm_ptr[0] == 0xFF) {
            printf("[ INFO ] Ende-Signal empfangen. Frames im Buffer: %d\n", frames_in_buffer);
            shm_ptr[0] = 0xAA; 
            ReleaseMutex(hMutex);
            running = 0;
            break;
        }

        if (shm_ptr[0] == 0x2A) {
            // WICHTIG: Wir lesen ab shm_ptr + 1, um das Handshake-Flag (Byte 0) zu ignorieren!
            memcpy(&disk_buffer[frames_in_buffer * BLOCK_SIZE], shm_ptr + 1, BLOCK_SIZE);
            frames_in_buffer++;

            // Signal an Conductor, dass wir fertig sind
            shm_ptr[0] = 0x00; 

            if (frames_in_buffer >= BUFFER_CAPACITY) {
                fwrite(disk_buffer, BLOCK_SIZE, frames_in_buffer, log_file);
                fflush(log_file);
                frames_in_buffer = 0;
            }
        }

        ReleaseMutex(hMutex);
        Sleep(1); // CPU etwas entlasten
    }

    if (frames_in_buffer > 0) {
        fwrite(disk_buffer, BLOCK_SIZE, frames_in_buffer, log_file);
        fflush(log_file);
    }

    printf("[ OK ] Aufnahme beendet. Datei 'blackbox.bin' ist bereit.\n");

    fclose(log_file);
    free(disk_buffer);
    UnmapViewOfFile(shm_ptr);
    CloseHandle(hMap);
    CloseHandle(hMutex);
    return 0;
}