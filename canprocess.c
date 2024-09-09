#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>

int main(int argc, char **argv) {
    int fdSocketCAN;
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct can_frame frame;
    pid_t pid;
    int pipefd[2];

    // Créer un socket CAN
    if ((fdSocketCAN = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("Socket");
        return -1;
    }

    // Configurer l'interface CAN
    strcpy(ifr.ifr_name, "vcan0");
    ioctl(fdSocketCAN, SIOCGIFINDEX, &ifr);
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    bind(fdSocketCAN, (struct sockaddr *)&addr, sizeof(addr));

    // Créer une pipe pour la communication entre processus
    pipe(pipefd);

    // Créer le processus fils
    pid = fork();

    if (pid == 0) { // Code du processus fils
        struct can_frame received_frame;
        close(pipefd[1]); // Fermer l'écriture dans le fils

        while (1) {
            int nbytes = read(fdSocketCAN, &received_frame, sizeof(struct can_frame));
            if (nbytes > 0) {
                printf("Fils: Message reçu: 0x%03X [%d] ", received_frame.can_id, received_frame.can_dlc);
                for (int i = 0; i < received_frame.can_dlc; i++) {
                    printf("%02X ", received_frame.data[i]);
                }
                printf("\n");
+
                // Transférer le message au père via la pipe
                write(pipefd[0], received_frame.data, received_frame.can_dlc);
            }
        }
    } else { // Code du processus père
        close(pipefd[0]); // Fermer la lecture dans le père
        int choice;

        while (1) {
            printf("Menu:\n1. Envoyer un message CAN\n2. Quitter\nChoisissez une option: ");
            scanf("%d", &choice);

            if (choice == 1) {
                printf("Entrez le message à envoyer: ");
                scanf("%s", frame.data);
                frame.can_id = 0x565; // Remplacez par les trois derniers chiffres de votre numéro DA
                frame.can_dlc = strlen(frame.data);

                // Envoyer le message
                if (write(fdSocketCAN, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
                    perror("Write");
                } else {
                    printf("Père: Message envoyé: 0x%03X [%d] %s\n", frame.can_id, frame.can_dlc, frame.data);
                }

                // Lire le message reçu du fils
                char buffer[8]; // Ajustez la taille selon vos besoins
                read(pipefd[0], buffer, sizeof(buffer));
                printf("Père: Message reçu du fils: %s\n", buffer);
            } else {
                break;
            }
        }
        wait(NULL); // Attendre le processus fils
        close(fdSocketCAN);
    }
    return 0;
}
