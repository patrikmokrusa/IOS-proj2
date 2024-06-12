#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <time.h>

int *kapacita; ///< kolik je lyzaru v busu
int *cekaji;	///< kolik celkove ceka (kvuli loopovani autobusu)
int *line_counter; ///< pro pocitani radku a nasledneho vypsani

int *zastavka; ///< pole zastavek, ve kterych je pocet lyzaru na danem indexu (index == cislo zastavky)
int *current_zastavka; ///< na ktere zastavce je prave bus

sem_t *all_started; ///< bus ceka az se forknou vsichni deti
sem_t *boarding; ///< kdy je mozne nastoupit do busu
sem_t *final; ///< lyzar nastoupi do budu a da wait na final a ceka az mu ho bus uvolni

sem_t *print_buffer; ///< aby se printovalo postupne

/**
 * @brief random generator od 1 do urciteho cisla
 * @return pseudo nahodne cislo
*/
int random_gen(int to){
	srand(time(0) + *line_counter);
	return (rand() % to + 1);
}


int main(int argc, char* argv[]) {
	if (argc != 6) ///< zpracovani argumentu
	{
		printf("5 args needed\n");
		return 1;
	}
	for (int i = 1; i < argc; i++)
	{
		if (atoi(argv[i]) <= 0)
		{
			printf("Bad arg: %s\n", argv[i]);
			return 1;
		}
		
	}

	FILE *file;
	file = fopen("proj2.out", "w");
	setbuf(file, NULL);
	
	int sleep_lyzar = atoi(argv[4]); ///<	TL	spozdeni pro lyzare
	int sleep_bus = atoi(argv[5]); ///<	TB	spozdeni pro bus
	int pocet_lyzaru = atoi(argv[1]); ///<	L	pocet lyzaru
	int pocet_zastavek = atoi(argv[2]); ///<	Z	pocet zastavek
	int max_kapacita = atoi(argv[3]); ///<	K	max kapacita busu
	pid_t pids[pocet_lyzaru]; ///< k vytvoreni vice child procesu

	

	
	kapacita = mmap(NULL, sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	cekaji = mmap(NULL, sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	line_counter = mmap(NULL, sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	all_started = mmap(NULL, sizeof(sem_t), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	boarding = mmap(NULL, sizeof(sem_t), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);	
	final = mmap(NULL, sizeof(sem_t), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

	print_buffer = mmap(NULL, sizeof(sem_t), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);

	if ((kapacita == MAP_FAILED)||(cekaji==MAP_FAILED)||(line_counter==MAP_FAILED)||(all_started==MAP_FAILED)||(boarding==MAP_FAILED)||(final==MAP_FAILED)) {
		printf("mmap faild\n");
		exit(1);
	}
	zastavka = mmap(NULL, sizeof(int)*(pocet_zastavek +1), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	current_zastavka = mmap(NULL, sizeof(int), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
	
	
	*current_zastavka = 1; ///< zastavka, kde se prave nachazi bus
	*cekaji = pocet_lyzaru; ///< kolik lizaru celkove ceka na vsech zastavkach
	*kapacita = 0; ///< kolik je v busu lyzaru
	*line_counter = 0; ///< pocitadlo radku
	
	int res_bus;
	sem_init(print_buffer, 1, 1);
	sem_init(all_started, 1, 1);
	sem_init(boarding, 1, 1);
	sem_init(final, 1, 1);

	sem_wait(all_started);
	res_bus = fork();
	if (res_bus < 0) { 
		printf("fork error\n");
		return 1;
		}
	else if (res_bus == 0) ///< proces BUS
	{
		sem_wait(boarding);
		sem_wait(final);

		sem_wait(all_started);

		sem_wait(print_buffer);
		*line_counter = *line_counter + 1;
		fprintf(file, "%d: BUS: started\n",*line_counter);
		sem_post(print_buffer);

		sem_post(all_started);
		while (*cekaji > 0) {

			for (int i = 1; i <= pocet_zastavek; i++)
			{
				usleep(random_gen(sleep_bus)); 

				*current_zastavka = i;

				sem_wait(print_buffer);
				*line_counter = *line_counter + 1;
				fprintf(file, "%d: BUS: arrived to %d\n", *line_counter, i);
				sem_post(print_buffer);

				sem_post(boarding);

				while (zastavka[i] > 0) {
					if (*kapacita >= max_kapacita)
					{
						break;
					}
					
				} ///< ceka az vsichni na zastavce nastoupi
				
				sem_wait(boarding);
				
				sem_wait(print_buffer);
				*line_counter = *line_counter + 1;
				fprintf(file, "%d: BUS: leaving %d\n", *line_counter, i);
				sem_post(print_buffer);
			}
			
			
			usleep(random_gen(sleep_bus));

			sem_wait(print_buffer);
			*line_counter = *line_counter + 1;
			fprintf(file, "%d: BUS: arrived to final\n", *line_counter);
			sem_post(print_buffer);

			sem_post(final);

			while (*kapacita > 0) {} ///< ceka az vsichni vystoupi
			
			
			sem_wait(final);

			sem_wait(print_buffer);
			*line_counter = *line_counter + 1;
			fprintf(file, "%d: BUS: leaving final\n", *line_counter);
			sem_post(print_buffer);
		}
		
		sem_wait(print_buffer);
		*line_counter = *line_counter + 1;
		fprintf(file, "%d: BUS: finish\n", *line_counter);
		sem_post(print_buffer);

		exit(0);
	}

	for (int i = 1; i <= pocet_lyzaru; ++i) { ///< procesy lyzaru
		if ((pids[i] = fork()) < 0) {
			printf("fork error\n");
			return 1;
		} else if (pids[i] == 0) {
			sem_wait(print_buffer);
			*line_counter = *line_counter + 1;
			fprintf(file, "%d: L %d: started\n", *line_counter, i);
			sem_post(print_buffer);

			usleep(random_gen(sleep_lyzar));

			int random_zastavka = (random_gen(pocet_zastavek));
			zastavka[random_zastavka] = zastavka[random_zastavka] + 1;

			sem_wait(print_buffer);
			*line_counter = *line_counter + 1;
			fprintf(file, "%d: L %d: arrived to %d\n", *line_counter , i, random_zastavka);
			sem_post(print_buffer);

			while (1) ///< vyhybani se jumpu
			{
				while ((*current_zastavka != random_zastavka) || (*kapacita >= max_kapacita)) {} ///< ceka az bude current_zastavka prave ta jeho
				
				sem_wait(boarding);
				if (*kapacita >= max_kapacita) ///< kontrola kapacity
				{
					sem_post(boarding);
				}
				else {
					*kapacita = *kapacita + 1;

					sem_wait(print_buffer);
					fprintf(file, "%d: L %d: boarding\n",*line_counter ,i);
					zastavka[random_zastavka] = zastavka[random_zastavka] - 1;
					*line_counter = *line_counter + 1;
					sem_post(print_buffer);

					sem_post(boarding);
					break;
				}
			}
			

			sem_wait(final);
			*line_counter = *line_counter + 1;
			fprintf(file, "%d: L %d: going to ski\n", *line_counter, i);
			*cekaji = *cekaji - 1;
			*kapacita = *kapacita - 1;
			sem_post(final);

			
			exit(0);
		}
	}
	sem_post(all_started);

	while(wait(NULL) > 0) {};

	fclose(file);

	sem_destroy(all_started);
	sem_destroy(boarding);
	sem_destroy(final);
	sem_destroy(print_buffer);
	munmap(all_started,sizeof(sem_t));
	munmap(boarding,sizeof(sem_t));
	munmap(final,sizeof(sem_t));
	munmap(print_buffer,sizeof(sem_t));
	munmap(kapacita,sizeof(int));
	munmap(cekaji,sizeof(int));
	munmap(line_counter,sizeof(int));

	return 0;
}