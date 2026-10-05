#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define FRAME_WIDTH  320
#define FRAME_HEIGHT 240
#define PIXEL_BYTES  4
#define FRAME_SIZE   (FRAME_WIDTH * FRAME_HEIGHT * PIXEL_BYTES)

// Hardware Addresses (Update DMA_BASE to match Vivado Address Editor)
#define DMA_BASE  0x40400000
#define MEM_IN    0x1E000000
#define MEM_OUT   0x1E100000

int main() {
    // 1. Bypass OS to access physical memory
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("Failed to open /dev/mem");
        return -1;
    }

    volatile unsigned int *dma = (volatile unsigned int *)mmap(
        NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, DMA_BASE);

    unsigned char *in_map = (unsigned char *)mmap(
        NULL, FRAME_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, MEM_IN);

    unsigned char *out_map = (unsigned char *)mmap(
        NULL, FRAME_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, MEM_OUT);

    if (dma == MAP_FAILED || in_map == MAP_FAILED || out_map == MAP_FAILED) {
        perror("mmap failed");
        return -1;
    }

    // 2. Setup TCP Socket Server
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port        = htons(5000);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 1);
    printf("Diagnostic Video Server Ready on Port 5000...\n");

    int conn = accept(server_fd, NULL, NULL);
    printf("PC Connected!\n");

    while (1) {
        // 3. Receive full frame from network directly into FPGA input RAM
        int bytes_received = 0;
        while (bytes_received < FRAME_SIZE) {
            int n = recv(conn, in_map + bytes_received, FRAME_SIZE - bytes_received, 0);
            if (n <= 0) goto cleanup;
            bytes_received += n;
        }

        // 4. Soft Reset DMA Engine
        dma[0x00 / 4] = 0x04;   // Reset MM2S
        dma[0x30 / 4] = 0x04;   // Reset S2MM
        usleep(1000);           // Wait 1 ms for reset to complete

        // 5. Configure and Trigger DMA
        dma[0x30 / 4] = 0x01;           // Start S2MM (Receive)
        dma[0x48 / 4] = MEM_OUT;        // Destination Address
        dma[0x58 / 4] = FRAME_SIZE;     // Set Length (Triggers S2MM)

        dma[0x00 / 4] = 0x01;           // Start MM2S (Transmit)
        dma[0x18 / 4] = MEM_IN;         // Source Address
        dma[0x28 / 4] = FRAME_SIZE;     // Set Length (Triggers MM2S)

        // 6. Poll DMA Status Registers with Timeout
        int timeout = 500;
        int mm2s_idle = 0, s2mm_idle = 0;

        while (timeout > 0) {
            mm2s_idle = (dma[0x04 / 4] & 0x02); // Check bit 1 (Idle)
            s2mm_idle = (dma[0x34 / 4] & 0x02);
            if (mm2s_idle && s2mm_idle) break;
            usleep(1000);
            timeout--;
        }

        if (timeout == 0) {
            printf("[DMA HANG] MM2S: 0x%08X | S2MM: 0x%08X\n",
                   dma[0x04 / 4], dma[0x34 / 4]);
            continue;
        }

        // 7. Send processed frame from RAM back to PC
        int bytes_sent = 0;
        while (bytes_sent < FRAME_SIZE) {
            int n = send(conn, out_map + bytes_sent, FRAME_SIZE - bytes_sent, 0);
            if (n <= 0) goto cleanup;
            bytes_sent += n;
        }
    }

cleanup:
    close(conn);
    close(server_fd);
    munmap((void *)dma, 0x1000);
    munmap(in_map, FRAME_SIZE);
    munmap(out_map, FRAME_SIZE);
    close(fd);
    return 0;
}
