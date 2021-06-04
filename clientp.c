#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include<netinet/in.h>
#include<sys/types.h>
#include<netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WINDOW 5 //Amount of sgments to be sent in one succession
#define BFFR_SZ 500   //Size of buffer utilized when sending or recieivng video data packets

// structure for carrying video data packets 

struct video_packet {

    int data_seq;
    size_t video_data_sz_lft;
    char data[BFFR_SZ];

};

// structure for carrying video data received acknowlgements 

struct data_ack {

    int data_seq;
    char data[WINDOW];

};


//global variables 

int global; // stores data sequence of video data packets
int iters; // used to iterate through loops
int vd_pckt_nmb; // determines the video data packet number
int wait_more; // determines teh amount of additional wait required
int rsnd_hndlr; // serves the function of a resending handler in case of data packet loss
int sckt_hndlr; // server the function of a socket handler

long dt_srvr_prt; // server the function of main server port to send video

FILE *video_file; // initlizes the video file to be opened
struct hostent *hos;

struct sockaddr_in srvr; //structure for the main server to send video
int first_sent = 0;

size_t size_left; // determines the size of video left to be sent
size_t rd_rslt; // initialized to read the result of the transfer
size_t total_size; // employed to determine the total size of the video
time_t tm_strt; // used to specify the starting time of data transfer to help in counting overall time spent in trasnfer

struct video_packet vid_packets[WINDOW]; // actual apckets used to transfer the video data
struct data_ack cummulative_ack; // acknowledgements for the video data packets sent to the server

int snd_hndlr; // serves the function of a resending handler
int ack_hndlr; // server the function of an acknowldgement handler

char *video_file_name = "earth.mp4"; //stores the name of the video file to be read and sent to the server

// used to initialize data packet acknowledgements and the actual packets for video transfer



// serves the function of reading the video file and filling the 
// data packets accordingly.
// returns the number of data packets successfully filled



int add_data_to_packets() {

    vd_pckt_nmb = 0;
    iters = 0;

    while (iters < WINDOW) {

        // Responsible for filling the video data packets 

        rd_rslt = fread(vid_packets[iters].data, BFFR_SZ, 1, video_file);
        vid_packets[iters].video_data_sz_lft = size_left;
        vid_packets[iters].data_seq = global;

        // used to iterate throgh loop variables by updating them

        vd_pckt_nmb++;
        global+= 1;
        
        if(vid_packets[iters].video_data_sz_lft >= BFFR_SZ){
        	size_left -= BFFR_SZ;
        
        }
        else{
        
        	size_left -= vid_packets[iters].video_data_sz_lft;
        
        }
        
        
        

        // checks for exceptions

        if (rd_rslt != 1 && vid_packets[iters].video_data_sz_lft == 0) {

            if (feof(video_file)) return vd_pckt_nmb;
            printf("\n ERROR! Video file could not be read!\n");
            return -1;

        }

        // increments the loop counter

        iters++;

    }
    
    // returns 0 in case EOF
    // returns -1 in case of ERROR

    return vd_pckt_nmb;

}

// Employed to wait untill acknowlegements for all packets are receieved,
// and all lost data packets are resent successfully.


int wait_for_packets() {

    // Serves to wait untill all data packets are successully receieved
    
    recvfrom(sckt_hndlr, &cummulative_ack, sizeof(struct data_ack), 0, NULL, NULL);

    // sets the flag to wait even more 
    
    wait_more = 0;

    // verifies acknowlegements of the packets sent and resends the packet not acknowledged 
    
    iters = 0;

    while ( iters < WINDOW ) {
    
    
    	if((global/5)<cummulative_ack.data_seq){
    		wait_more = 1;
    		continue;
    	}
 
        if (cummulative_ack.data[iters] == '0') {
 
            rsnd_hndlr = sendto(sckt_hndlr, &vid_packets[iters], sizeof(struct video_packet), 
                                    0,(struct sockaddr *) &srvr, sizeof(srvr));

            // sets the flag to wait even more 
 
            wait_more = 1;
 
        }
        
        else{
        	first_sent++;
        
        }
 
        iters++;
 
    }
    
    // Returns  0 in case all acknowledgements are succesfully recieved
    
    
    if (wait_more == 0){
    	return wait_more;
    }
    else{
    	return wait_for_packets();
    }
    

    

}

int main(int argc, char *argv[]) {

    // Makes sure that correct number of arguments are passsed 

    if (argc < 3) {

        printf("ENTER OTHER ARGUMENTS!");
        return -1;

    }

    // Serves to pass the arguments onto the server port

    dt_srvr_prt = atoi(argv[2]);

    sckt_hndlr = socket(AF_INET, SOCK_DGRAM, 0);

    if (sckt_hndlr == -1) {

        printf("\n ERROR! Socket could not be created!\n");
        return -1;

    }

    printf(" Socket has been successfully created! \n");
    
    
    
    //Assigning address
    srvr.sin_family = AF_INET;
    hos = gethostbyname(argv[1]);
    bcopy((char *)hos->h_addr, (char *)&srvr.sin_addr, hos->h_length);
    
    
    
    
    srvr.sin_port = htons(dt_srvr_prt);

    global = 0;
    video_file = fopen(video_file_name, "r");

    if (video_file == NULL) {
    	printf("\nERROR! Video could not be opened successfully!\n");
    	}
    
    //Finding file size
    fseek(video_file, 0, SEEK_END);
    size_left = ftell(video_file);
    fseek(video_file, 0, SEEK_SET);

    // Initialize data packets and their acknowlgements
    
    cummulative_ack.data_seq = -1;
    iters = 0;

    while (iters < WINDOW ) {

        vid_packets[iters].data_seq = 0;
        vid_packets[iters].video_data_sz_lft = 0;
        cummulative_ack.data[iters] = '-';
        iters++;

    }
    
    
    total_size = size_left;
    tm_strt = time(NULL);

    // continous loop employed to run till video file is sent successfully
    while (1) {

        // Responsible for reading the video file and filling the respective data packets
        vd_pckt_nmb = add_data_to_packets();

        // responsible for sending the recently filled data packets to the opened server port
        iters = 0;
      
        while (iters < vd_pckt_nmb ) {
           
            snd_hndlr = sendto(sckt_hndlr, &vid_packets[iters], sizeof(vid_packets[iters]), 0,
                                      (struct sockaddr *) &srvr, sizeof(srvr));
            if (snd_hndlr == -1) {
                printf("\n ERROR! Video could not be sent video_packet!\n");
                return -1;
            }
            
            printf(" VIDEO UPLOADED: %lu%% | Time Elpased: %lds\r",
                   ((total_size - size_left) * 100) / total_size,
                   time(NULL) - tm_strt);
            fflush(stdout);


            
            iters++;
        }

        // Employeed to wait for the acknowledgements of teh packets sent recently
     
        ack_hndlr = wait_for_packets();
        

        // checks if all packets are succesfully sent and acknowledged and exist the loop
     
        if (size_left == 0) {
            printf("\n VIDEO FILE SENT!\n");
            break;
    
        }
  
    }

    // Used to clean up the descriptor of video file
 
    fclose(video_file);
    close(sckt_hndlr);

    return 0;

}
