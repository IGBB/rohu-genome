#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define MAXITER 100000

typedef struct {
    char name [256];
    int gender;
} sample_t;


FILE * safe_open(char * fn, char* flags){
    FILE * file = fopen(fn, flags);
    if (file==NULL) {fputs ("File error",stderr); exit (1);}
    return file;
}

/* get abs mean diff */
inline float mean_diff(float reads[], int gender[], int len[]){
    float sum[2] = {0.0, 0.0};
    int total = len[0] + len[1];
    int i;

    /* sum reads based on gender  */
    for(i = 0; i < total; i++){
        sum[gender[i]] += reads[i];
    }

    /* Get absolute value of mean difference */
    float stat = sum[1]/(float)len[1] - sum[0]/(float)len[0];
    stat = fabs(stat);

    return stat;
}

int main(int argc, char *argv[]) {

    FILE * file;
    char * line;
    size_t len = 0;
    ssize_t nread;

    sample_t * samples = NULL;
    int num_samples =0;

    /* Read pheno from first cmd arg. Expected 3 columns similar to plink:
     * Family ID\tIndv ID\tGender */
    file = safe_open(argv[1], "r");
    while ((nread = getline(&line, &len, file)) != -1) {
        char FID[256], IID[256], gender[256];
        int index = num_samples;

        sscanf(line, "%s\t%s\t%s", FID, IID, gender);

        /* Skip header */
        if(strcmp(FID, "FID") == 0)
            continue;

        /* add space to sample array */
        num_samples++;
        samples = realloc(samples, num_samples*sizeof(sample_t));

        /* Convert Male/Female to index for use later  */
        strncpy(samples[index].name, IID, 256);
        if(strcmp(gender, "Male") == 0)
            samples[index].gender = 1;
        else
            samples[index].gender = 0;
    }
    fclose(file);



    file = safe_open(argv[2], "r");

    /* Read header from normalized read matrix to get sample order and fill
     * gender index array */
    int * gender = calloc(num_samples, sizeof(int));
    int count[2] = {0,0};

    if ((nread = getline(&line, &len, file)) != -1) {
        char * name = strtok(line, "\t");

        /* Loop through samples, finding the gender from the pheno file. Die if
         * can't find name in pheno data. Tally number of sample per gender */
        int i = 0;
        while (( name = strtok(NULL, "\t\n")) != NULL){
            int j ;
            for(j = 0; j < num_samples; j++){
                if(strcmp(samples[j].name, name) == 0){
                    gender[i] = samples[j].gender;
                    break;
                }
            }

            if(j == num_samples){
                fprintf(stderr, "Didnt' find sample: \"%s\"\n", name);
                exit(1);
            }

            count[gender[i]]++;
            i++;
        }
        num_samples = i;

    }


    /* Select gender permutations for each iteration before loop. Calloc
     * mass_gender to hold all indices and init to 0. Build convenience array
     * perm_gender to point to the start of each permutation. Randomly toggle
     * index until split is equal to input. */
    srand((int)time(NULL));
    int * mass_gender = calloc(num_samples*MAXITER, sizeof(float));
    int * perm_gender [MAXITER];
    int i, j;
    for( i = 0; i < MAXITER; i++){
        perm_gender[i] = &(mass_gender[i*num_samples]);

            for(j=0; j< count[1]; j++){
                int n;
                do{
                 n = rand() % num_samples;
                }while(perm_gender[i][n] == 1);
                perm_gender[i][n] = 1;
            }
    }

    /* Read each line of normalized read count matrix.  */
    while ((nread = getline(&line, &len, file)) != -1) {
        char  * frag  = strtok(line, "\t"), *data;
        float * reads = malloc(num_samples * sizeof(float));

        /* Read columns for current line and fille reads array */
        int i = 0;
        while (( data = strtok(NULL, "\t")) != NULL){
            reads[i] = atof(data);
            i++;
        }

        /* Get observed mean diff */
        float stat = mean_diff(reads, gender, count);

        int count = 0;
        int iter;
        /* Compare randomly assigned mean diff to ovserved for all iterations */
        for(iter = 0; iter < MAXITER; iter++){
            int * new_gender = perm_gender[iter];

            float new_stat = mean_diff(reads, new_gender, count);

            count += (new_stat > stat);

        }

        printf("%s\t%f\t%d\t%f\n", frag, stat, count, ((float) count)/MAXITER);
    }
    fclose(file);

    return 0;
}
