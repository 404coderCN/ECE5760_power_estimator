///////////////////////////////////////
/// 640x480 version! 16-bit color
/// This code will segfault the original
/// DE1 computer
/// compile with
/// gcc graphics_video_16bit.c -o gr -O2 -lm
///
///////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <sys/mman.h>
#include <sys/time.h> 
#include <math.h>
#include <string.h>
#include <errno.h>    // Definition for "error handling"
#include <sys/wait.h> // Definition for wait()
//#include "address_map_arm_brl4.h"

// threads
#include <pthread.h>

/****************************************************************************************
 *                              Prameter Definition
****************************************************************************************/

//============= VGA PLOT PARAMS ========================
/* Cyclone V FPGA devices */
#define HW_REGS_BASE          0xff200000
//#define HW_REGS_SPAN        0x00200000 
#define HW_REGS_SPAN          0x00005000 

#define FPGA_ONCHIP_BASE      0xC8000000
//#define FPGA_ONCHIP_END       0xC803FFFF
// modified for 640x480
// #define FPGA_ONCHIP_SPAN      0x00040000
#define FPGA_ONCHIP_SPAN      0x00080000

#define FPGA_CHAR_BASE        0xC9000000 
#define FPGA_CHAR_END         0xC9001FFF
#define FPGA_CHAR_SPAN        0x00002000

//============= ADDRESS OFFSET =============
#define FPGA_CURRENT       	0x00000070
// #define FPGA_ENERGY        	0x00000080
#define FPGA_CYCLE_CNT     	0x00000090
#define FPGA_START         	0x000000a0
#define FPGA_STOP          	0x000000b0
#define FPGA_RESET	       	0x000000c0
#define FPGA_DATA_VALID    	0x000000d0
#define FPGA_CALIBRATE    	0x000000f0
#define ENERGY_OUT_LOWER    0x00000200
#define ENERGY_OUT_UPPER    0x00000100
//======================================================
/* function prototypes */
void VGA_text (int, int, char *);
void VGA_text_clear();
void VGA_box (int, int, int, int, short);
void VGA_line(int, int, int, int, short) ;
void VGA_disc (int, int, int, short);

// 16-bit primary colors
#define red  (0+(0<<5)+(31<<11))
#define dark_red (0+(0<<5)+(15<<11))
#define green (0+(63<<5)+(0<<11))
#define dark_green (0+(31<<5)+(0<<11))
#define blue (31+(0<<5)+(0<<11))
#define dark_blue (15+(0<<5)+(0<<11))
#define yellow (0+(63<<5)+(31<<11))
#define cyan (31+(63<<5)+(0<<11))
#define magenta (31+(0<<5)+(31<<11))
#define black (0x0000)
#define gray (15+(31<<5)+(51<<11))
#define white (0xffff)
int colors[] = {red, dark_red, green, dark_green, blue, dark_blue, 
		yellow, cyan, magenta, gray, black, white};


// 8-bit color
#define rgb(r,g,b) ((((r)&7)<<5) | (((g)&7)<<2) | (((b)&3)))

// pixel macro
#define VGA_PIXEL(x,y,color) do{\
	char  *pixel_ptr ;\
	pixel_ptr = (char *)vga_pixel_ptr + ((y)<<10) + (x) ;\
	*(char *)pixel_ptr = (color);\
} while(0)


// ============== SOLVER-LW-AXI POINTER ================
volatile          char *  fpga_reset_ptr      		= NULL ;
volatile          char *  fpga_start_ptr      		= NULL ;
volatile          char *  fpga_stop_ptr       		= NULL ;
volatile          char *  fpga_data_valid_ptr 		= NULL ;
volatile          signed int *   fpga_current_ptr   = NULL ;
// volatile          signed int *   fpga_energy_ptr    = NULL ;
volatile          int *   fpga_cycle_cnt_ptr  		= NULL ;
volatile          char *  fpga_calibration_ptr      = NULL ;
volatile          unsigned int *  energy_out_lower_ptr 		= NULL ;
volatile          signed int * energy_out_upper_ptr = NULL ;
//======================================================

// the light weight buss base
void *h2p_lw_virtual_base;

// pixel buffer
volatile unsigned int * vga_pixel_ptr = NULL ;
void *vga_pixel_virtual_base;

// character buffer
volatile unsigned int * vga_char_ptr = NULL ;
void *vga_char_virtual_base;

// /dev/mem file id
int fd;

// measure time
struct timeval t1, t2;
double elapsedTime;
/****************************************************************************************
// 							Energy estimator variables
****************************************************************************************/
double time_spent;
float current_float;
double energy_double;
float power_float;
float cycle_cnt;

long long energy_64bit;

// Coordinate variables for plotting graphs
int coord_x_pre;
int coord_x;
signed int current_y_pre;
signed int current_y;
signed int power_y_pre;
signed int power_y;
signed int avg_power_y_pre;
signed int avg_power_y;
double time_start;
double time_pre;

// Time axis variable
int time_axis1;
int time_axis2;
int time_axis3;

// Calibrate indicator
int cali_flag;



/****************************************************************************************
 *                              Thread stuff
****************************************************************************************/

#define TRUE 1
#define FALSE 0



// access to enter condition
// -- for signalling enter done
pthread_mutex_t enter_lock= PTHREAD_MUTEX_INITIALIZER;
// access to print condition
// -- for signalling print done
pthread_mutex_t print_lock= PTHREAD_MUTEX_INITIALIZER;
// counter protection
pthread_mutex_t count_lock= PTHREAD_MUTEX_INITIALIZER;

// the two condition variables related to the mutex above
pthread_cond_t enter_cond ;
pthread_cond_t print_cond ;
pthread_cond_t hardware_cond ;

// globals for perfromance
int count1, count2;

// Thread variables
char input_buffer[64];
// int threshold;

//Thread functions
void * hardware();
void * software();
void * read1();
void * write1();
void * counter1();



	
/****************************************************************************************
 *                              Main Function
****************************************************************************************/
int main()
{

	// === need to mmap: =======================
	// FPGA_CHAR_BASE
	// FPGA_ONCHIP_BASE      
	// HW_REGS_BASE        
  
	// === get FPGA addresses ==================
    // Open /dev/mem
	if( ( fd = open( "/dev/mem", ( O_RDWR | O_SYNC ) ) ) == -1 ) 	{
		printf( "ERROR: could not open \"/dev/mem\"...\n" );
		return( 1 );
	}
    
    // get virtual addr that maps to physical
	h2p_lw_virtual_base = mmap( NULL, HW_REGS_SPAN, ( PROT_READ | PROT_WRITE ), MAP_SHARED, fd, HW_REGS_BASE );	
	if( h2p_lw_virtual_base == MAP_FAILED ) {
		printf( "ERROR: mmap1() failed...\n" );
		close( fd );
		return(1);
	}
    

	// === get VGA char addr =====================
	// get virtual addr that maps to physical
	vga_char_virtual_base = mmap( NULL, FPGA_CHAR_SPAN, ( 	PROT_READ | PROT_WRITE ), MAP_SHARED, fd, FPGA_CHAR_BASE );	
	if( vga_char_virtual_base == MAP_FAILED ) {
		printf( "ERROR: mmap2() failed...\n" );
		close( fd );
		return(1);
	}
    
    // Get the address that maps to the FPGA LED control 
	vga_char_ptr =(unsigned int *)(vga_char_virtual_base);

	// === get VGA pixel addr ====================
	// get virtual addr that maps to physical
	vga_pixel_virtual_base = mmap( NULL, FPGA_ONCHIP_SPAN, ( 	PROT_READ | PROT_WRITE ), MAP_SHARED, fd, 			FPGA_ONCHIP_BASE);	
	if( vga_pixel_virtual_base == MAP_FAILED ) {
		printf( "ERROR: mmap3() failed...\n" );
		close( fd );
		return(1);
	}
    
    // Get the address that maps to the FPGA pixel buffer
	vga_pixel_ptr =(unsigned int *)(vga_pixel_virtual_base);


/****************************************************************************************
 *                              Draw things on VGA
****************************************************************************************/

	/* create a message to be displayed on the VGA 
          and LCD displays */
	// char text_top_row[40] = "DE1-SoC ARM/FPGA\0";
	// char text_bottom_row[40] = "Cornell ece5760\0";
	// char text_next[40] = "Graphics primitives\0";
	// char num_string[20], time_string[20] ;
	// char color_index = 0 ;
	// int color_counter = 0 ;

	char time_text[30] = "Time/s\0";
	char curent_text[30] = "Current/A\0";
	char dyn_power_text[30] = "Dynamic power/W\0";
	char avg_power_text[30] = "Average power/W\0";
	char curren_y_text1[10] = "0.5";
	char curren_y_text2[10] = "1.0";
	char curren_y_text3[10] = "1.5";
	char curren_y_text4[10] = "2.0";
	char dyn_power_y_text1[10] = " 6";
	char dyn_power_y_text2[10] = "12";
	char dyn_power_y_text3[10] = "18";
	char dyn_power_y_text4[10] = "24";
	char avg_power_y_text1[10] = " 6";
	char avg_power_y_text2[10] = "12";
	char avg_power_y_text3[10] = "18";
	char avg_power_y_text4[10] = "24";
	// char time_axis_text1[10];
	// char time_axis_text2[10];
	// char time_axis_text3[10];

	// clear the screen
	VGA_box (0, 0, 639, 479, 0x0000);
	// clear the text
	VGA_text_clear();


	// ====================== Plot graphs ==============================
	// =============== Current ================
	VGA_line(40,20,40,139,dark_blue);
	VGA_line(40,139,420,139,dark_blue);
	VGA_text(4,1,curent_text);
	VGA_text(45,18,time_text);
	// Y axis data points
	// 0.5A
	VGA_line(40,110,37,110,dark_blue);
	VGA_text(1,13,curren_y_text1);
	// 1.0A
	VGA_line(40,81,37,81,dark_blue);
	VGA_text(1,10,curren_y_text2);
	// 1.5A
	VGA_line(40,52,37,52,dark_blue);
	VGA_text(1,6,curren_y_text3);
	// 2.0A
	VGA_line(40,23,37,23,dark_blue);
	VGA_text(1,3,curren_y_text4);

	// ============== Dynamic ower ==============
	VGA_line(40,169,40,288,yellow);
	VGA_line(40,288,420,288,yellow);
	VGA_text(4,19,dyn_power_text);
	VGA_text(45,37,time_text);	
	// Y axis data points
	// 6W
	VGA_line(40,259,37,259,yellow);
	VGA_text(2,33,dyn_power_y_text1);
	// 12kW
	VGA_line(40,230,37,230,yellow);
	VGA_text(2,29,dyn_power_y_text2);
	// 18W
	VGA_line(40,201,37,201,yellow);
	VGA_text(2,25,dyn_power_y_text3);
	// 24W
	VGA_line(40,172,37,172,yellow);
	VGA_text(2,21,dyn_power_y_text4);

	// ================ Average power ==============
	VGA_line(40,329,40,448, magenta);
	VGA_line(40,448,420,448,magenta);
	VGA_text(4,39,avg_power_text);
	// VGA_text(45,57,time_text);	
	// Y axis data points
	// 6W
	VGA_line(40,419,37,419,magenta);
	VGA_text(2,52,avg_power_y_text1);
	// 12kW
	VGA_line(40,390,37,390,magenta);
	VGA_text(2,49,avg_power_y_text2);
	// 18W
	VGA_line(40,361,37,361,magenta);
	VGA_text(2,45,avg_power_y_text3);
	// 24W
	VGA_line(40,332,37,332,magenta);
	VGA_text(2,41,avg_power_y_text4);

	// Time axis
	VGA_line(166,448,166,451,magenta);
	// VGA_text(20,57,time_axis_text1);

	VGA_line(292,448,292,451,magenta);
	// VGA_text(36,57,time_axis_text2);

	VGA_line(418,448,418,451,magenta);
	// VGA_text(51,57,time_axis_text3);

	// =================== Box to print data ===========================
	VGA_line(440,30,630,30,white);
	VGA_line(440,450,630,450,white);
	VGA_line(440,30,440,450,white);
	VGA_line(630,30,630,450,white);

	// print data string
	// char current_string[20] = "Current:";
	// char current_data_string[20];
	// char dyn_power_string[20] = "Dynamic power:";
	// char dyn_power_data_string[20];
	// char avg_power_string[20] = "Average power:";
	// char avg_power_data_string[20];
	// char energy_string[20] = "Energy:";
	// char energy_data_string[20];
	// char time_string[20] = "Time:";
	// char time_data_string[20];
	// char current_unit_string[5] = "A";
	// char power_unit_string[5] = "W";
	// char energy_unit_string[5] = "J";
	// char time_unit_string[5] = "s";
	// char title_string[30] = "==== Data table ====";
	// char assume_vol1[20] = "Assuming constant";
	// char assume_vol2[20] = "voltage at 12V";
	// char choose_operation[30] = " Choose from operations:";
	// char operation0[30] = "0 -- reset";
	// char operation1[30] = "1 -- start";
	// char operation2[30] = "2 -- stop";
	// char operation3[30] = "3 -- calibrate";
	
// ==================================================================================


	// =================== LW AXI connection ================================
	fpga_reset_ptr = (char *)(h2p_lw_virtual_base + FPGA_RESET);    
    fpga_start_ptr = (char *)(h2p_lw_virtual_base + FPGA_START);        
	fpga_stop_ptr  = (char *)(h2p_lw_virtual_base + FPGA_STOP);         
	fpga_data_valid_ptr = (char *)(h2p_lw_virtual_base + FPGA_DATA_VALID);    
	fpga_current_ptr = (signed int *)(h2p_lw_virtual_base + FPGA_CURRENT);       
	// fpga_energy_ptr  = (signed int *)(h2p_lw_virtual_base + FPGA_ENERGY);      
	fpga_cycle_cnt_ptr = (int *)(h2p_lw_virtual_base + FPGA_CYCLE_CNT);    
	fpga_calibration_ptr = (char *)(h2p_lw_virtual_base + FPGA_CALIBRATE);
	energy_out_lower_ptr = (unsigned int *)(h2p_lw_virtual_base + ENERGY_OUT_LOWER);
	energy_out_upper_ptr = (signed int *)(h2p_lw_virtual_base + ENERGY_OUT_UPPER);
    // ======================================================================
	printf("start setting power estimator\n");
	*(fpga_stop_ptr) = 0;
	*(fpga_stop_ptr) = 1;
	usleep(1000);
	*(fpga_stop_ptr) = 0;
	printf("set stop to 1\n");
	usleep(100);
	*(fpga_reset_ptr) = 0;
    *(fpga_reset_ptr) = 1;
	usleep(1000);
	*(fpga_reset_ptr) = 0;
	printf("set reset to 1\n");
	usleep(100);
	*(fpga_start_ptr) = 0;
	*(fpga_start_ptr) = 1;
	usleep(1000);
	*(fpga_start_ptr) = 0;
	printf("set start to 1\n");


	// ================== Thread ============================================
	int status;
	// the thread identifiers
	pthread_t thread_read, thread_write, thread_count1, thread_count2, thread_hardware;
	// pthread_t software_thread;
	
	// the condition variables
	pthread_cond_init (&enter_cond, NULL);
	pthread_cond_init (&print_cond, NULL);
	pthread_cond_init (&hardware_cond, NULL);
	
	//For portability, explicitly create threads in a joinable state 
	// thread attribute used here to allow JOIN
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	
	// now the threads
	pthread_create(&thread_read,NULL,read1,NULL);
	pthread_create(&thread_write,NULL,write1,NULL);
	pthread_create(&thread_count1,NULL,counter1,NULL);
	pthread_create(&thread_hardware, NULL, hardware(),NULL);
	pthread_join(thread_read,NULL);
	pthread_join(thread_write,NULL);

	// Initialize calibration flag
	cali_flag = 0;

// ============================================================================================

	// // Initialize variable
	// time_start = 0;
	// time_axis1 = 1;
	// time_axis2 = 2;
	// time_axis3 = 3;

	// // Dot coordinate
	// coord_x_pre = 41;
	// current_y_pre = 139;
	// power_y_pre = 288;
	// avg_power_y_pre = 448;

	// time_pre = 0;

    // while(1){
	// 	// start timer
	// 	gettimeofday(&t1, NULL);
		

    //     if(*(fpga_data_valid_ptr) == 1){
	// 		current_float = (float)(*(fpga_current_ptr)/pow(2,23));
	// 		// energy_float = (float)((*(fpga_energy_ptr)/pow(2,23)));
	// 		energy_64bit = (((long long) *(energy_out_upper_ptr))<<32) | *(energy_out_lower_ptr);
	// 		energy_double = energy_64bit/pow(2,23);
	// 		cycle_cnt = *(fpga_cycle_cnt_ptr);
	// 		time_spent = ((cycle_cnt-1)*18)/200000;
	// 		// usleep(100);
    // 		printf("Current measured is:%fA\n", current_float);
	// 		printf("Energy measured is:%lfJ\n", energy_double);
	// 		printf("Power is:%f W\n", current_float*12);
	// 		printf("Cycle count is:%f\n", cycle_cnt);
	// 		printf("Time spent is: %lfs\n\n",time_spent);
	// 		usleep(1000);

	// 		coord_x = 41 + (time_spent - time_start)/0.00789; 
	// 		// if(coord_x >= 420){
	// 		// 	VGA_box (41, 15, 425, 138, 0x000);
	// 		// 	VGA_box (41, 164, 425, 287, 0x000);
	// 		// 	VGA_box (41, 324, 425, 447, 0x000);
	// 		// 	time_start = time_spent;
	// 		// 	time_axis1 = time_axis3 + 1;
	// 		// 	time_axis2 = time_axis3 + 2;
	// 		// 	time_axis3 = time_axis3 + 3;
	// 		// }

	// 		current_y = 139 - current_float/0.0167;
	// 		power_y = 288 - (current_float*12)/0.2017;
	// 		avg_power_y = 448 - (energy_double/time_spent)/0.2017;

	// 		if(current_y > 139 ){
	// 			current_y = 139;
	// 		}
	// 		if(power_y > 288){
	// 			power_y = 288;
	// 		}
	// 		if(avg_power_y > 448){
	// 			power_y = 448;
	// 		}

	// 		// VGA_PIXEL(coord_x, current_y, dark_blue);
	// 		// VGA_PIXEL(coord_x, power_y, yellow);
	// 		// VGA_PIXEL(coord_x,avg_power_y,magenta);
	// 		VGA_line(coord_x_pre, current_y_pre,coord_x, current_y,dark_blue);
	// 		VGA_line(coord_x_pre, power_y_pre, coord_x, power_y, yellow);
	// 		VGA_line(coord_x_pre, avg_power_y, coord_x, avg_power_y, magenta);

	// 		if(time_spent-time_pre>3){
	// 			// clear graphs
	// 			VGA_box (41, 15, 435, 138, 0x000);
	// 			VGA_box (41, 164, 435, 287, 0x000);
	// 			VGA_box (41, 324, 435, 447, 0x000);
	// 			time_start = time_spent;
	// 			// increment time
	// 			time_axis1 = time_axis3 + 1;
	// 			time_axis2 = time_axis3 + 2;
	// 			time_axis3 = time_axis3 + 3;
	// 			coord_x_pre = 41;
	// 			coord_x = 41;
	// 			time_pre = time_pre + 3;
	// 		}

	// 		// Print current and power data on VGA
	// 		VGA_text (57, 7, title_string);
	// 		VGA_text(57,9,assume_vol1);
	// 		VGA_text(57,10,assume_vol2);
	// 		// Current
	// 		VGA_text (57, 16, current_string);
	// 		sprintf(current_data_string, "%f", current_float);
	// 		VGA_text (57, 17, current_data_string);
	// 		VGA_text (68, 17, current_unit_string);

	// 		// Dynamic power
	// 		VGA_text (57, 19, dyn_power_string);
	// 		sprintf(dyn_power_data_string, "%f", current_float*12);
	// 		VGA_text (57, 20, dyn_power_data_string);
	// 		VGA_text (68, 20, power_unit_string);
	// 		// Average power
	// 		VGA_text (57, 22, avg_power_string);
	// 		sprintf(avg_power_data_string, "%lf", energy_double/time_spent);
	// 		VGA_text (57, 23, avg_power_data_string);
	// 		VGA_text (68, 23, power_unit_string);
	// 		// Energy
	// 		VGA_text (57, 25, energy_string);
	// 		sprintf(energy_data_string, "%lf", energy_double);
	// 		VGA_text (57, 26, energy_data_string);
	// 		VGA_text (68, 26, energy_unit_string);
	// 		// Time
	// 		VGA_text (57, 28, time_string);
	// 		sprintf(time_data_string, "%lf", time_spent);
	// 		VGA_text (57, 29, time_data_string);
	// 		VGA_text (57, 29, time_data_string);
	// 		VGA_text (68, 29, time_unit_string);

			
	// 		// Time axis
	// 		sprintf(time_axis_text1, "%d", time_axis1);
	// 		VGA_text (20, 57, time_axis_text1);
	// 		sprintf(time_axis_text2, "%d", time_axis2);
	// 		VGA_text(36,57,time_axis_text2);
	// 		sprintf(time_axis_text3, "%d", time_axis3);
	// 		VGA_text(51,57,time_axis_text3);

	// 		coord_x_pre = coord_x;
	// 		current_y_pre = current_y;
	// 		power_y_pre = power_y;
	// 		avg_power_y_pre = avg_power_y;

    //     }

// ==================================================================================================

		// coord_x = 41 + (time_spent - time_start)/0.00789; 
		// if(coord_x >= 420){
		// 	VGA_box (41, 15, 421, 138, 0x000);
		// 	VGA_box (41, 164, 421, 287, 0x000);
		// 	VGA_box (41, 324, 421, 447, 0x000);
		// 	time_start = time_spent;
		// }

		// current_y = 139 - current_float/0.0167;
		// power_y = 288 - (current_float*12)/0.2017;
		// avg_power_y = 448 - (energy_float/time_spent)*1000/0.2017;

		// if(current_y > 139 ){
		// 	current_y = 139;
		// }
		// if(power_y > 288){
		// 	power_y = 288;
		// }
		// if(avg_power_y > 448){
		// 	power_y = 448;
		// }

		// VGA_PIXEL(coord_x, current_y, dark_blue);
		// VGA_PIXEL(coord_x, power_y, yellow);
		// VGA_PIXEL(coord_x,avg_power_y,magenta);

		// Print current and power data on VGA
		// VGA_text (57, 15, current_measure);
		// sprintf(current_string, "Curent: %f A", current_float);
		// VGA_text (57, 15, current_string);

    // }
	
	// Print current and power data on VGA
	// VGA_text (57, 7, title_string);
	// VGA_text(57,9,assume_vol1);
	// VGA_text(57,10,assume_vol2);
	// // Current
	// VGA_text (57, 16, current_string);
	// sprintf(current_data_string, "%f", current_float);
	// VGA_text (57, 17, current_data_string);
	// VGA_text (68, 17, current_unit_string);
	// // Dynamic power
	// VGA_text (57, 19, dyn_power_string);
	// sprintf(dyn_power_data_string, "%f", current_float*12);
	// VGA_text (57, 20, dyn_power_data_string);
	// VGA_text (68, 20, power_unit_string);
	// // Average power
	// VGA_text (57, 22, avg_power_string);
	// sprintf(avg_power_data_string, "%lf", energy_double/time_spent);
	// VGA_text (57, 23, avg_power_data_string);
	// VGA_text (68, 23, power_unit_string);
	// // Energy
	// VGA_text (57, 25, energy_string);
	// sprintf(energy_data_string, "%lf", energy_double);
	// VGA_text (57, 26, energy_data_string);
	// VGA_text (68, 26, energy_unit_string);
	// // Time
	// VGA_text (57, 28, time_string);
	// sprintf(time_data_string, "%lf", time_spent);
	// VGA_text (57, 29, time_data_string);
	// VGA_text (57, 29, time_data_string);
	// VGA_text (68, 29, time_unit_string);
	// // Choose from operations text
	// VGA_text(57, 34, choose_operation);
	// VGA_text(57, 36, operation0);
	// VGA_text(57, 37, operation1);
	// VGA_text(57, 38, operation2);
	// VGA_text(57, 39, operation3);
	
	// // Time axis
	// sprintf(time_axis_text1, "%d", time_axis1);
	// VGA_text (20, 57, time_axis_text1);
	// sprintf(time_axis_text2, "%d", time_axis2);
	// VGA_text(36,57,time_axis_text2);
	// sprintf(time_axis_text3, "%d", time_axis3);
	// VGA_text(51,57,time_axis_text3);

} // end main

/****************************************************************************************
 *                          Thread functions
****************************************************************************************/
void * hardware(){
		// print data string
	char current_string[20] = "Current:";
	char current_data_string[20];
	char dyn_power_string[20] = "Dynamic power:";
	char dyn_power_data_string[20];
	char avg_power_string[20] = "Average power:";
	char avg_power_data_string[20];
	char energy_string[20] = "Energy:";
	char energy_data_string[20];
	char time_string[20] = "Time:";
	char time_data_string[20];
	char current_unit_string[5] = "A";
	char power_unit_string[5] = "W";
	char energy_unit_string[5] = "J";
	char time_unit_string[5] = "s";
	char title_string[30] = "==== Data table ====";
	char assume_vol1[20] = "Assuming constant";
	char assume_vol2[20] = "voltage at 12V";
	char choose_operation[30] = "Choose from below:";
	char operation1[30] = "1 -- start";
	char operation2[30] = "2 -- stop";
	char operation3[30] = "3 -- calibrate";

	char time_axis_text1[10];
	char time_axis_text2[10];
	char time_axis_text3[10];
	
	// Initialize variable
	time_start = 0;
	time_axis1 = 1;
	time_axis2 = 2;
	time_axis3 = 3;

	// Dot coordinate
	coord_x_pre = 41;
	current_y_pre = 139;
	power_y_pre = 288;
	avg_power_y_pre = 448;

	time_pre = 0;

    while(1){
		// start timer
		gettimeofday(&t1, NULL);
		

        if(*(fpga_data_valid_ptr) == 1){
			current_float = (float)(*(fpga_current_ptr)/pow(2,23));
			// energy_float = (float)((*(fpga_energy_ptr)/pow(2,23)));
			energy_64bit = (((long long) *(energy_out_upper_ptr))<<32) | *(energy_out_lower_ptr);
			energy_double = energy_64bit/pow(2,23);
			cycle_cnt = *(fpga_cycle_cnt_ptr);
			time_spent = ((cycle_cnt-1)*18)/200000;
			// usleep(100);
    		// printf("Current measured is:%fA\n", current_float);
			// printf("Energy measured is:%lfJ\n", energy_double);
			// printf("Power is:%f W\n", current_float*12);
			// printf("Cycle count is:%f\n", cycle_cnt);
			// printf("Time spent is: %lfs\n\n",time_spent);
			usleep(1000);

			coord_x = 41 + (time_spent - time_start)/0.00789; 
			// if(coord_x >= 420){
			// 	VGA_box (41, 15, 425, 138, 0x000);
			// 	VGA_box (41, 164, 425, 287, 0x000);
			// 	VGA_box (41, 324, 425, 447, 0x000);
			// 	time_start = time_spent;
			// 	time_axis1 = time_axis3 + 1;
			// 	time_axis2 = time_axis3 + 2;
			// 	time_axis3 = time_axis3 + 3;
			// }

			current_y = 139 - current_float/0.0167;
			power_y = 288 - (current_float*12)/0.2017;
			avg_power_y = 448 - (energy_double/time_spent)/0.2017;

			if(current_y > 139 ){
				current_y = 139;
			}
			if(power_y > 288){
				power_y = 288;
			}
			if(avg_power_y > 448){
				power_y = 448;
			}

			// VGA_PIXEL(coord_x, current_y, dark_blue);
			// VGA_PIXEL(coord_x, power_y, yellow);
			// VGA_PIXEL(coord_x,avg_power_y,magenta);

			if(cali_flag){
				// clear graphs
				VGA_box (41, 15, 435, 138, 0x000);
				VGA_box (41, 164, 435, 287, 0x000);
				VGA_box (41, 324, 435, 447, 0x000);
				time_axis1 = 1;
				time_axis2 = 2;
				time_axis3 = 3;
				coord_x_pre = 41;
				coord_x = 41;
				time_pre = 0;
				cali_flag = 0;
				time_start = 0;
			}

			VGA_line(coord_x_pre, current_y_pre,coord_x, current_y,dark_blue);
			VGA_line(coord_x_pre, power_y_pre, coord_x, power_y, yellow);
			VGA_line(coord_x_pre, avg_power_y, coord_x, avg_power_y, magenta);

			if(time_spent-time_pre>3){
				// clear graphs
				VGA_box (41, 15, 435, 138, 0x000);
				VGA_box (41, 164, 435, 287, 0x000);
				VGA_box (41, 324, 435, 447, 0x000);
				time_start = time_spent;
				// increment time
				time_axis1 = time_axis3 + 1;
				time_axis2 = time_axis3 + 2;
				time_axis3 = time_axis3 + 3;
				coord_x_pre = 41;
				coord_x = 41;
				time_pre = time_pre + 3;
			}

			// // Print current and power data on VGA
			VGA_text (57, 7, title_string);
			VGA_text(57,9,assume_vol1);
			VGA_text(57,10,assume_vol2);
			// Current
			VGA_text (57, 16, current_string);
			sprintf(current_data_string, "%f", current_float);
			VGA_text (57, 17, current_data_string);
			VGA_text (68, 17, current_unit_string);

			// Dynamic power
			VGA_text (57, 19, dyn_power_string);
			sprintf(dyn_power_data_string, "%f", current_float*12);
			VGA_text (57, 20, dyn_power_data_string);
			VGA_text (68, 20, power_unit_string);
			// Average power
			VGA_text (57, 22, avg_power_string);
			sprintf(avg_power_data_string, "%lf", energy_double/time_spent);
			VGA_text (57, 23, avg_power_data_string);
			VGA_text (68, 23, power_unit_string);
			// Energy
			VGA_text (57, 25, energy_string);
			sprintf(energy_data_string, "%lf", energy_double);
			VGA_text (57, 26, energy_data_string);
			VGA_text (68, 26, energy_unit_string);
			// Time
			VGA_text (57, 28, time_string);
			sprintf(time_data_string, "%lf", time_spent);
			VGA_text (57, 29, time_data_string);
			VGA_text (57, 29, time_data_string);
			VGA_text (68, 29, time_unit_string);

			// Choose from operations text
			VGA_text(57, 34, choose_operation);
			VGA_text(57, 38, operation1);
			VGA_text(57, 40, operation2);
			VGA_text(57, 42, operation3);
			
			// Time axis
			sprintf(time_axis_text1, "%d   ", time_axis1);
			VGA_text (20, 57, time_axis_text1);
			sprintf(time_axis_text2, "%d   ", time_axis2);
			VGA_text(36,57,time_axis_text2);
			sprintf(time_axis_text3, "%d   ", time_axis3);
			VGA_text(51,57,time_axis_text3);

			coord_x_pre = coord_x;
			current_y_pre = current_y;
			power_y_pre = power_y;
			avg_power_y_pre = avg_power_y;

        }
	}
}

void * read1()
{
		printf("\n");
		printf("Power estimator user interface\n");
		printf("**************************************************************\n");
		while(1){
                //wait for print done
				pthread_mutex_lock(&print_lock);
				pthread_cond_wait(&print_cond,&print_lock);
				// the actual enter				
                printf("Plese choose an operation:\n");
				printf("1-start   2-stop   3-calibrate\n");
                scanf("%s",input_buffer);
				// unlock the input_buffer
                pthread_mutex_unlock(&print_lock);
			    // and tell write1 thread that enter is complete
                pthread_cond_signal(&enter_cond);
        } // while(1)
}

void * write1() {
		sleep(1);
		// signal that the print process is ready when started
		pthread_cond_signal(&print_cond);
		
        while(1){
				// wait for enter done
                pthread_mutex_lock(&enter_lock);
                pthread_cond_wait(&enter_cond,&enter_lock);
				// the protected print (with protected counter)
				pthread_mutex_lock(&count_lock);
				gettimeofday(&t1, NULL);

				if(strcmp(input_buffer,"1")==0){
					*(fpga_start_ptr) = 0;
    				*(fpga_start_ptr) = 1;
					usleep(1000);
					*(fpga_start_ptr) = 0;
				}
				else if(strcmp(input_buffer,"2")==0){
					*(fpga_stop_ptr) = 0;
    				*(fpga_stop_ptr) = 1;
					usleep(1000);
					*(fpga_stop_ptr) = 0;
				}
				else if(strcmp(input_buffer,"3")==0){
					*(fpga_calibration_ptr) = 0;
    				*(fpga_calibration_ptr) = 1;
					usleep(1000);
					*(fpga_calibration_ptr) = 0;
					cali_flag = 1;
				}
				count1 = 0;
				pthread_mutex_unlock(&count_lock);
				// unlock the input_buffer
				pthread_mutex_unlock(&enter_lock);
                // and tell read1 thread that print is done
				pthread_cond_signal(&print_cond);         
        } // while(1)
}

void * counter1() {
		//
        while(1){
				// count as fast as possible
				pthread_mutex_lock(&count_lock);
                count1++;    
				pthread_mutex_unlock(&count_lock);
				              
        } // while(1)
}

/****************************************************************************************
 * Subroutine to send a string of text to the VGA monitor 
****************************************************************************************/
void VGA_text(int x, int y, char * text_ptr)
{
  	volatile char * character_buffer = (char *) vga_char_ptr ;	// VGA character buffer
	int offset;
	/* assume that the text string fits on one line */
	offset = (y << 7) + x;
	while ( *(text_ptr) )
	{
		// write to the character buffer
		*(character_buffer + offset) = *(text_ptr);	
		++text_ptr;
		++offset;
		// Added to slow print on vga
		usleep(250);
	}
}

/****************************************************************************************
 * Subroutine to clear text to the VGA monitor 
****************************************************************************************/
void VGA_text_clear()
{
  	volatile char * character_buffer = (char *) vga_char_ptr ;	// VGA character buffer
	int offset, x, y;
	for (x=0; x<79; x++){
		for (y=0; y<59; y++){
	/* assume that the text string fits on one line */
			offset = (y << 7) + x;
			// write to the character buffer
			*(character_buffer + offset) = ' ';		
		}
	}
}

/****************************************************************************************
 * Draw a filled rectangle on the VGA monitor 
****************************************************************************************/
#define SWAP(X,Y) do{int temp=X; X=Y; Y=temp;}while(0) 

void VGA_box(int x1, int y1, int x2, int y2, short pixel_color)
{
	char  *pixel_ptr ; 
	int row, col;

	/* check and fix box coordinates to be valid */
	if (x1>639) x1 = 639;
	if (y1>479) y1 = 479;
	if (x2>639) x2 = 639;
	if (y2>479) y2 = 479;
	if (x1<0) x1 = 0;
	if (y1<0) y1 = 0;
	if (x2<0) x2 = 0;
	if (y2<0) y2 = 0;
	if (x1>x2) SWAP(x1,x2);
	if (y1>y2) SWAP(y1,y2);
	for (row = y1; row <= y2; row++)
		for (col = x1; col <= x2; ++col)
		{
			//640x480
			pixel_ptr = (char *)vga_pixel_ptr + (row<<10)    + col ;
			// set pixel color
			*(char *)pixel_ptr = pixel_color;		
		}
}

/****************************************************************************************
 * Draw a filled circle on the VGA monitor 
****************************************************************************************/

void VGA_disc(int x, int y, int r, short pixel_color)
{
	char  *pixel_ptr ; 
	int row, col, rsqr, xc, yc;
	
	rsqr = r*r;
	
	for (yc = -r; yc <= r; yc++)
		for (xc = -r; xc <= r; xc++)
		{
			col = xc;
			row = yc;
			// add the r to make the edge smoother
			if(col*col+row*row <= rsqr+r){
				col += x; // add the center point
				row += y; // add the center point
				//check for valid 640x480
				if (col>639) col = 639;
				if (row>479) row = 479;
				if (col<0) col = 0;
				if (row<0) row = 0;
				pixel_ptr = (char *)vga_pixel_ptr + (row<<10) + col ;
				// set pixel color
				*(char *)pixel_ptr = pixel_color;
			}
					
		}
}

// =============================================
// === Draw a line
// =============================================
//plot a line 
//at x1,y1 to x2,y2 with color 
//Code is from David Rodgers,
//"Procedural Elements of Computer Graphics",1985
void VGA_line(int x1, int y1, int x2, int y2, short c) {
	int e;
	signed int dx,dy,j, temp;
	signed int s1,s2, xchange;
     signed int x,y;
	char *pixel_ptr ;
	
	/* check and fix line coordinates to be valid */
	if (x1>639) x1 = 639;
	if (y1>479) y1 = 479;
	if (x2>639) x2 = 639;
	if (y2>479) y2 = 479;
	if (x1<0) x1 = 0;
	if (y1<0) y1 = 0;
	if (x2<0) x2 = 0;
	if (y2<0) y2 = 0;
        
	x = x1;
	y = y1;
	
	//take absolute value
	if (x2 < x1) {
		dx = x1 - x2;
		s1 = -1;
	}

	else if (x2 == x1) {
		dx = 0;
		s1 = 0;
	}

	else {
		dx = x2 - x1;
		s1 = 1;
	}

	if (y2 < y1) {
		dy = y1 - y2;
		s2 = -1;
	}

	else if (y2 == y1) {
		dy = 0;
		s2 = 0;
	}

	else {
		dy = y2 - y1;
		s2 = 1;
	}

	xchange = 0;   

	if (dy>dx) {
		temp = dx;
		dx = dy;
		dy = temp;
		xchange = 1;
	} 

	e = ((int)dy<<1) - dx;  
	 
	for (j=0; j<=dx; j++) {
		//video_pt(x,y,c); //640x480
		pixel_ptr = (char *)vga_pixel_ptr + (y<<10)+ x; 
		// set pixel color
		*(char *)pixel_ptr = c;	
		 
		if (e>=0) {
			if (xchange==1) x = x + s1;
			else y = y + s2;
			e = e - ((int)dx<<1);
		}

		if (xchange==1) y = y + s2;
		else x = x + s1;

		e = e + ((int)dy<<1);
	}
}