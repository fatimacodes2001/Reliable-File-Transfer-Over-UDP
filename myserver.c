#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include <unistd.h>
#include <time.h>

#define WINDOW 5// Defines how many segment to send at one go
#define PACKET_SIZE 500   // Defines buffer sizes while sending and receiving payload

// Declaring global variables
    int stale = 0, established = 0, overall_seqnum = 0; //Connection state variables
    time_t time_start;		//Determining time elapsed
    size_t total_size;		//Variable representing file size
    FILE *vid_file;		//File pointer to point to the opened file



//Variables to establish socket connection

struct sockaddr_in reciever;
size_t len = sizeof(reciever);



// Struct that carries video data
struct data_packet {
    int data_seq;
    size_t size_left;
    char data[PACKET_SIZE];
};

// Struct that carries acknowledgements
//It uses cummulative acknowledgements
struct data_ack {
    int data_seq;
    char data[WINDOW];
};

	
//Variables to store data and acks
struct data_packet packets[WINDOW];
struct data_ack commulative_ack;
struct data_packet packet_init;

//Socket file descriptor
int socketfd;


//Function that saves the recieved packets in buffer
void save() {

    //Do nothing with a duplicate packet
    if ((packet_init.data_seq/5) < overall_seqnum) {
        return;
    }
    
    //Determining packet index
    int packet_loc = packet_init.data_seq % WINDOW;
    packets[packet_loc].data_seq = packet_init.data_seq;
    packets[packet_loc].size_left = packet_init.size_left;
    
    //Saving packet
    memcpy(packets[packet_loc].data, packet_init.data, sizeof(packet_init.data));
    
    printf("VIDEO FILE DOWNLOADED: %lu%% | TIME ELAPSED: %lds\r",
               ((total_size - packet_init.size_left) * 100) / (total_size),
              time(NULL) - time_start);
    fflush(stdout);
    
    
}



// Function to check if all packets have been recieved


int check() {


    int received_all = 1;
    
    for (int numb = 0; numb < WINDOW; numb++) {
        if (packets[numb].data_seq == -1) {
            commulative_ack.data[numb] = '0';
            
            //Setting flag to -1 even if one packet has not been recieved in the window
            received_all = -1;
        }
        
        commulative_ack.data[numb] = '1';
        if (packets[numb].size_left == 0) {
            return numb;
        }
    }
    
    //Returns WINDOW-1 if all packets have been recieved
    
    if (received_all == 1){
    	return WINDOW - 1;
    
    }
    
    //Returns -1 otherwise
    else{
    	return received_all;
    }
    
   
    
}


//Function to write data packets in the memory

void write_data(int packet_loc) {
	

    for (int numb = 0; numb <= packet_loc; numb++) {
    	size_t size;
    	
    	//Guard condition for last packet
    
    	if( packets[numb].size_left > PACKET_SIZE ){
    	
    	
    		 size = PACKET_SIZE;
    	}
    	else{
    	//If packet size is less than PACKET_SIZE then use its size_left attribute
    		 size = packets[numb].size_left;
    	}
    	
    	//Writing the data packet
    	fwrite(packets[numb].data, size, 1, vid_file);
          
    }
    overall_seqnum += 1;
    return ;
}





int main(int argc, char *argv[]) {
	//Start of main function


    // Ensuring that correct number of arguments have been entered
    if (argc < 2) {
    
    
        printf("ENTER PORT NUMB AS SECOMD ARGUMENT!");
        return -1;
    }

    //Parse argument to server port
    long port = atoi(argv[1]);


    //Initialize socket file descriptor
    socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    
    //Displaying error message
    if (socketfd == -1) {
        printf("\nFAILURE IN SOCKET CREATION!\n");
        return -1;
    }
    printf("SOCKET CREATION SUCCESSFUL!\n");
    fflush(stdout);
    

    
    
    //Determing binding state
    int bind_fd;
    
    //Initialzing address variables
    struct sockaddr_in recv_address;
    recv_address.sin_family = AF_INET;
    recv_address.sin_addr.s_addr = INADDR_ANY;
    recv_address.sin_port = htons(port);

    //Binding the port to the socket
    bind_fd = bind(socketfd, (struct sockaddr *) &recv_address, sizeof(recv_address));
    
    

    // Bind socket to port
    if (bind_fd < 0) {
        printf("\nERROR! FAILURE IN BINDING THE PORT!\n");
        return -1;
    }
    
    
    
    printf("RECIEVER ACTIVE! \nWaiting for sender to connect....\n");
    
    
    // Initialize vid_file pointer
    
    vid_file = fopen("earth-2.mp4", "w");
    
    //Displaying error message
    if (vid_file == NULL) {
        printf("\nERROR OPENING VIDEO FILE!\n");
    }
    
    
    //Initializing variables that store data and acks
    commulative_ack.data_seq = -1;
    for (int numb = 0; numb < WINDOW; numb++) {
        packets[numb].data_seq = -1;
        packets[numb].size_left = -1;
        commulative_ack.data[numb] = '-';
    }
    
    
    
    commulative_ack.data_seq = 0;
    int loop = 1;

    

    //Loop till sender connects & sends video file
    while(loop) {
    
    
        // If sender has connected
        if (stale == 0 && established == 1) {
            // Check if all WINDOW packets have been received
            
            int check_handler = check();
            if (check_handler != -1) {
            
            
                //Write packets to vid_file
                write_data(check_handler);
                
                commulative_ack.data_seq = overall_seqnum-1;

                // Send data_ack to reciever about all of WINDOW received packets
                int ack_handler = sendto(socketfd, &commulative_ack, sizeof(commulative_ack), 0,
                                         (const struct sockaddr *) &reciever, sizeof(reciever));
                                         
                //Handling sender error
                if (ack_handler == -1) {
                    printf("\nERROR SENDING ACKS!!!\n");
                    return -1;
                }


                // If last data_packet was received
                if (packets[check_handler].size_left == 0) {
                
                
                    // Closing video file
                    fclose(vid_file);
                    
                    // Closing socket connection
                    close(socketfd);

                    printf("\nVIDEO FILE RECIEVED SUCCESSFULLY!\n");
                    return 0;
                }

                // Reinitialize packets and data for further recieving
                
               commulative_ack.data_seq = -1;
    		for (int numb = 0; numb < WINDOW; numb++) {
        		packets[numb].data_seq = -1;
        		packets[numb].size_left = -1;
        		commulative_ack.data[numb] = '-';
        	
        	
    }
            }
            
                   
            
        } 
        
        else if (stale == 1) {
        //If packets have not been recieved for 0.3s
        
        
            // Mark received and unreceived packets
            for (int numb = 0; numb < WINDOW; numb++) {
            
            
                if (packets[numb].data_seq == -1) {
                    commulative_ack.data[numb] = '0';
                } else {
                    commulative_ack.data[numb] = '1';
                }
                
            }

            commulative_ack.data_seq = overall_seqnum;
            int ack_handler = sendto(socketfd, &commulative_ack, sizeof(commulative_ack), 0,
                                     (const struct sockaddr *) &reciever, sizeof(reciever));
           
            // Reset stale state
            stale = 0;
            
            
        }
        
        

        // Receive packets
        recvfrom(socketfd, &packet_init, sizeof(packet_init), 0,
                 (struct sockaddr *) &reciever, (socklen_t *) &len);


	//----------------RELIABILITY IMPLEMENTATION----------------------------


	//If no packet has been recieved
        if (packet_init.size_left < 0) {
        
            //No data_packet received
            if (established == 1) {
                stale++;
            }

	//Initialzing time delay variables
            struct timespec time1, time2;
            time1.tv_sec = 0;
            time2.tv_nsec = 300000000L;
            nanosleep(&time1, &time2);
        
           
                  
            
            
        } 
        else {
        
        
            //Set program in running state once the sender has connected
            
            if (established == 0) {
                established = 1;
                time_start = time(NULL);
                total_size = packet_init.size_left + 500;
            }

            //Reset stale state
            stale = 0;
            save(overall_seqnum);

        
                      
            
        }
        
        
    }
}
