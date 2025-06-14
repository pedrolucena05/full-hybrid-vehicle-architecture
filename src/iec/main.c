#include "iec.h"

#ifndef TESTING
int main() {
    // Configure signal handlers for graceful shutdown and pause
    signal(SIGUSR1, handle_signal);
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    int status = iec_initializer();

    printf("IEC Module Running\n");

    system("clear");
    
    // Main loop of the IEC module
    while (running) {
        if (!paused) {

            // Receive commands from the VMU through the message queue
            cmd = iec_receive(cmd);
            
            sem_wait(sem); // Acquire the semaphore to protect shared memory
            if (!cmd.toVMU) {

                treatValues();

                cmd.fuelIEC = fuel;
                cmd.rpm_iec = iecRPM;
                cmd.iecActive = iecActive;
                strcpy(cmd.check, "ok");
                cmd.toVMU = true;
                mq_send(iec_mq, (const char *)&cmd, sizeof(cmd), 0);
                
            }

            else {
                mq_send(iec_mq, (const char *)&cmd, sizeof(cmd), 0);
                
            }
            
            sem_post(sem);

        } else {
            sleep(1); // Sleep for 1 second if paused
        }
        
        usleep(5000);
        printf("\033[5A");
        printf("\nIEC usage: %f", iecPercentage);
        printf("\nIEC Fuel (liters): %f", fuel);
        printf("\nIEC engine RPM: %f", iecRPM);
        printf("\nIEC gear: %d", gear);
        printf("\nVehicle speed: %f", localVelocity);
        
    }

    // Cleanup resources before exiting
    iecCleanUp();

    printf("[IEC] Shut down complete.\n");
    return 0;
}
#endif