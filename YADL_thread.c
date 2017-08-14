#define OWN_P_TAG 1
#define P_TAG 2
#define FREEDYA_TAG 3
#define NUMBER_TAG 4
#define T_TAG 5
#define C_TAG 6

#define GLOBAL_COUNT_TAG 11

#define READ_P_TAG 12
#define READ_FREEDYA_TAG 13
#define READ_NUMBER_TAG 14
#define READ_T_TAG 15
#define READ_C_TAG 16

#define SENDER_TAG 7
#define RECEIVER_TAG 8

#define TRUE 1
#define FALSE 0
#define VERBOSE 0

#define TOTAL_PROCESSES 8	/*12.4.11: has to match during MPI run instantiation at CLI*/

#define REGENERATE_INTERVAL (TOTAL_PROCESSES) /*26.5: equate REGENERATE_INTERVAL to TOTAL_PROCESSES*/
#define REGENERATE_TIME 3

#define CS_INTERVAL 1.0 	/*29.3: stands for 1ms*/
#define CS_TIME 3		/*6.4: runtime in C-S or time between regeneration is 1x, 2x, or RUNTIME_LIMITx*/

#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <math.h>
#include <mpi.h>
#include <unistd.h>
#include <time.h>
#include <sys/timeb.h>

/*D-S for keeping time information*/

extern float system_time;
extern float creation_time;

extern long int baseline_time;
extern int time_horizon;

extern struct timeb baseline_precise_time;

extern int remote_accesses;

extern int MCS_queue_number;
extern int MCS_already_linkedin;
extern int MCS_times_count;

extern float MCS_avg_queue_length;
extern int MCS_locks_scanned;

extern float spartime;
extern float max_spartime;

extern MPI_Status status_thread;

extern int C[2];
extern int T;
extern int P;
extern int Freed_YA;
extern int Number;
extern int total_processes;
extern int process_count;
extern int process_index;
extern int GLOBAL_COUNT;

struct flagdata { float start_time; float end_time; };
extern struct flagdata flag;

int lock_number(int process_index, int level)
	{
	int current_level_lock, locks, levels;

	current_level_lock=floor( process_index/ ((int) pow(2, level+1)));
	
	locks=0;

	for (levels=0; levels<level; levels++)
		locks+=floor(TOTAL_PROCESSES/((int) pow(2, levels+1)));

	if(!level) locks=current_level_lock; else locks+=current_level_lock;

	return locks;
	}

int lock_sequence(int level, int order)
	{
	int levels, locks;

	locks=0;

	for (levels=0; levels<level; levels++)
		locks+=(TOTAL_PROCESSES/pow(2, levels+1));

	locks+=order;
	return locks;
	}

int side(int process_index, int level)
	{
	/*merely probing the relevant bit in process_index should do the job*/
	int temporary, j;
	temporary = process_index & ((int)(pow(2, level)));

	temporary = (temporary)?1:0;

	return( temporary );
	}

void YA_CS(int process_index, int level, int running_time)
	{

	int rival, j, J, k, l, ierr, message, message1, message2, T_process_index, P_rival, NTjk, Number_Temp;
	long int scrap_time;
	long int GFLOPS;
	double gflops, gflops_j;

	int MCS_queue_length, irecv_count, junk;
	int global_count;

	int head_lowcost_MCS, tail_lowcost_MCS, Tjk, Cjk[2], Cjk0, Cjk1, P_Freed_YA_request_index;

	struct timeb scrap_precise_time;
	MPI_Request P_Freed_YA_request[3], Number_request;

	int scan_map[((int)(log10(TOTAL_PROCESSES)/log10(2.0)))][TOTAL_PROCESSES/2];

	scan_map[((int)(log10(TOTAL_PROCESSES)/log10(2.0)))-1][0]=1;

	MCS_queue_length=0;

	message=2*(process_index+1) + side(process_index, level) -1;
	ierr=MPI_Send( &message, 1, MPI_INT, lock_number(process_index, level), C_TAG, MPI_COMM_WORLD);
	remote_accesses++;

	if (VERBOSE)
	{{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
	printf("(%d, %d): C[%d][%d]:=1", process_index, level, lock_number(process_index, level), side(process_index, level));
	fflush(stdout);}

	message=1000+process_index;
	ierr=MPI_Send( &message, 1, MPI_INT, lock_number(process_index, level), T_TAG, MPI_COMM_WORLD);
	remote_accesses++;

	if (VERBOSE)
	{{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
	printf("(%d, %d): T[%d]:=%d", process_index, level, lock_number(process_index, level), process_index);
	fflush(stdout);}

	/*Write own `P' as 3*level+0*/
	P=3*level+0;

	if (VERBOSE)
	{{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
	printf("(%d, %d): P[%d]:=%d", process_index, level, process_index, P);
	fflush(stdout);}

	message=-2*(process_index+1) - (1-side(process_index, level)) +1;
	ierr=MPI_Send( &message, 1, MPI_INT, lock_number(process_index, level), C_TAG, MPI_COMM_WORLD);
	MPI_Recv( &rival, 1, MPI_INT,  lock_number(process_index, level), READ_C_TAG, MPI_COMM_WORLD, &status_thread);
	remote_accesses+=2;

	if (VERBOSE)
	{{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
	printf("(%d, %d): Rival P%d", process_index, level, rival);
	fflush(stdout);}
	
	if (rival!=-1)
		{
		/*12.4.11: Request-Response to read 'T' from lock-holder thread*/

			message=-1000-process_index;
			ierr=MPI_Send( &message, 1, MPI_INT, lock_number(process_index, level), T_TAG, MPI_COMM_WORLD);
			MPI_Recv(&T_process_index, 1, MPI_INT, lock_number(process_index, level), READ_T_TAG, MPI_COMM_WORLD, &status_thread);
			remote_accesses+=2;

			if (VERBOSE)
			{{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
			printf("(%d, %d): T[%d]==%d", process_index, level, lock_number(process_index, level), T_process_index);
			fflush(stdout);}

			if ( T_process_index  == process_index )
			{

				/*12.4.11: Request-Response pair to read rival's 'P'*/
				message=-1500-process_index;
				ierr=MPI_Send( &message, 1, MPI_INT, rival, P_TAG, MPI_COMM_WORLD);
				MPI_Recv(&P_rival, 1, MPI_INT, rival, READ_P_TAG, MPI_COMM_WORLD, &status_thread);
				remote_accesses+=2;

				if (VERBOSE)
				{{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
				printf("(%d, %d): P[%d]==%d", process_index, level, rival, P_rival);
				fflush(stdout);}

				/*12.4.11: write rival's 'P' as 1 (or 1 equivalent, note explanation in YAL_thread.c for 3*level+0 coding*/
				if (P_rival==3*level+0)
					{

					message=1500+(3*level+1);
					ierr=MPI_Send( &message, 1, MPI_INT, rival, P_TAG, MPI_COMM_WORLD);
					remote_accesses++;

					if (VERBOSE)
					{{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
					printf("(%d, %d): P[%d]:=%d", process_index, level, rival, message-1500);
					fflush(stdout);}

					}

				if(VERBOSE) {{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
				printf("(%d, %d): SPIN(P==0 || F) for %d", process_index, level, rival);
				fflush(stdout);}

				/*16.5.11: waiting for P or Freed_YA to change because process has been queued up*/
				while( P==3*level+0 && !Freed_YA ) ;

				if (!Freed_YA)	/*5.5.11: if P has been set by rival process and not Freed_YA, so usual routine kicks in*/

				{

				message=-1000-process_index;
				ierr=MPI_Send( &message, 1, MPI_INT, lock_number(process_index, level), T_TAG, MPI_COMM_WORLD);
				MPI_Recv(&T_process_index, 1, MPI_INT, lock_number(process_index, level), READ_T_TAG, MPI_COMM_WORLD, &status_thread);
				remote_accesses+=2;

				if (VERBOSE)
				{{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
				printf("(%d, %d): T[%d]==%d", process_index, level, lock_number(process_index, level), T_process_index);
				fflush(stdout);}

				if (T_process_index==process_index)
					{

					if(VERBOSE) {{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
					printf("(%d, %d): SPIN(P<=1 || F) for %d", process_index, level, rival);
					fflush(stdout);}
 					
					while ( P<=3*level+1 && !Freed_YA) ;
					}
				}
			} /*end of if(T_process_index == process_index)*/
		} /*end of if(rival!=-1)*/

	if ( !Freed_YA ){ /*regular method of contending in tree, like in YAL_thread.c*/

	if ( level!=(log10(TOTAL_PROCESSES)/log10(2.0))-1)
		{

		/*26.5.11: unnecessary assignment of -(process_index+1) due to mysterious bug*/
		/*Number=-(process_index+1);*/ /*suitable candidate for removal*/
		YA_CS(process_index, level+1, running_time);
		}
	else 
		{ /*process with ID process_index has now acquired the apex-lock, so it will build MCS queue*/
		tail_lowcost_MCS=process_index;
		MCS_queue_length=0;
		head_lowcost_MCS=process_index;

		MCS_times_count++; /*increment count of MCS queues the simulation has built thus far (is this the same as MCS_queue_number ?)*/
		J=log10(TOTAL_PROCESSES)/log10(2.0)-1; /*J is depth of the arbitration tree*/

		scan_map[J][0]=1; /*the apex lock certainly has one occupant (maybe two)*/

		/*the array scan_map is initialized with zero entries*/
		for (j=J-1;j>=0;j--)
			for (k=0;k<pow(2, ((int)(log10(TOTAL_PROCESSES)/log10(2.0)-1-j)));k++)
				scan_map[j][k]=0;

		for (j=J;j>=0;j--)
			{

			for (k=0;k<pow(2, ((int)(log10(TOTAL_PROCESSES)/log10(2.0)-1-j)));k++)
				{

				/*12.4.11: Request-Response pair to read both 'C' entries from lock-holder thread*/

				if( ! scan_map[j][k] ) continue; else MCS_locks_scanned++; /*increment count of locks scanned for occupants*/

				message=-2*(process_index+1)-0+1;
				ierr=MPI_Send( &message, 1, MPI_INT, lock_sequence(j, k), C_TAG, MPI_COMM_WORLD);
				MPI_Recv( &Cjk0, 1, MPI_INT, lock_sequence(j, k), READ_C_TAG, MPI_COMM_WORLD, &status_thread);
				remote_accesses+=2;

				if (VERBOSE)
				{{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
				printf("(%d, %d): C[%d][0]==%d", process_index, level, lock_sequence(j, k), Cjk0);
				fflush(stdout);}
				
				message=-2*(process_index+1)-1+1;
				ierr=MPI_Send( &message, 1, MPI_INT, lock_sequence(j, k), C_TAG, MPI_COMM_WORLD);
				MPI_Recv( &Cjk1, 1, MPI_INT, lock_sequence(j, k), READ_C_TAG, MPI_COMM_WORLD, &status_thread);
				remote_accesses+=2;

				if (VERBOSE)
				{{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
				printf("(%d, %d): C[%d][1]==%d", process_index, level, lock_sequence(j, k), Cjk1);
				fflush(stdout);}

				/*algorithm's indefinite spinning maybe on this account !*/
				if(Cjk0!=-1 && Cjk1!=-1)   /*link-in only (the 'stuck' process) from a lock that has contention*/
				{				

					if (VERBOSE)
					{{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
					printf("(%d, %d): Qualified L%d", process_index, level, lock_sequence(j, k));
					fflush(stdout);}

					message=-1000-process_index;
					ierr=MPI_Send( &message, 1, MPI_INT, lock_sequence(j, k), T_TAG, MPI_COMM_WORLD);
					MPI_Recv( &Tjk, 1, MPI_INT, lock_sequence(j, k), READ_T_TAG, MPI_COMM_WORLD, &status_thread);
					remote_accesses+=2;
					
					if (VERBOSE)
					{{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
					printf("(%d, %d): T[%d]==%d", process_index, level, lock_sequence(j, k), Tjk);
					fflush(stdout);}
					
					/*12.4.11: Request-Response pair to read 'Number' of process Tjk into NTjk*/

					if (VERBOSE)
					{{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
					printf("(%d, %d): Checking P%d", process_index, level, Tjk);
					fflush(stdout);}

					message=-3000-(process_index+1);
					ierr=MPI_Send( &message, 1, MPI_INT, Tjk, NUMBER_TAG, MPI_COMM_WORLD);
					MPI_Recv(&NTjk, 1, MPI_INT, Tjk, READ_NUMBER_TAG, MPI_COMM_WORLD, &status_thread);
					remote_accesses+=2;

					if (VERBOSE)
					{{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
					printf("(%d, %d): N[%d]==%d", process_index, level, Tjk, NTjk);
					fflush(stdout);}

					if (NTjk != -(Tjk+1) || tail_lowcost_MCS == Tjk || process_index == Tjk)
						MCS_already_linkedin++; /*4.8.17: Don't get what's 'already linked in' !*/
					else	{
						/*12.4.11: Request write 'Number' field of process tail_lowcost_MCS as -(Tjk+1)*/
						/*4.8.17: It appears that N field is doing the job of creating MCS linked-list*/
						if (MCS_queue_length)
							{ 
							message=3000+(Tjk+1);
							ierr=MPI_Send( &message, 1, MPI_INT, tail_lowcost_MCS, NUMBER_TAG, MPI_COMM_WORLD);
							remote_accesses++;

							if (VERBOSE)
							{{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
							printf("(%d, %d): N[%d]:=%d", process_index, level, tail_lowcost_MCS, -(Tjk+1));
							fflush(stdout);}
							}

						MCS_queue_length++;
						if (MCS_queue_length==1) head_lowcost_MCS=Tjk; /*but is tail_lowcost_MCS pointing to Tjk ?*/

						tail_lowcost_MCS=Tjk;
	
						/*12.4.11: Request to write 'Freed_YA' of process Tjk to (process_index+1)*/
						message=4000+(process_index+1);
						ierr=MPI_Send(&message, 1, MPI_INT, Tjk, FREEDYA_TAG, MPI_COMM_WORLD);
						remote_accesses++;

						if (VERBOSE)
						{{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
						printf("(%d, %d): F[%d]:=%d", process_index, level, Tjk, process_index+1);
						fflush(stdout);}
					    	}
				}

				if (j==J) { scan_map[j-1][2*k]=1; scan_map[j-1][2*k+1]=1; }
				else if(j>0)
				  {
					scan_map[j-1][2*k]=(Cjk0==-1)?0:1;
					scan_map[j-1][2*k+1]=(Cjk1==-1)?0:1;
					remote_accesses+=2;
				  }

				} /*for (k=0;k<pow(2, ((int)(log10(TOTAL_PROCESSES)/log10(2.0)-1-j)));k++)*/
			}/*for (j=J; j>=0; j--)*/

		/*For uniformity, the apex-lock winner's Freed_YA is being set to its own index*/
		Freed_YA=process_index+1;

		/*12.4.11: Request to write 'Number' of true tail_lowcost_MCS as -(process_index+1)*/
		message=3000+(process_index+1);
		ierr=MPI_Send( &message, 1, MPI_INT, tail_lowcost_MCS, NUMBER_TAG, MPI_COMM_WORLD);
		remote_accesses++;

		if (VERBOSE)
		{{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
		printf("(%d, %d): N[%d]:=%d", process_index, level, tail_lowcost_MCS, -(process_index+1));
		fflush(stdout);}

		/*16.5.11: Write own 'Number' as -(process_index+1). Just to be sure - however this is ripe for delete.*/
		Number=-(process_index+1);

		if (VERBOSE)
		{{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
		printf("(%d, %d): N[%d]:=%d", process_index, level, process_index, -(process_index+1));
		fflush(stdout);}

		MCS_queue_length++;
		tail_lowcost_MCS=process_index;

		MCS_queue_number++;

		/*12.4.11: Request to wake-up via 'Number' the process at head of MCS queue*/
		message=3000;
		ierr=MPI_Send( &message, 1, MPI_INT, head_lowcost_MCS, NUMBER_TAG, MPI_COMM_WORLD);
		remote_accesses++;

		if (VERBOSE)
		{{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
		printf("(%d, %d): Woken P%d", process_index, level, head_lowcost_MCS);
		fflush(stdout);}

		MCS_avg_queue_length=((MCS_times_count-1)*MCS_avg_queue_length+MCS_queue_length)/MCS_times_count;
		
		} /*else module of if ( level!=(log10(TOTAL_PROCESSES)/log10(2.0))-1)*/
	} /*end of if(!Freed_YA)*/

	if ( Freed_YA )
		{

		if (VERBOSE)
		{{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
		printf("(%d, %d): F[%d]==%d", process_index, level, process_index, Freed_YA);
		fflush(stdout);}

		if(VERBOSE) {{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
		printf("(%d, %d): SPIN(N<=0)", process_index, level);
		fflush(stdout);}

		while(Number<=0);

		if(VERBOSE) {{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
		printf("(%d, %d): Woken Up", process_index, level);
		fflush(stdout);}

		if(VERBOSE) {{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
		printf("(%d, %d): Enters C-S", process_index, level, lock_number(process_index, level));
		fflush(stdout);}

		/*12.8.17: Request-Response pair to read 'GLOBAL_COUNT' from listener (N-1), if you aren't node (N-1)*/
		if (process_index != TOTAL_PROCESSES-1)		
			{
			message=-process_index;
			ierr=MPI_Send( &message, 1, MPI_INT, TOTAL_PROCESSES-1, GLOBAL_COUNT_TAG, MPI_COMM_WORLD);
			MPI_Recv( &global_count, 1, MPI_INT, TOTAL_PROCESSES-1, GLOBAL_COUNT_TAG, MPI_COMM_WORLD, &status_thread);
			}
		else
			global_count=GLOBAL_COUNT;

		if(VERBOSE) {{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
		printf("(%d, %d): GLOBAL_COUNT==%d", process_index, level, global_count);
		fflush(stdout);}
		
		global_count++;

		/*12.8.17: Write 'global_count' using listener (N-1)'s services, provided this thread isn't at node (N-1)*/
		if (process_index != TOTAL_PROCESSES-1)		
			{
			message=global_count;
			ierr=MPI_Send( &message, 1, MPI_INT, TOTAL_PROCESSES-1, GLOBAL_COUNT_TAG, MPI_COMM_WORLD);
			}
		else
			GLOBAL_COUNT=global_count;

		if(VERBOSE) {{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
		printf("(%d, %d): GLOBAL_COUNT:=%d", process_index, level, global_count);
		fflush(stdout);}

		Freed_YA=0;
		}

	/*12.4.11: Request for lock-holder thread to re-set 'C' to -1. Guard against possibility of synchronization errors*/
	/*26.5.11: Probe if this is necessary. Reduce remote accesses*/

	message= 2*(process_index+1) + side(process_index, level)-1;
	ierr=MPI_Send( &message, 1, MPI_INT, lock_number(process_index, level), C_TAG, MPI_COMM_WORLD);
	remote_accesses+=1;
	
	if (VERBOSE)
	{{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
	printf("(%d, %d): reset C[%d]", process_index, level, lock_number(process_index, level));
	fflush(stdout);}

	message=-1000-process_index;
	ierr=MPI_Send( &message, 1, MPI_INT, lock_number(process_index, level), T_TAG, MPI_COMM_WORLD);
	MPI_Recv(&rival, 1, MPI_INT, lock_number(process_index, level), READ_T_TAG, MPI_COMM_WORLD, &status_thread);
	remote_accesses+=2;

	if (VERBOSE)
	{{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
	printf("(%d, %d): T[%d]==%d", process_index, level, lock_number(process_index, level), rival);
	fflush(stdout);}

	/*12.4.11: Request to write rival's 'P' as 2*/
	if (rival!=process_index && rival!=-1 )
		   { 
		     message=1500+(3*level+2);
		     ierr=MPI_Send( &message, 1, MPI_INT, rival, P_TAG, MPI_COMM_WORLD);
		     remote_accesses++;

		     if (VERBOSE)
		     {{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
		     printf("(%d, %d): P[%d]:=%d", process_index, level, rival, message-1500);
		     fflush(stdout);}
		   }

	if (!level)
		{

		if (abs(Number) != (process_index+1)) /*3.5.11: unnecessary use of abs(Number)*/
			{
			message=3000;
			/*12.4.11: Request to wake-up via reversing sign of 'Number' the process currently at head of MCS queue*/
			ierr=MPI_Send( &message, 1, MPI_INT, abs(Number)-1, NUMBER_TAG, MPI_COMM_WORLD);
			remote_accesses++;

			if (VERBOSE)
			{{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
			printf("(%d, %d): Woken P%d", process_index, level, abs(Number)-1);
			fflush(stdout);}
			}

		}

	return;
	}

void* Process(void *parameters)
{
	int i, j, k, l, rival, running_time, retries, message, ierr;
	int head_local;
	float start_time, end_time, inter_arrival_time, toss;
	long int GFLOPS;
	double gflops, gflops_j;

	float waiting_time_YA, waiting_time_MCS, waiting_time_total, past_creation_time;
	struct timeb scrap_precise_time;

	past_creation_time=0.0;
	inter_arrival_time=0.0;

	do
	{

	past_creation_time=creation_time;

	/*toss=(int)(ceil(REGENERATE_TIME*((float)(random()/(RAND_MAX+1.0)))));
	usleep(toss*1000);*/

	if (Number==0)
		{ 

		Number=-(process_index+1);
		/*21.4.11: will need to write Number using MPI routine*/

		ftime(&scrap_precise_time);
		system_time = (scrap_precise_time.time-baseline_precise_time.time)+(.001)*(scrap_precise_time.millitm-baseline_precise_time.millitm);

		toss=(int)(ceil(CS_TIME*((float)(random()/(RAND_MAX+1.0)))));

		i=process_index;
		start_time=(system_time > creation_time)? system_time: creation_time;
		flag.start_time=start_time;
		running_time=toss;

		if (VERBOSE)
		{{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
		printf("(%d,%d): N[%d]:=%d", process_index, -1, process_index, Number);
		fflush(stdout);}

		Number=-(process_index+1);	/*26.5.11: Unnecessary location but bug is changing value of Number*/
		YA_CS(i, 0, running_time); /*intended to be recursive and massively re-entrant, perhaps not in true spirit of Yang-Anderson*/

		process_count++;
		
		Number=0;

		ftime(&scrap_precise_time);
		system_time = (scrap_precise_time.time-baseline_precise_time.time)+(.001)*(scrap_precise_time.millitm-baseline_precise_time.millitm);
		end_time=(system_time > creation_time) ? system_time: creation_time;

		if (VERBOSE)
		{{ printf("\n"); for (l=0; l<process_index; l++) printf("\t"); }
		printf("P%d Done %dx @%0.2f", i, process_count, end_time);
		fflush(stdout);}

		ftime(&scrap_precise_time);
		waiting_time_total=((scrap_precise_time.time-baseline_precise_time.time)+(.001)*(scrap_precise_time.millitm-baseline_precise_time.millitm))-start_time;

		spartime=((process_count-1)*spartime+(waiting_time_total-0.001*running_time))/process_count;
		max_spartime=(max_spartime<waiting_time_total-0.001*running_time)? (waiting_time_total-0.001*running_time):max_spartime;

		ftime(&scrap_precise_time);
		flag.start_time=start_time;
		flag.end_time=(scrap_precise_time.time-baseline_precise_time.time)+(.001)*(scrap_precise_time.millitm-baseline_precise_time.millitm);

		}
	else 
		retries++;

	ftime(&scrap_precise_time);
	creation_time=(scrap_precise_time.time-baseline_precise_time.time)+(.001)*(scrap_precise_time.millitm-baseline_precise_time.millitm);
	inter_arrival_time=((process_count-1)*inter_arrival_time+(creation_time-past_creation_time))/process_count;

	}while(creation_time<=time_horizon);

	if (VERBOSE)
	{{ printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) { printf("\t"); fflush(stdout); } }
	printf("Thread-%d exits", i);
	fflush(stdout);}

	pthread_exit(&process_count);
}

