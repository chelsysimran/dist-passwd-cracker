/**
 * passwd.c
 *
 * Given a length and SHA-1 password hash, this program will use a brute-force
 * algorithm to recover the password using hash inversions.
 *
 * Compile: mpicc -g -Wall passwd.c -o passwd
 * Run: mpirun --oversubscribe -n 4 ./passwd num-chars hash [valid-chars]
 *
 * Where:
 *   - num-chars is the number of characters in the password
 *   - hash is the SHA-1 password hash (may not be in uppercase...)
 *   - valid-chars tells the program what character set to use (numeric, alpha,
 *     alphanumeric)
 */

//splitting things into a couple of different files

#include <ctype.h>
#include <mpi.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Provides SHA-1 functions: */
#include "sha1.c"

/* Defines the alphanumeric character set: */
char *alpha_num = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

/* Defines the alphabet character set: */
char *alpha = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

/* Defines the numeric character set: */
char *numeric = "0123456789";

/* Pointer to the current valid character set */
char *valid_chars;

/* When the password is found, we'll store it here: */
char found_pw[128] = { 0 };

int my_rank;
int comm_size;
unsigned long counter = 0;

/**
 * Generates passwords in order (brute-force) and checks them against a
 * specified target hash.
 * Inputs:
 *  - target - target hash to compare against
 *  - str - initial string. For the first function call, this can be "".
 *  - max_length - maximum length of the password string to generate
 */
bool crack(char *target, char *str, int max_length) 

{
    int curr_len = strlen(str);
    char *strcp = calloc(max_length + 1, sizeof(char));
    strcpy(strcp, str);
    int flag = 0;

    /* We'll copy the current string and then add the next character to it. So
     * if we start with 'a', then we'll append 'aa', and so on. */
    int i;
    for (i = 0; i < strlen(valid_chars); ++i) 
    {

        //printf("%s\n", "hi3");

    //call iProbe to check if the pword has been found (gets from send)
    MPI_Iprobe(MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &flag,  MPI_STATUS_IGNORE); 

    //     /*MPI_Iprobe(source, tag, comm, flag, status) returns flag = true if there is a message that can be 
    //     received and that matches the pattern specified by the arguments source, tag, and comm. The call matches 
    //     the same message that would have been received by a call to MPI_Recv(..., source, tag, comm, status) executed 
    //     at the same point in the program, and returns in status the same value that would have been returned by MPI_Recv(). 
    //     Otherwise, the call returns flag = false, and leaves status undefined.*/
        
         if (flag) 

        {
            MPI_Recv(found_pw, 128, MPI_CHAR, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            // if (strlen(found_pw) > 0) 
            // {
            //     printf("Recovered password: %s\n", found_pw);
            //     printf("%s\n%d\n", valid_chars, comm_size); //valid charaters numprocs
            // }
        }

        strcp[curr_len] = valid_chars[i];
        
        if (curr_len + 1 < max_length) 
        {
            bool found = crack(target, strcp, max_length);
            
            //printf("%d\n", counter); //printing counter to check whether its getting to this partttt
            
            if (found == true) 
            {
                return true;
            }
        } 

        else 
        
        {
            /* Only check the hash if our string is long enough */
            char hash[41];
            sha1sum(hash, strcp);
            /* TODO: This prints way too often... */
            counter++;
            if (counter % 1000000 == 0) //it only prints every one million combinations
            {
                printf("%s -> %s\n", strcp, hash);
            }
            
            if (strcmp(hash, target) == 0) 

            {
                /* We found a match! */
                strcpy(found_pw, strcp);

                int i;

                for (i=0; i < comm_size; ++i)
                {
                    if( i != my_rank)
                    {
                         MPI_Send(found_pw, 128, MPI_CHAR, i, 0, MPI_COMM_WORLD);
                    }
                }
                
                return true;
            }
        }
    }

    free(strcp);
    return false;
}
/**
 * Modifies a string to only contain uppercase characters.
 */
void uppercase(char *string) 

    {
        int i;
        for (i = 0; string[i] != '\0'; i++) 
        {
            string[i] = toupper(string[i]);
        }
    }

int main(int argc, char *argv[]) 

{

    MPI_Init(&argc, &argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
    
    if (argc < 3 || argc > 4) 

    {
        printf("Usage: mpirun %s num-chars hash [valid-chars]\n", argv[0]);
        printf("  Options for valid-chars: numeric, alpha, alphanum\n");
        printf("  (defaults to 'alphanum')\n");
        return 0;
    }

    //STEP 1: DO SOME ARGUMENT CHECKING

    /* TODO: We need some sanity checking here... */
    int length = atoi(argv[1]);
    char *target = argv[2];

    valid_chars = alpha_num; // sets the default = alpha_num

     //2 cases, if the argument passed by the user is 'alpha', this re-sets the value of valid chars to that
    
    if (argc >= 4)

    {

        if (strcmp(argv[3], "alpha") == 0)
        {
            valid_chars = alpha;
        }

        if (strcmp(argv[3], "numeric") == 0)
        {
            valid_chars = numeric;
        }

    }

    int rangeOfRank;
    rangeOfRank = strlen(valid_chars)/comm_size;

    int remainder;
    remainder = strlen(valid_chars)%comm_size;


    //takes care of the "leftover" if the valid chars doesn't divide evenly by the number of processors
    //if we are at the last rank, then the remaining characters should be appended to the last rank
    if (my_rank == comm_size - 1)
    {
        rangeOfRank += remainder;
    }

    /* TODO: Print out job information here (valid characters, number of
     * processes, etc). */ //you dont want this part parallelized
    if (my_rank == 0)
    {
        printf("%s\n", "Starting parallel password cracker." );
        printf("%s%d\n", "Number of Processes: ", comm_size);
        printf("%s%s\n", "Valid characters: ", valid_chars);
        printf("%s%d\n", "Target password length: ", length);

    }

    /*TODO: the 'crack' call above starts with a blank string, and then
     * proceeds to add characters one by one, in order. To parallelize this, we
     * need to make each process start with a specific character. Kind of like
     * the following:*/

    //splitting up the workload here, and dividing each to the different processes
    //split the performance workload here in order to make sure there are multiple cores 
    //the main function is where you have to parallelize the all the inner workings

    //getting the time for each run

    int i = 0;
    int start = my_rank * rangeOfRank;
    int end = start + rangeOfRank;

    for (i = start; i < end; ++i) 
    {
        //printf("%s\n", "hi"); //printing check
        char start_str[2] = {valid_chars[i], '\0'};
        bool found = crack(target, start_str, length);

        if (found)
        break;
        //printf("%s\n", "hi2"); //printing check
    } 

    double time_1 = MPI_Wtime();
    long total;
    long totalTime;
    MPI_Reduce(&totalTime, &total, 1, MPI_UNSIGNED_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    double time_2 = MPI_Wtime();

    if(my_rank == 0)
    
    {
        
        printf("%s%s\n", "Target hash: ", target);
        printf("%s\n", "Operation Complete!");
        printf("%s%ld\n", "Total number of passwords hashed: ", total);
        printf("%s%f\n", "Time elapsed: ", time_2 - time_1);
        printf("Recovered password: %s\n", found_pw);

    }


    // if (strlen(found_pw) > 0) 
    // {
        
    // }

    MPI_Finalize();
    return 0;
}
